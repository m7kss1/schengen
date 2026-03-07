#pragma once

#include "Storage/storage.h"
#include "Storage/writer.h"

#include <arrow/record_batch.h>

#include <memory>
#include <string>
#include <vector>

#if defined(ENABLE_VORTEX)
struct VortexWriterOptions final : IFormatWriterOptions
{
    std::int64_t max_partition_rows = 0;
};

/*
 * This implementation is intentionally minimal and is used as a very simple
 * PoC writer. It is expected to be reworked and extended
 *
 * TODO:
 * - Replace full in-memory buffering with streaming/chunked writing
 * - Add format-specific writer options
 * - Support multipart/partition-aware output strategy
 */
class VortexTableWriter final : public ITableWriter
{
public:
    static const VortexWriterOptions & ResolveOptions(const WriterOptions & options);
    static std::string BuildPartitionFileName(const std::string & table_name, const PartitionSpec & partition);

    arrow::Status OpenPartition(
        const TableMetadata & table,
        const OutputLocation & output,
        const WriterOptions & options,
        const PartitionSpec & partition,
        arrow::MemoryPool * pool) override;

    arrow::Status WriteBatch(const TableBatch & batch) override;
    arrow::Status ClosePartition() override;

private:
    const TableMetadata * table_ = nullptr;
    std::string output_uri_;
    FileSystemPtr fs_;
    std::string file_path_;
    std::vector<std::shared_ptr<arrow::RecordBatch>> batches_;
    bool is_open_ = false;
};
#endif
