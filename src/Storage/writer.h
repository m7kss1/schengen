#pragma once

#include "Storage/storage.h"
#include "Tables/table.h"

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
    /* TODO: Orc, Lance, Iceberg, Paimon */
};

struct OutputLocation
{
    /*
     * Output directory URI/path
     * Examples:
     *  - "file:///tmp/schengen"
     *  - "s3://bucket/prefix"
     *  - "obs://bucket/prefix" (TODO)
     */
    std::string uri;
};

/*
 * Arrow comments for WriteRecordBatch:
 * If you are writing multiple files in parallel in the same
 * executor, deadlock may occur if ArrowWriterProperties::use_threads
 * is set to true to write columns in parallel. Please disable use_threads
 * option in this case.
 */
struct ParquetWriterOptions
{
    /* Control parallelism for single row group */
    bool use_threads = false;
    /* Target row group size in bytes. Used to estimate rows per group */
    std::int64_t row_group_bytes = 7 * 1024 * 1024;
    /* Override row group length in rows (0 = auto from row_group_bytes) */
    std::int64_t max_row_group_rows = 0;
    /* Wrap filesystem output stream into a buffered stream to reduce small write overhead (set <= 0 to disable) */
    std::int64_t output_buffer_bytes = 21 * 1024 * 1024;
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
    static arrow::Result<FileSystemPtr> GetFilesystem(std::string_view target_uri);
    static std::int64_t EstimateParquetRowBytes(std::string_view table_name);
    static std::int64_t ResolveRowGroupRows(const TableMetadata & table, const ParquetWriterOptions & options);
    static arrow::Result<std::string> GetPath(std::string_view uri, const arrow::fs::FileSystem & fs);

    arrow::Status Open(
        const TableMetadata & table,
        const OutputLocation & output,
        std::int32_t part_num,
        const WriterOptions & options,
        arrow::MemoryPool * pool) override;

    arrow::Status BeginRowGroup() override;
    arrow::Status WriteBatch(const TableBatch & batch) override;
    arrow::Status Close() override;

private:
    static std::string JoinPath(const std::string & base, const std::string & leaf);

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

std::unique_ptr<ITableWriter> MakeTableWriter(OutputFormat format);
