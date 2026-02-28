#include "Storage/vortex_writer.h"

#if defined(ENABLE_VORTEX)
#    include <vortex/exception.hpp>
#    include <vortex/write_options.hpp>
#endif

#include <arrow/c/bridge.h>
#include <arrow/filesystem/filesystem.h>
#include <arrow/record_batch.h>
#include <arrow/table.h>
#include <arrow/type.h>
#include <arrow/type_fwd.h>
#include <arrow/util/string.h>

#if defined(ENABLE_VORTEX)
namespace
{
std::string JoinPath(const std::string & base, const std::string & leaf)
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

arrow::Result<std::string> ResolvePath(std::string_view uri, const arrow::fs::FileSystem & fs)
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
} // namespace

arrow::Status VortexTableWriter::Open(
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & options,
    arrow::MemoryPool * pool)
{
    (void)options;
    (void)pool;

    // Compile-time smoke: proves Vortex C++ headers are reachable.
    vortex::VortexException smoke("vortex-cxx headers are available");
    (void)smoke;

    table_ = &table;
    output_uri_ = output.uri;

    ARROW_ASSIGN_OR_RAISE(fs_, ResolveTarget(output_uri_));
    ARROW_ASSIGN_OR_RAISE(const std::string base_dir, ResolvePath(output_uri_, *fs_));

    const auto table_dir = JoinPath(base_dir, table.name);
    RETURN_NOT_OK(fs_->CreateDir(table_dir, /*recursive=*/true));

    file_path_ = JoinPath(table_dir, table.name + "-1.vortex");

    ARROW_ASSIGN_OR_RAISE(const auto info, fs_->GetFileInfo(file_path_));
    if (info.type() != arrow::fs::FileType::NotFound)
    {
        return arrow::Status::AlreadyExists(file_path_);
    }

    batches_.clear();
    partition_open_ = false;
    return arrow::Status::OK();
}

arrow::Status VortexTableWriter::BeginPartition(std::int32_t part_num, std::int32_t part_count)
{
    if (table_ == nullptr)
    {
        return arrow::Status::Invalid("VortexTableWriter::BeginPartition() called before Open()");
    }
    if (part_num < 1 || part_count < 1 || part_num > part_count)
    {
        return arrow::Status::Invalid("Invalid partition range");
    }
    if (partition_open_)
    {
        return arrow::Status::Invalid("Partition is already open");
    }
    partition_open_ = true;
    return arrow::Status::OK();
}

arrow::Status VortexTableWriter::WriteBatch(const TableBatch & batch)
{
    if (table_ == nullptr)
    {
        return arrow::Status::Invalid("VortexTableWriter::WriteBatch() called before Open()");
    }
    if (!partition_open_)
    {
        return arrow::Status::Invalid("VortexTableWriter::WriteBatch() called before BeginPartition()");
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
    batches_.push_back(arrow::RecordBatch::Make(table_->schema, row_count, batch.columns));
    return arrow::Status::OK();
}

arrow::Status VortexTableWriter::EndPartition()
{
    if (!partition_open_)
    {
        return arrow::Status::Invalid("VortexTableWriter::EndPartition() called without active partition");
    }
    partition_open_ = false;
    return arrow::Status::OK();
}

arrow::Status VortexTableWriter::Close()
{
    if (table_ != nullptr)
    {
        try
        {
            if (batches_.empty())
            {
                std::vector<std::shared_ptr<arrow::Array>> empty_columns;
                empty_columns.reserve(static_cast<std::size_t>(table_->schema->num_fields()));
                for (const auto & field : table_->schema->fields())
                {
                    ARROW_ASSIGN_OR_RAISE(auto arr, arrow::MakeArrayOfNull(field->type(), 0));
                    empty_columns.emplace_back(std::move(arr));
                }
                batches_.push_back(arrow::RecordBatch::Make(table_->schema, 0, std::move(empty_columns)));
            }

            ARROW_ASSIGN_OR_RAISE(
                auto reader, arrow::RecordBatchReader::Make(std::move(batches_), table_->schema));

            ArrowArrayStream stream{};
            RETURN_NOT_OK(arrow::ExportRecordBatchReader(reader, &stream));

            vortex::VortexWriteOptions write_options;
            write_options.WriteArrayStream(stream, file_path_);
        }
        catch (const vortex::VortexException & ex)
        {
            return arrow::Status::IOError("Vortex write failed: ", ex.what());
        }
        catch (const std::exception & ex)
        {
            return arrow::Status::IOError("Vortex write failed: ", ex.what());
        }
    }

    table_ = nullptr;
    output_uri_.clear();
    file_path_.clear();
    fs_.reset();
    batches_.clear();
    partition_open_ = false;
    return arrow::Status::OK();
}
#endif
