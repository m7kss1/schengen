#pragma once

#include "Storage/storage.h"
#include "Storage/writer.h"

#include <arrow/io/interfaces.h>
#include <arrow/result.h>

#include <memory>
#include <string>
#include <string_view>

#if defined(ARROW_ORC)
#    include <arrow/adapters/orc/adapter.h>
#endif

struct OrcWriterOptions final : IFormatWriterOptions
{
    std::int64_t max_partition_rows = 0;
    std::int64_t stripe_bytes = 7 * 1024 * 1024;
    std::int64_t max_stripe_rows = 0;
    std::int64_t output_buffer_bytes = 21 * 1024 * 1024;
#if defined(ARROW_ORC) && defined(ARROW_WITH_SNAPPY)
    ::arrow::Compression::type compression = ::arrow::Compression::SNAPPY;
#elif defined(ARROW_ORC)
    ::arrow::Compression::type compression = ::arrow::Compression::UNCOMPRESSED;
#else
    int compression = 0;
#endif
};

class OrcTableWriter final : public ITableWriter
{
public:
    static arrow::Result<FileSystemPtr> GetFilesystem(std::string_view target_uri);
    static std::int64_t EstimateOrcRowBytes(std::string_view table_name);
    static std::int64_t ResolveStripeRows(const TableMetadata & table, const OrcWriterOptions & options);
    static std::int64_t ResolveStripeBytes(const TableMetadata & table, const OrcWriterOptions & options);
    static const OrcWriterOptions & ResolveOptions(const WriterOptions & options);
    static arrow::Result<std::string> GetPath(std::string_view uri, const arrow::fs::FileSystem & fs);
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
    static std::string JoinPath(const std::string & base, const std::string & leaf);

    const TableMetadata * table_ = nullptr;
    arrow::MemoryPool * pool_ = nullptr;
    OrcWriterOptions options_{};
    bool is_open_ = false;

#if defined(ARROW_ORC)
    FileSystemPtr fs_;
    std::string temp_path_;
    std::string final_path_;
    std::shared_ptr<arrow::io::OutputStream> sink_;
    std::unique_ptr<arrow::adapters::orc::ORCFileWriter> writer_;
#endif
};
