#pragma once

#include "Storage/lance_bridge.h"
#include "Storage/writer.h"

#include <arrow/result.h>

#include <memory>
#include <string>

struct LanceWriterOptions final : IFormatWriterOptions
{
    std::int64_t target_partition_rows = 0;
    std::int64_t max_rows_per_file = 0;
    std::int64_t max_rows_per_group = 0;
    std::int64_t max_bytes_per_file = 0;
};

class LanceOrderedWriter final : public IOrderedTableWriter
{
public:
    static const LanceWriterOptions & ResolveOptions(const WriterOptions & options);
    static std::string BuildDatasetPath(const OutputLocation & output, const std::string & table_name);

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
    void CloseHandle();
    static arrow::Status BridgeStatus(const LanceWriterHandle * handle, const char * fallback);

    const TableMetadata * table_ = nullptr;
    std::string dataset_uri_;
    LanceWriterHandle * handle_ = nullptr;
    bool is_open_ = false;
};
