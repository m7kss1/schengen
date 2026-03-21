#include "Storage/lance_writer.h"

#include <arrow/c/bridge.h>

namespace
{
arrow::Status InvalidOption(const char * option_name)
{
    return arrow::Status::Invalid(option_name, " must be >= 0");
}
} // namespace

const LanceWriterOptions & LanceOrderedWriter::ResolveOptions(const WriterOptions & options)
{
    static const LanceWriterOptions defaults{};

    if (!options.format_options)
    {
        return defaults;
    }

    auto typed_options = std::dynamic_pointer_cast<const LanceWriterOptions>(options.format_options);
    if (!typed_options)
    {
        return defaults;
    }
    return *typed_options;
}

std::string LanceOrderedWriter::BuildDatasetPath(const OutputLocation & output, const std::string & table_name)
{
    return JoinPath(JoinPath(output.uri, table_name), table_name + "-1.lance");
}

arrow::Status LanceOrderedWriter::OpenTable(
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & options,
    arrow::MemoryPool * pool)
{
    (void)pool;

    if (is_open_)
    {
        return arrow::Status::Invalid("LanceOrderedWriter::OpenTable() called while table is already open");
    }

    const auto & lance_options = ResolveOptions(options);
    RETURN_NOT_OK(ValidateNonNegative(lance_options.target_partition_rows, "lance-target-partition-rows"));
    RETURN_NOT_OK(ValidateNonNegative(lance_options.max_rows_per_file, "lance-max-rows-per-file"));
    RETURN_NOT_OK(ValidateNonNegative(lance_options.max_rows_per_group, "lance-max-rows-per-group"));
    RETURN_NOT_OK(ValidateNonNegative(lance_options.max_bytes_per_file, "lance-max-bytes-per-file"));

    table_ = &table;
    dataset_uri_ = BuildDatasetPath(output, table.name);

    ArrowSchema schema{};
    RETURN_NOT_OK(arrow::ExportSchema(*table.schema, &schema));

    const LanceWriterOptionsC bridge_options{
        .max_rows_per_file = lance_options.max_rows_per_file,
        .max_rows_per_group = lance_options.max_rows_per_group,
        .max_bytes_per_file = lance_options.max_bytes_per_file,
    };
    if (lance_writer_open(dataset_uri_.c_str(), &schema, &bridge_options, &handle_) != 0)
    {
        table_ = nullptr;
        dataset_uri_.clear();
        handle_ = nullptr;
        return BridgeStatus(nullptr, "Failed to open Lance dataset writer");
    }

    is_open_ = true;
    return arrow::Status::OK();
}

arrow::Status LanceOrderedWriter::BeginInputPartition(const PartitionSpec & partition)
{
    if (!is_open_ || handle_ == nullptr)
    {
        return arrow::Status::Invalid("LanceOrderedWriter::BeginInputPartition() called before OpenTable()");
    }
    if (partition.part_num < 1 || partition.part_count < 1 || partition.part_num > partition.part_count)
    {
        return arrow::Status::Invalid("Invalid partition range");
    }
    if (lance_writer_begin_partition(handle_, partition.part_num, partition.part_count) != 0)
    {
        return BridgeStatus(handle_, "Failed to begin Lance input partition");
    }
    return arrow::Status::OK();
}

arrow::Status LanceOrderedWriter::WriteBatch(const TableBatch & batch)
{
    if (!is_open_ || handle_ == nullptr || table_ == nullptr)
    {
        return arrow::Status::Invalid("LanceOrderedWriter::WriteBatch() called before OpenTable()");
    }
    if (batch.metadata == nullptr || batch.metadata != table_)
    {
        return arrow::Status::Invalid("TableBatch metadata mismatch");
    }
    if (batch.row_count == 0)
    {
        return arrow::Status::OK();
    }

    ArrowArray array{};
    const auto row_count = static_cast<std::int64_t>(batch.row_count);
    auto record_batch = arrow::RecordBatch::Make(table_->schema, row_count, batch.columns);
    RETURN_NOT_OK(arrow::ExportRecordBatch(*record_batch, &array));

    if (lance_writer_push_batch(handle_, &array) != 0)
    {
        return BridgeStatus(handle_, "Failed to push Arrow batch into Lance writer");
    }
    return arrow::Status::OK();
}

arrow::Status LanceOrderedWriter::EndInputPartition()
{
    if (!is_open_ || handle_ == nullptr)
    {
        return arrow::Status::Invalid("LanceOrderedWriter::EndInputPartition() called before OpenTable()");
    }
    if (lance_writer_end_partition(handle_) != 0)
    {
        return BridgeStatus(handle_, "Failed to end Lance input partition");
    }
    return arrow::Status::OK();
}

arrow::Status LanceOrderedWriter::CloseTable()
{
    if (!is_open_)
    {
        CloseHandle();
        table_ = nullptr;
        dataset_uri_.clear();
        return arrow::Status::OK();
    }

    arrow::Status finish_status = arrow::Status::OK();
    if (handle_ != nullptr && lance_writer_finish(handle_) != 0)
    {
        finish_status = BridgeStatus(handle_, "Failed to finish Lance dataset writer");
    }

    CloseHandle();
    table_ = nullptr;
    dataset_uri_.clear();
    is_open_ = false;
    return finish_status;
}

std::string LanceOrderedWriter::JoinPath(const std::string & base, const std::string & leaf)
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

arrow::Status LanceOrderedWriter::ValidateNonNegative(std::int64_t value, const char * option_name)
{
    if (value < 0)
    {
        return InvalidOption(option_name);
    }
    return arrow::Status::OK();
}

void LanceOrderedWriter::CloseHandle()
{
    if (handle_ != nullptr)
    {
        lance_writer_destroy(handle_);
        handle_ = nullptr;
    }
}

arrow::Status LanceOrderedWriter::BridgeStatus(const LanceWriterHandle * handle, const char * fallback)
{
    const char * message = handle != nullptr ? lance_last_error(handle) : lance_bridge_last_error();
    if (message == nullptr || *message == '\0')
    {
        message = fallback;
    }
    return arrow::Status::IOError(message);
}
