#pragma once

#include "Storage/storage.h"
#include "Storage/writer.h"

#include <arrow/record_batch.h>

#include <memory>
#include <string>
#include <vector>

#if defined(ENABLE_VORTEX)
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
    arrow::Status Open(
        const TableMetadata & table,
        const OutputLocation & output,
        const WriterOptions & options,
        arrow::MemoryPool * pool) override;

    arrow::Status BeginPartition(std::int32_t part_num, std::int32_t part_count) override;
    arrow::Status WriteBatch(const TableBatch & batch) override;
    arrow::Status EndPartition() override;
    arrow::Status Close() override;

private:
    const TableMetadata * table_ = nullptr;
    std::string output_uri_;
    FileSystemPtr fs_;
    std::string file_path_;
    std::vector<std::shared_ptr<arrow::RecordBatch>> batches_;
    bool partition_open_ = false;
};
#endif
