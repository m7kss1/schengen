#pragma once

#include "Tables/table.h"

#include <arrow/api.h>
#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/util/config.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace boost::program_options
{
class options_description;
class variables_map;
} // namespace boost::program_options

struct ScaleConfig;

enum class OutputFormat : uint8_t
{
    Parquet,
    Orc,
    Vortex,
    /* TODO: Lance, Iceberg, Paimon */
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

struct PartitionSpec
{
    std::int32_t part_num = 1;
    std::int32_t part_count = 1;
};

class ITableWriter
{
public:
    virtual ~ITableWriter() = default;

    virtual arrow::Status OpenPartition(
        const TableMetadata & table,
        const OutputLocation & output,
        const WriterOptions & options,
        const PartitionSpec & partition,
        arrow::MemoryPool * pool)
        = 0;

    virtual arrow::Status WriteBatch(const TableBatch & batch) = 0;
    virtual arrow::Status ClosePartition() = 0;
};

class IFormatDriver
{
public:
    virtual ~IFormatDriver() = default;

    virtual std::string_view Name() const = 0;
    virtual OutputFormat Format() const = 0;
    virtual void RegisterCliOptions(boost::program_options::options_description & desc) const = 0;
    virtual arrow::Result<WriterOptions> BuildWriterOptions(const boost::program_options::variables_map & vm) const = 0;
    virtual std::int32_t ResolvePartCount(const TableMetadata & table, const ScaleConfig & scale, const WriterOptions & options)
        const
        = 0;
    virtual std::unique_ptr<ITableWriter> CreateWriter() const = 0;
};

std::vector<std::string> SupportedFormatNames();
void RegisterFormatCliOptions(boost::program_options::options_description & desc);
arrow::Result<const IFormatDriver *> ResolveFormatDriver(std::string_view format_name);
