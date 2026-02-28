#pragma once

#include "Tables/table.h"

#include <arrow/api.h>
#include <arrow/status.h>
#include <arrow/util/config.h>

#include <cstdint>
#include <memory>
#include <string>

struct ScaleConfig;

enum class OutputFormat : uint8_t
{
    Parquet,
    Vortex,
    /* TODO: Orc, Lance, Iceberg, Paimon */
};

struct OutputLocation
{
    /*
     * Output directory URI/path
     * Examples:
     *  - "file:///tmp/schengen"
     *  - "s3://bucket/prefix"
     *  - "obs://bucket/prefix" (TODO)
     */
    std::string uri;
};

struct IFormatWriterOptions
{
    virtual ~IFormatWriterOptions() = default;
};

struct WriterOptions
{
    OutputFormat format = OutputFormat::Parquet;
    std::shared_ptr<const IFormatWriterOptions> format_options;
};

class ITableWriter
{
public:
    virtual ~ITableWriter() = default;

    virtual arrow::Status Open(
        const TableMetadata & table,
        const OutputLocation & output,
        const WriterOptions & options,
        arrow::MemoryPool * pool)
        = 0;

    virtual arrow::Status BeginPartition(std::int32_t part_num, std::int32_t part_count) = 0;
    virtual arrow::Status WriteBatch(const TableBatch & batch) = 0;
    virtual arrow::Status EndPartition() = 0;
    virtual arrow::Status Close() = 0;
};

std::unique_ptr<ITableWriter> MakeTableWriter(OutputFormat format);
std::int32_t ResolveWriterPartCount(const TableMetadata & table, const ScaleConfig & scale, const WriterOptions & options);
