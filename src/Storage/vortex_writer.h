#pragma once

#include "Storage/vortex_bridge.h"
#include "Storage/writer.h"

#include <arrow/result.h>

#include <memory>
#include <string>

#if defined(ENABLE_VORTEX)
struct VortexWriterOptions final : IFormatWriterOptions
{
    std::int64_t target_partition_rows = 0;
};

class VortexOrderedWriter final : public IOrderedTableWriter
{
public:
    static const VortexWriterOptions & ResolveOptions(const WriterOptions & options);
    static std::string BuildFilePath(const OutputLocation & output, const std::string & table_name);

    arrow::Status OpenTable(
        const TableMetadata & table,
        const OutputLocation & output,
        const WriterOptions & options,
        arrow::MemoryPool * pool) override;

    arrow::Status BeginInputPartition(const PartitionSpec & partition) override;
    arrow::Status WriteBatch(const TableBatch & batch) override;
    arrow::Status EndInputPartition() override;
    arrow::Status CloseTable() override;

private:
    static std::string JoinPath(const std::string & base, const std::string & leaf);
    static arrow::Status ValidateNonNegative(std::int64_t value, const char * option_name);
    static arrow::Status BridgeStatus(const VortexWriterHandle * handle, const char * fallback);

    arrow::Status PushRecordBatch(const std::shared_ptr<arrow::RecordBatch> & record_batch);
    void CloseHandle();

    const TableMetadata * table_ = nullptr;
    std::string file_uri_;
    VortexWriterHandle * handle_ = nullptr;
    bool is_open_ = false;
    bool wrote_non_empty_batch_ = false;
};
#endif
