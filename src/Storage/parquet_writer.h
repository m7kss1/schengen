#pragma once

#include "Storage/storage.h"
#include "Storage/writer.h"

#include <arrow/io/interfaces.h>
#include <arrow/result.h>

#include <memory>
#include <string>
#include <string_view>

#if defined(ARROW_PARQUET)
#    include <parquet/arrow/writer.h>
#    include <parquet/properties.h>
#endif

/*
 * Arrow comments for WriteRecordBatch:
 * If you are writing multiple files in parallel in the same
 * executor, deadlock may occur if ArrowWriterProperties::use_threads
 * is set to true to write columns in parallel. Please disable use_threads
 * option in this case.
 */
struct ParquetWriterOptions final : IFormatWriterOptions
{
    bool use_threads = false;
    std::int64_t row_group_bytes = 7 * 1024 * 1024;
    std::int64_t max_row_group_rows = 0;
    std::int64_t output_buffer_bytes = 21 * 1024 * 1024;
#if defined(ARROW_PARQUET)
#    if defined(ARROW_WITH_SNAPPY)
    ::parquet::Compression::type compression = ::parquet::Compression::SNAPPY;
#    else
    ::parquet::Compression::type compression = ::parquet::Compression::UNCOMPRESSED;
#    endif
#else
    int compression = 0;
#endif
};

class ParquetTableWriter final : public ITableWriter
{
public:
    static arrow::Result<FileSystemPtr> GetFilesystem(std::string_view target_uri);
    static std::int64_t EstimateParquetRowBytes(std::string_view table_name);
    static std::int64_t ResolveRowGroupRows(const TableMetadata & table, const ParquetWriterOptions & options);
    static const ParquetWriterOptions & ResolveOptions(const WriterOptions & options);
    static arrow::Result<std::string> GetPath(std::string_view uri, const arrow::fs::FileSystem & fs);

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
    static std::string JoinPath(const std::string & base, const std::string & leaf);

    const TableMetadata * table_ = nullptr;
    arrow::MemoryPool * pool_ = nullptr;
    ParquetWriterOptions options_{};

#if defined(ARROW_PARQUET)
    FileSystemPtr fs_;
    std::string temp_path_;
    std::string final_path_;
    std::shared_ptr<arrow::io::OutputStream> sink_;
    std::unique_ptr<::parquet::arrow::FileWriter> writer_;
#endif

    bool row_group_open_ = false;
};
