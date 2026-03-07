#include "Storage/parquet_writer.h"

#include <algorithm>
#include <arrow/io/buffered.h>

arrow::Result<FileSystemPtr> ParquetTableWriter::GetFilesystem(std::string_view target_uri)
{
    return ResolveTarget(target_uri);
}

std::int64_t ParquetTableWriter::EstimateParquetRowBytes(std::string_view table_name)
{
    if (table_name == "nation")
    {
        return 117;
    }
    if (table_name == "region")
    {
        return 151;
    }
    if (table_name == "part")
    {
        return 70;
    }
    if (table_name == "supplier")
    {
        return 164;
    }
    if (table_name == "partsupp")
    {
        return 141 * 4;
    }
    if (table_name == "customer")
    {
        return 168;
    }
    if (table_name == "orders")
    {
        return 75;
    }
    if (table_name == "lineitem")
    {
        return 64;
    }
    return 0;
}

std::int64_t ParquetTableWriter::ResolveRowGroupRows(const TableMetadata & table, const ParquetWriterOptions & options)
{
    if (options.max_row_group_rows > 0)
    {
        return options.max_row_group_rows;
    }
    if (options.row_group_bytes <= 0)
    {
        return 0;
    }
    const auto avg_row_bytes = EstimateParquetRowBytes(table.name);
    if (avg_row_bytes <= 0)
    {
        return 0;
    }
    return std::max<std::int64_t>(1, options.row_group_bytes / avg_row_bytes);
}

const ParquetWriterOptions & ParquetTableWriter::ResolveOptions(const WriterOptions & options)
{
    static const ParquetWriterOptions defaults{};

    if (!options.format_options)
    {
        return defaults;
    }

    auto typed_options = std::dynamic_pointer_cast<const ParquetWriterOptions>(options.format_options);
    if (!typed_options)
    {
        return defaults;
    }
    return *typed_options;
}

arrow::Result<std::string> ParquetTableWriter::GetPath(std::string_view uri, const arrow::fs::FileSystem & fs)
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

std::string ParquetTableWriter::BuildPartitionFileName(const std::string & table_name, const PartitionSpec & partition)
{
    if (partition.part_count <= 1)
    {
        return table_name + "-1.parquet";
    }
    return table_name + "-part-" + std::to_string(partition.part_num) + "-of-" + std::to_string(partition.part_count) + ".parquet";
}

arrow::Status ParquetTableWriter::OpenPartition(
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & options,
    const PartitionSpec & partition,
    arrow::MemoryPool * pool)
{
#if !defined(ARROW_PARQUET)
    (void)table;
    (void)output;
    (void)options;
    (void)partition;
    (void)pool;
    return arrow::Status::NotImplemented("Arrow was built without Parquet support");
#else
    if (is_open_)
    {
        return arrow::Status::Invalid("ParquetTableWriter::OpenPartition() called while partition is already open");
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

    auto props_builder = ::parquet::WriterProperties::Builder();
    props_builder.compression(options_.compression);
    const auto row_group_rows = ResolveRowGroupRows(*table_, options_);
    if (row_group_rows > 0)
    {
        props_builder.max_row_group_length(row_group_rows);
    }
    auto props = props_builder.build();

    auto arrow_props = ::parquet::ArrowWriterProperties::Builder().set_use_threads(options_.use_threads)->build();

    ARROW_ASSIGN_OR_RAISE(
        writer_, ::parquet::arrow::FileWriter::Open(*table.schema, pool_, sink_, std::move(props), std::move(arrow_props)));

    RETURN_NOT_OK(writer_->NewBufferedRowGroup());
    is_open_ = true;
    return arrow::Status::OK();
#endif
}

arrow::Status ParquetTableWriter::WriteBatch(const TableBatch & batch)
{
#if !defined(ARROW_PARQUET)
    (void)batch;
    return arrow::Status::NotImplemented("Arrow was built without Parquet support");
#else
    if (!is_open_ || !writer_ || table_ == nullptr)
    {
        return arrow::Status::Invalid("ParquetTableWriter::WriteBatch() called before OpenPartition()");
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
    return writer_->WriteRecordBatch(*rb);
#endif
}

arrow::Status ParquetTableWriter::ClosePartition()
{
#if !defined(ARROW_PARQUET)
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

std::string ParquetTableWriter::JoinPath(const std::string & base, const std::string & leaf)
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
