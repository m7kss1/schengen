#pragma once

#include "storage.h"

#include "table.h"

#include <arrow/api.h>
#include <arrow/io/interfaces.h>
#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/util/config.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#if defined(ARROW_PARQUET)
#    include <parquet/arrow/writer.h>
#    include <parquet/properties.h>
#endif

enum class OutputFormat : uint8_t
{
    Parquet,
    /* TODO: Orc and Lance */
};

struct OutputLocation
{
    /* 
     * Output directory URI/path
     * Examples:
     *  - "file:///tmp/schengen"
     *  - "s3://bucket/prefix" (TODO)
     *  - "obs://bucket/prefix" (TODO)
     */
    std::string uri;
};

/* 
 * By default, we already write one file per partition in parallel
 * No need to use internal parquet writer threads since it may cause deadlock [1]
 * Keep it off by default or enable explicitly if needed
 * 
 * [1]: Arrow comments for WriteRecordBatch:
 * If you are writing multiple files in parallel in the same
 * executor, deadlock may occur if ArrowWriterProperties::use_threads
 * is set to true to write columns in parallel. Please disable use_threads
 * option in this case.
 */
struct ParquetWriterOptions
{
    /* Control parallelism for single row group */
    bool use_threads = false;
    std::int64_t max_row_group_rows = 128 * 1024;
    /*
     * TODO: Possible perfomance improvement. Wrap filesystem output stream into
     * large buffered stream (e.g. 32MB) to reduces overhead of 
     * many small writes emitted by writer. Will gain some perf for 'costy' S3 writes
     */
#if defined(ARROW_PARQUET)
#    if defined(ARROW_WITH_SNAPPY)
    ::parquet::Compression::type compression = ::parquet::Compression::SNAPPY;
#    else
    ::parquet::Compression::type compression = ::parquet::Compression::UNCOMPRESSED;
#    endif
#else
    int compression = 0;
#endif
};

struct WriterOptions
{
    OutputFormat format = OutputFormat::Parquet;
    ParquetWriterOptions parquet{};
};

class ITableWriter
{
public:
    virtual ~ITableWriter() = default;

    virtual arrow::Status Open(
        const TableMetadata & table,
        const OutputLocation & output,
        std::int32_t part_num,
        const WriterOptions & options,
        arrow::MemoryPool * pool)
        = 0;

    virtual arrow::Status BeginRowGroup() = 0;

    virtual arrow::Status WriteBatch(const TableBatch & batch) = 0;

    virtual arrow::Status Close() = 0;
};

class ParquetTableWriter final : public ITableWriter
{
public:
    static arrow::Result<FileSystemPtr> GetFilesystem(std::string_view target_uri) { return ResolveTarget(target_uri); }

    static arrow::Result<std::string> GetPath(std::string_view uri, const arrow::fs::FileSystem & fs)
    {
        if (IsS3Uri(uri) || IsObsUri(uri))
        {
            return arrow::Status::NotImplemented("S3/OBS filesystem is not implemented yet");
        }

        if (HasUriScheme(uri))
        {
            return fs.PathFromUri(std::string(uri));
        }
        return std::string(uri);
    }

