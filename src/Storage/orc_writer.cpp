#include "Storage/orc_writer.h"
#include "Storage/row_size_estimates.h"

#include <algorithm>
#include <arrow/io/buffered.h>
#include <limits>

arrow::Result<FileSystemPtr> OrcTableWriter::GetFilesystem(std::string_view target_uri)
{
    return ResolveTarget(target_uri);
}

std::int64_t OrcTableWriter::EstimateOrcRowBytes(std::string_view table_name)
{
    return EstimateTpchRowBytes(table_name);
}

std::int64_t OrcTableWriter::ResolveStripeRows(const TableMetadata & table, const OrcWriterOptions & options)
{
    if (options.max_stripe_rows > 0)
    {
        return options.max_stripe_rows;
    }
    if (options.stripe_bytes <= 0)
    {
        return 0;
    }

    const auto avg_row_bytes = EstimateOrcRowBytes(table.name);
    if (avg_row_bytes <= 0)
    {
        return 0;
    }
    return std::max<std::int64_t>(1, options.stripe_bytes / avg_row_bytes);
}

std::int64_t OrcTableWriter::ResolveStripeBytes(const TableMetadata & table, const OrcWriterOptions & options)
{
    const auto avg_row_bytes = EstimateOrcRowBytes(table.name);

    if (options.max_stripe_rows > 0)
    {
        if (avg_row_bytes > 0)
        {
            const auto max_rows = options.max_stripe_rows;
            if (max_rows > std::numeric_limits<std::int64_t>::max() / avg_row_bytes)
            {
                return std::numeric_limits<std::int64_t>::max();
            }
            return std::max<std::int64_t>(1, max_rows * avg_row_bytes);
        }
        if (options.stripe_bytes > 0)
        {
            return options.stripe_bytes;
        }
        return 0;
    }

    if (options.stripe_bytes > 0)
    {
        return options.stripe_bytes;
    }

    const auto rows = ResolveStripeRows(table, options);
    if (rows <= 0)
    {
        return 0;
    }

    if (avg_row_bytes <= 0)
    {
        return 0;
    }
    return std::max<std::int64_t>(1, rows * avg_row_bytes);
}

const OrcWriterOptions & OrcTableWriter::ResolveOptions(const WriterOptions & options)
{
    static const OrcWriterOptions defaults{};

    if (!options.format_options)
    {
        return defaults;
    }

    auto typed_options = std::dynamic_pointer_cast<const OrcWriterOptions>(options.format_options);
    if (!typed_options)
    {
        return defaults;
    }
    return *typed_options;
}

std::string OrcTableWriter::BuildPartitionFileName(const std::string & table_name, const PartitionSpec & partition)
{
    if (partition.part_count <= 1)
    {
        return table_name + "-1.orc";
    }
    return table_name + "-part-" + std::to_string(partition.part_num) + "-of-" + std::to_string(partition.part_count) + ".orc";
}

arrow::Result<std::string> OrcTableWriter::GetPath(std::string_view uri, const arrow::fs::FileSystem & fs)
{
    if (IsObsUri(uri))
    {
        return arrow::Status::NotImplemented("OBS filesystem is not implemented yet");
    }

    if (HasUriScheme(uri))
    {
        return fs.PathFromUri(std::string(uri));
    }
    return std::string(uri);
}

arrow::Status OrcTableWriter::OpenPartition(
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & options,
    const PartitionSpec & partition,
    arrow::MemoryPool * pool)
{
#if !defined(ARROW_ORC)
    (void)table;
    (void)output;
    (void)options;
    (void)partition;
    (void)pool;
    return arrow::Status::NotImplemented("Arrow was built without ORC support");
#else
    if (is_open_)
    {
        return arrow::Status::Invalid("OrcTableWriter::OpenPartition() called while partition is already open");
    }
    if (partition.part_num < 1 || partition.part_count < 1 || partition.part_num > partition.part_count)
    {
        return arrow::Status::Invalid("Invalid partition range");
    }

    table_ = &table;
    pool_ = pool != nullptr ? pool : arrow::default_memory_pool();
    options_ = ResolveOptions(options);

    ARROW_ASSIGN_OR_RAISE(fs_, GetFilesystem(output.uri));
    ARROW_ASSIGN_OR_RAISE(const std::string base_dir, GetPath(output.uri, *fs_));

    const auto table_dir = JoinPath(base_dir, table.name);
    RETURN_NOT_OK(fs_->CreateDir(table_dir, /*recursive=*/true));

    final_path_ = JoinPath(table_dir, BuildPartitionFileName(table.name, partition));
    temp_path_ = final_path_ + ".tmp";

    ARROW_ASSIGN_OR_RAISE(const auto info, fs_->GetFileInfo(final_path_));
    if (info.type() != arrow::fs::FileType::NotFound)
    {
        return arrow::Status::AlreadyExists(final_path_);
    }

    ARROW_ASSIGN_OR_RAISE(sink_, fs_->OpenOutputStream(temp_path_));
    if (options_.output_buffer_bytes > 0)
    {
        ARROW_ASSIGN_OR_RAISE(
            sink_,
            arrow::io::BufferedOutputStream::Create(options_.output_buffer_bytes, pool_, std::move(sink_)));
    }

    arrow::adapters::orc::WriteOptions write_options;
    const auto stripe_bytes = ResolveStripeBytes(*table_, options_);
    if (stripe_bytes > 0)
    {
        write_options.stripe_size = stripe_bytes;
    }
    write_options.compression = options_.compression;

    ARROW_ASSIGN_OR_RAISE(writer_, arrow::adapters::orc::ORCFileWriter::Open(sink_.get(), write_options));
    is_open_ = true;
    return arrow::Status::OK();
#endif
}

arrow::Status OrcTableWriter::WriteBatch(const TableBatch & batch)
{
#if !defined(ARROW_ORC)
    (void)batch;
    return arrow::Status::NotImplemented("Arrow was built without ORC support");
#else
    if (!is_open_ || !writer_ || table_ == nullptr)
    {
        return arrow::Status::Invalid("OrcTableWriter::WriteBatch() called before OpenPartition()");
    }
    if (batch.metadata == nullptr || batch.metadata != table_)
    {
        return arrow::Status::Invalid("TableBatch metadata mismatch");
    }
    if (batch.row_count == 0)
    {
        return arrow::Status::OK();
    }

    const auto row_count = static_cast<std::int64_t>(batch.row_count);
    auto rb = arrow::RecordBatch::Make(table_->schema, row_count, batch.columns);
    return writer_->Write(*rb);
#endif
}

arrow::Status OrcTableWriter::ClosePartition()
{
#if !defined(ARROW_ORC)
    return arrow::Status::OK();
#else
    if (!is_open_)
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

    if (!temp_path_.empty() && !final_path_.empty())
    {
        RETURN_NOT_OK(fs_->Move(temp_path_, final_path_));
    }

    table_ = nullptr;
    pool_ = nullptr;
    fs_.reset();
    temp_path_.clear();
    final_path_.clear();
    is_open_ = false;
    return arrow::Status::OK();
#endif
}

std::string OrcTableWriter::JoinPath(const std::string & base, const std::string & leaf)
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
