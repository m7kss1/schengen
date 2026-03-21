#include "Storage/vortex_writer.h"

#if defined(ENABLE_VORTEX)
#    include <arrow/api.h>
#    include <arrow/c/bridge.h>
#endif

#if defined(ENABLE_VORTEX)
namespace
{
arrow::Status InvalidOption(const char * option_name)
{
    return arrow::Status::Invalid(option_name, " must be >= 0");
}
} // namespace

const VortexWriterOptions & VortexOrderedWriter::ResolveOptions(const WriterOptions & options)
{
    static const VortexWriterOptions defaults{};

    if (!options.format_options)
    {
        return defaults;
    }

    auto typed_options = std::dynamic_pointer_cast<const VortexWriterOptions>(options.format_options);
    if (!typed_options)
    {
        return defaults;
    }
    return *typed_options;
}

std::string VortexOrderedWriter::BuildFilePath(const OutputLocation & output, const std::string & table_name)
{
    return JoinPath(JoinPath(output.uri, table_name), table_name + "-1.vortex");
}

arrow::Status VortexOrderedWriter::OpenTable(
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & options,
    arrow::MemoryPool * pool)
{
    (void)pool;

    if (is_open_)
    {
        return arrow::Status::Invalid("VortexOrderedWriter::OpenTable() called while table is already open");
    }

    const auto & vortex_options = ResolveOptions(options);
    RETURN_NOT_OK(ValidateNonNegative(vortex_options.target_partition_rows, "vortex-target-partition-rows"));

    table_ = &table;
    file_uri_ = BuildFilePath(output, table.name);

    ArrowSchema schema{};
    RETURN_NOT_OK(arrow::ExportSchema(*table.schema, &schema));

    const VortexWriterOptionsC bridge_options{
        .abi_version = 1,
        .reserved = 0,
    };
    if (vortex_writer_open(file_uri_.c_str(), &schema, &bridge_options, &handle_) != 0)
    {
        table_ = nullptr;
        file_uri_.clear();
        handle_ = nullptr;
        return BridgeStatus(nullptr, "Failed to open Vortex file writer");
    }

    wrote_non_empty_batch_ = false;
    is_open_ = true;
    return arrow::Status::OK();
}

arrow::Status VortexOrderedWriter::BeginInputPartition(const PartitionSpec & partition)
{
    if (!is_open_ || handle_ == nullptr)
    {
        return arrow::Status::Invalid("VortexOrderedWriter::BeginInputPartition() called before OpenTable()");
    }
    if (partition.part_num < 1 || partition.part_count < 1 || partition.part_num > partition.part_count)
    {
        return arrow::Status::Invalid("Invalid partition range");
    }
    if (vortex_writer_begin_partition(handle_, partition.part_num, partition.part_count) != 0)
    {
        return BridgeStatus(handle_, "Failed to begin Vortex input partition");
    }
    return arrow::Status::OK();
}

arrow::Status VortexOrderedWriter::WriteBatch(const TableBatch & batch)
{
    if (!is_open_ || handle_ == nullptr || table_ == nullptr)
    {
        return arrow::Status::Invalid("VortexOrderedWriter::WriteBatch() called before OpenTable()");
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
    auto record_batch = arrow::RecordBatch::Make(table_->schema, row_count, batch.columns);
    RETURN_NOT_OK(PushRecordBatch(record_batch));
    wrote_non_empty_batch_ = true;
    return arrow::Status::OK();
}

arrow::Status VortexOrderedWriter::EndInputPartition()
{
    if (!is_open_ || handle_ == nullptr)
    {
        return arrow::Status::Invalid("VortexOrderedWriter::EndInputPartition() called before OpenTable()");
    }
    if (vortex_writer_end_partition(handle_) != 0)
    {
        return BridgeStatus(handle_, "Failed to end Vortex input partition");
    }
    return arrow::Status::OK();
}

arrow::Status VortexOrderedWriter::CloseTable()
{
    if (!is_open_)
    {
        CloseHandle();
        table_ = nullptr;
        file_uri_.clear();
        wrote_non_empty_batch_ = false;
        return arrow::Status::OK();
    }

    arrow::Status finish_status = arrow::Status::OK();
    if (!wrote_non_empty_batch_ && table_ != nullptr)
    {
        std::vector<std::shared_ptr<arrow::Array>> empty_columns;
        empty_columns.reserve(static_cast<std::size_t>(table_->schema->num_fields()));
        for (const auto & field : table_->schema->fields())
        {
            ARROW_ASSIGN_OR_RAISE(auto arr, arrow::MakeArrayOfNull(field->type(), 0));
            empty_columns.emplace_back(std::move(arr));
        }

        auto empty_batch = arrow::RecordBatch::Make(table_->schema, 0, std::move(empty_columns));
        const auto empty_status = PushRecordBatch(empty_batch);
        if (!empty_status.ok())
        {
            finish_status = empty_status;
        }
    }

    if (handle_ != nullptr && vortex_writer_finish(handle_) != 0 && finish_status.ok())
    {
        finish_status = BridgeStatus(handle_, "Failed to finish Vortex file writer");
    }

    CloseHandle();
    table_ = nullptr;
    file_uri_.clear();
    is_open_ = false;
    wrote_non_empty_batch_ = false;
    return finish_status;
}

std::string VortexOrderedWriter::JoinPath(const std::string & base, const std::string & leaf)
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

arrow::Status VortexOrderedWriter::ValidateNonNegative(std::int64_t value, const char * option_name)
{
    if (value < 0)
    {
        return InvalidOption(option_name);
    }
    return arrow::Status::OK();
}

arrow::Status VortexOrderedWriter::BridgeStatus(const VortexWriterHandle * handle, const char * fallback)
{
    const char * message = handle != nullptr ? vortex_last_error(handle) : vortex_bridge_last_error();
    if (message == nullptr || *message == '\0')
    {
        message = fallback;
    }
    return arrow::Status::IOError(message);
}

arrow::Status VortexOrderedWriter::PushRecordBatch(const std::shared_ptr<arrow::RecordBatch> & record_batch)
{
    ArrowArray array{};
    RETURN_NOT_OK(arrow::ExportRecordBatch(*record_batch, &array));
    if (vortex_writer_push_batch(handle_, &array) != 0)
    {
        return BridgeStatus(handle_, "Failed to push Arrow batch into Vortex writer");
    }
    return arrow::Status::OK();
}

void VortexOrderedWriter::CloseHandle()
{
    if (handle_ != nullptr)
    {
        vortex_writer_destroy(handle_);
        handle_ = nullptr;
    }
}
#endif