    arrow::Status Open(
        const TableMetadata & table,
        const OutputLocation & output,
        std::int32_t part_num,
        const WriterOptions & options,
        arrow::MemoryPool * pool) override
    {
#if !defined(ARROW_PARQUET)
        (void)table;
        (void)output;
        (void)part_num;
        (void)options;
        (void)pool;
        return arrow::Status::NotImplemented("Arrow was built without Parquet support");
#else
        if (part_num < 1)
        {
            return arrow::Status::Invalid("Invalid partition number");
        }

        table_ = &table;
        pool_ = pool != nullptr ? pool : arrow::default_memory_pool();
        options_ = options.parquet;

        ARROW_ASSIGN_OR_RAISE(fs_, GetFilesystem(output.uri));
        ARROW_ASSIGN_OR_RAISE(const std::string base_dir, GetPath(output.uri, *fs_));

        /* 
         * Default layout:
         *  <out>/<table>/<table>-<part>.parquet
         */
        const auto table_dir = JoinPath(base_dir, table.name);
        RETURN_NOT_OK(fs_->CreateDir(table_dir, /*recursive=*/true));

        final_path_ = JoinPath(table_dir, table.name + "-" + std::to_string(part_num) + ".parquet");
        temp_path_ = final_path_ + ".tmp";

        ARROW_ASSIGN_OR_RAISE(const auto info, fs_->GetFileInfo(final_path_));
        if (info.type() != arrow::fs::FileType::NotFound)
        {
            return arrow::Status::AlreadyExists(final_path_);
        }

        ARROW_ASSIGN_OR_RAISE(sink_, fs_->OpenOutputStream(temp_path_));

        auto props_builder = ::parquet::WriterProperties::Builder();
#    if !defined(ARROW_WITH_SNAPPY)
        if (options_.compression == ::parquet::Compression::SNAPPY)
        {
            options_.compression = ::parquet::Compression::UNCOMPRESSED;
        }
#    endif
        props_builder.compression(options_.compression);
        if (options_.max_row_group_rows > 0)
        {
            props_builder.max_row_group_length(options_.max_row_group_rows);
        }
        auto props = props_builder.build();

        auto arrow_props = ::parquet::ArrowWriterProperties::Builder().set_use_threads(options_.use_threads)->build();

        ARROW_ASSIGN_OR_RAISE(
            writer_, ::parquet::arrow::FileWriter::Open(*table.schema, pool_, sink_, std::move(props), std::move(arrow_props)));

        row_group_open_ = false;
        return arrow::Status::OK();
#endif
    }

    arrow::Status BeginRowGroup() override
    {
#if !defined(ARROW_PARQUET)
        return arrow::Status::NotImplemented("Arrow was built without Parquet support");
#else
        if (!writer_)
        {
            return arrow::Status::Invalid("Row group started before file created");
        }
        const auto status = writer_->NewBufferedRowGroup();
        if (status.ok())
        {
            row_group_open_ = true;
        }
        return status;
#endif
    }

    arrow::Status WriteBatch(const TableBatch & batch) override
    {
#if !defined(ARROW_PARQUET)
        (void)batch;
        return arrow::Status::NotImplemented("Arrow was built without Parquet support");
#else
        if (!writer_ || table_ == nullptr)
        {
            return arrow::Status::Invalid("ParquetTableWriter::WriteBatch() called before Open()");
        }
        if (batch.metadata == nullptr || batch.metadata != table_)
        {
            return arrow::Status::Invalid("TableBatch metadata mismatch");
        }
        if (batch.row_count == 0)
        {
            return arrow::Status::OK();
        }
        if (!row_group_open_)
        {
            RETURN_NOT_OK(BeginRowGroup());
        }

        const auto row_count = static_cast<std::int64_t>(batch.row_count);
        auto rb = arrow::RecordBatch::Make(table_->schema, row_count, batch.columns);
        return writer_->WriteRecordBatch(*rb);
#endif
    }

    arrow::Status Close() override
    {
#if !defined(ARROW_PARQUET)
        return arrow::Status::OK();
#else
        if (!writer_)
        {
            return arrow::Status::OK();
        }
        RETURN_NOT_OK(writer_->Close());
        writer_.reset();
        if (sink_)
        {
            RETURN_NOT_OK(sink_->Close());
            sink_.reset();
        }

        /* Publish the file only after a successful write */
        if (!temp_path_.empty() && !final_path_.empty())
        {
            RETURN_NOT_OK(fs_->Move(temp_path_, final_path_));
        }

        table_ = nullptr;
        row_group_open_ = false;
        return arrow::Status::OK();
#endif
    }

private:
    static std::string JoinPath(const std::string & base, const std::string & leaf)
    {
        if (base.empty())
        {
            return leaf;
        }
        if (leaf.empty())
        {
            return base;
        }
        if (base.back() == '/')
        {
            return base + leaf;
        }
        return base + "/" + leaf;
    }

    const TableMetadata * table_ = nullptr;
    arrow::MemoryPool * pool_ = nullptr;
    ParquetWriterOptions options_{};

#if defined(ARROW_PARQUET)
    FileSystemPtr fs_;
    std::string temp_path_;
    std::string final_path_;
    std::shared_ptr<arrow::io::OutputStream> sink_;
    std::unique_ptr<::parquet::arrow::FileWriter> writer_;
#endif

    bool row_group_open_ = false;
};

inline std::unique_ptr<ITableWriter> MakeTableWriter(OutputFormat format)
{
    switch (format)
    {
        case OutputFormat::Parquet:
            return std::make_unique<ParquetTableWriter>();
        default:
            return nullptr;
    }
}
