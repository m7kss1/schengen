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

const VortexWriterOptions & VortexTableWriter::ResolveOptions(const WriterOptions & options)
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

std::string VortexTableWriter::BuildPartitionFileName(const std::string & table_name, const PartitionSpec & partition)
{
    if (partition.part_count <= 1)
    {
        return table_name + "-1.vortex";
    }
    return table_name + "-chunk-" + std::to_string(partition.part_num) + "-of-" + std::to_string(partition.part_count) + ".vortex";
}

arrow::Status VortexTableWriter::OpenPartition(
    const TableMetadata & table,
    const OutputLocation & output,
    const WriterOptions & options,
    const PartitionSpec & partition,
    arrow::MemoryPool * pool)
{
    (void)options;
    (void)pool;

    if (is_open_)
    {
        return arrow::Status::Invalid("VortexTableWriter::OpenPartition() called while partition is already open");
    }
    if (partition.part_num < 1 || partition.part_count < 1 || partition.part_num > partition.part_count)
    {
        return arrow::Status::Invalid("Invalid partition range");
    }

    // Compile-time smoke: proves Vortex C++ headers are reachable.
    vortex::VortexException smoke("vortex-cxx headers are available");
    (void)smoke;

    table_ = &table;
    output_uri_ = output.uri;

    ARROW_ASSIGN_OR_RAISE(fs_, ResolveTarget(output_uri_));
    ARROW_ASSIGN_OR_RAISE(const std::string base_dir, ResolvePath(output_uri_, *fs_));

    const auto table_dir = JoinPath(base_dir, table.name);
    RETURN_NOT_OK(fs_->CreateDir(table_dir, /*recursive=*/true));

    file_path_ = JoinPath(table_dir, BuildPartitionFileName(table.name, partition));

    ARROW_ASSIGN_OR_RAISE(const auto info, fs_->GetFileInfo(file_path_));
    if (info.type() != arrow::fs::FileType::NotFound)
    {
        return arrow::Status::AlreadyExists(file_path_);
    }

    batches_.clear();
    is_open_ = true;
    return arrow::Status::OK();
}

arrow::Status VortexTableWriter::WriteBatch(const TableBatch & batch)
{
    if (!is_open_ || table_ == nullptr)
    {
        return arrow::Status::Invalid("VortexTableWriter::WriteBatch() called before OpenPartition()");
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

arrow::Status VortexTableWriter::ClosePartition()
{
    if (!is_open_)
    {
        return arrow::Status::OK();
    }

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
    is_open_ = false;
    return arrow::Status::OK();
}
#endif
