#include "Storage/writer.h"

#include "Common/partition.h"
#include "Storage/orc_writer.h"
#include "Storage/parquet_writer.h"

#if defined(ENABLE_VORTEX)
#    include "Storage/vortex_writer.h"
#endif

#include <boost/program_options.hpp>

#include <algorithm>
#include <cctype>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace po = boost::program_options;

namespace
{
std::string ToLower(std::string value)
{
    std::transform(
        value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::int32_t ResolvePartCountByTargetRows(std::uint64_t total_rows, std::int64_t target_rows)
{
    if (target_rows <= 0 || total_rows == 0)
    {
        return 1;
    }
    const auto target_rows_u = static_cast<std::uint64_t>(target_rows);
    const auto parts_u = (total_rows + target_rows_u - 1) / target_rows_u;
    const auto max_parts = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    return static_cast<std::int32_t>(std::min(parts_u, max_parts));
}

class ParquetFormatDriver final : public IFormatDriver
{
public:
    std::string_view Name() const override { return "parquet"; }
    OutputFormat Format() const override { return OutputFormat::Parquet; }

    void RegisterCliOptions(po::options_description & desc) const override
    {
        const ParquetWriterOptions defaults{};
        desc.add_options()(
            "parquet-use-threads",
            po::bool_switch()->default_value(defaults.use_threads),
            "Enable Parquet internal column write threads")(
            "parquet-row-group-bytes",
            po::value<std::int64_t>()->default_value(defaults.row_group_bytes),
            "Target Parquet row group size in bytes (<=0 disables auto partitioning)")(
            "parquet-max-row-group-rows",
            po::value<std::int64_t>()->default_value(defaults.max_row_group_rows),
            "Max Parquet row group rows (overrides parquet-row-group-bytes when >0)")(
            "parquet-output-buffer-bytes",
            po::value<std::int64_t>()->default_value(defaults.output_buffer_bytes),
            "Buffered output stream size in bytes (<=0 disables buffering)")(
            "parquet-compression",
            po::value<std::string>()->default_value(DefaultCompressionName()),
            "Parquet compression codec: snappy | uncompressed");
    }

    arrow::Result<WriterOptions> BuildWriterOptions(const po::variables_map & vm) const override
    {
        auto parquet_options = std::make_shared<ParquetWriterOptions>();
        parquet_options->use_threads = vm["parquet-use-threads"].as<bool>();
        parquet_options->row_group_bytes = vm["parquet-row-group-bytes"].as<std::int64_t>();
        parquet_options->max_row_group_rows = vm["parquet-max-row-group-rows"].as<std::int64_t>();
        parquet_options->output_buffer_bytes = vm["parquet-output-buffer-bytes"].as<std::int64_t>();

        if (parquet_options->max_row_group_rows < 0)
        {
            return arrow::Status::Invalid("parquet-max-row-group-rows must be >= 0");
        }
        if (parquet_options->row_group_bytes < 0)
        {
            return arrow::Status::Invalid("parquet-row-group-bytes must be >= 0");
        }

        const auto compression = ToLower(vm["parquet-compression"].as<std::string>());
        RETURN_NOT_OK(ParseCompression(compression, parquet_options.get()));

        WriterOptions options;
        options.format = OutputFormat::Parquet;
        options.format_options = parquet_options;
        return options;
    }

    std::int32_t ResolvePartCount(const TableMetadata & table, const ScaleConfig & scale, const WriterOptions & options) const override
    {
        const auto & parquet_options = ParquetTableWriter::ResolveOptions(options);
        const auto row_group_rows = ParquetTableWriter::ResolveRowGroupRows(table, parquet_options);
        return ResolvePartCountByTargetRows(scale.RowCount(table), row_group_rows);
    }

    std::unique_ptr<ITableWriter> CreateWriter() const override { return std::make_unique<ParquetTableWriter>(); }

private:
    static std::string DefaultCompressionName()
    {
#if defined(ARROW_PARQUET) && defined(ARROW_WITH_SNAPPY)
        return "snappy";
#else
        return "uncompressed";
#endif
    }

    static arrow::Status ParseCompression(const std::string & compression, ParquetWriterOptions * options)
    {
#if !defined(ARROW_PARQUET)
        (void)compression;
        (void)options;
        return arrow::Status::NotImplemented("Arrow was built without Parquet support");
#else
        if (compression == "uncompressed")
        {
            options->compression = ::parquet::Compression::UNCOMPRESSED;
            return arrow::Status::OK();
        }
        if (compression == "snappy")
        {
#    if defined(ARROW_WITH_SNAPPY)
            options->compression = ::parquet::Compression::SNAPPY;
            return arrow::Status::OK();
#    else
            return arrow::Status::Invalid("parquet-compression=snappy requires Arrow built with Snappy");
#    endif
        }
        return arrow::Status::Invalid("Unsupported parquet-compression: ", compression);
#endif
    }
};

#if defined(ARROW_ORC)
class OrcFormatDriver final : public IFormatDriver
{
public:
    std::string_view Name() const override { return "orc"; }
    OutputFormat Format() const override { return OutputFormat::Orc; }

    void RegisterCliOptions(po::options_description & desc) const override
    {
        const OrcWriterOptions defaults{};
        desc.add_options()(
            "orc-max-partition-rows",
            po::value<std::int64_t>()->default_value(defaults.max_partition_rows),
            "Max rows per output ORC partition file (<=0 derives from stripe settings)")(
            "orc-stripe-bytes",
            po::value<std::int64_t>()->default_value(defaults.stripe_bytes),
            "Target ORC stripe size in bytes (<=0 disables byte-based stripe sizing)")(
            "orc-max-stripe-rows",
            po::value<std::int64_t>()->default_value(defaults.max_stripe_rows),
            "Max ORC stripe rows (overrides orc-stripe-bytes when >0)")(
            "orc-output-buffer-bytes",
            po::value<std::int64_t>()->default_value(defaults.output_buffer_bytes),
            "Buffered output stream size in bytes (<=0 disables buffering)")(
            "orc-compression",
            po::value<std::string>()->default_value(DefaultCompressionName()),
            "ORC compression codec: snappy | zstd | lz4 | zlib | uncompressed");
    }

    arrow::Result<WriterOptions> BuildWriterOptions(const po::variables_map & vm) const override
    {
        auto orc_options = std::make_shared<OrcWriterOptions>();
        orc_options->max_partition_rows = vm["orc-max-partition-rows"].as<std::int64_t>();
        orc_options->stripe_bytes = vm["orc-stripe-bytes"].as<std::int64_t>();
        orc_options->max_stripe_rows = vm["orc-max-stripe-rows"].as<std::int64_t>();
        orc_options->output_buffer_bytes = vm["orc-output-buffer-bytes"].as<std::int64_t>();

        if (orc_options->max_partition_rows < 0)
        {
            return arrow::Status::Invalid("orc-max-partition-rows must be >= 0");
        }
        if (orc_options->stripe_bytes < 0)
        {
            return arrow::Status::Invalid("orc-stripe-bytes must be >= 0");
        }
        if (orc_options->max_stripe_rows < 0)
        {
            return arrow::Status::Invalid("orc-max-stripe-rows must be >= 0");
        }
        if (orc_options->output_buffer_bytes < 0)
        {
            return arrow::Status::Invalid("orc-output-buffer-bytes must be >= 0");
        }

        const auto compression = ToLower(vm["orc-compression"].as<std::string>());
        RETURN_NOT_OK(ParseCompression(compression, orc_options.get()));

        WriterOptions options;
        options.format = OutputFormat::Orc;
        options.format_options = orc_options;
        return options;
    }

    std::int32_t ResolvePartCount(const TableMetadata & table, const ScaleConfig & scale, const WriterOptions & options) const override
    {
        const auto & orc_options = OrcTableWriter::ResolveOptions(options);
        if (orc_options.max_partition_rows > 0)
        {
            return ResolvePartCountByTargetRows(scale.RowCount(table), orc_options.max_partition_rows);
        }
        const auto stripe_rows = OrcTableWriter::ResolveStripeRows(table, orc_options);
        return ResolvePartCountByTargetRows(scale.RowCount(table), stripe_rows);
    }

    std::unique_ptr<ITableWriter> CreateWriter() const override { return std::make_unique<OrcTableWriter>(); }

private:
    static std::string DefaultCompressionName()
    {
#    if defined(ARROW_WITH_SNAPPY)
        return "snappy";
#    else
        return "uncompressed";
#    endif
    }

    static arrow::Status ParseCompression(const std::string & compression, OrcWriterOptions * options)
    {
        if (compression == "uncompressed")
        {
            options->compression = ::arrow::Compression::UNCOMPRESSED;
            return arrow::Status::OK();
        }
        if (compression == "snappy")
        {
#    if defined(ARROW_WITH_SNAPPY)
            options->compression = ::arrow::Compression::SNAPPY;
            return arrow::Status::OK();
#    else
            return arrow::Status::Invalid("orc-compression=snappy requires Arrow built with Snappy");
#    endif
        }
        if (compression == "zstd")
        {
#    if defined(ARROW_WITH_ZSTD)
            options->compression = ::arrow::Compression::ZSTD;
            return arrow::Status::OK();
#    else
            return arrow::Status::Invalid("orc-compression=zstd requires Arrow built with ZSTD");
#    endif
        }
        if (compression == "lz4")
        {
#    if defined(ARROW_WITH_LZ4)
            options->compression = ::arrow::Compression::LZ4;
            return arrow::Status::OK();
#    else
            return arrow::Status::Invalid("orc-compression=lz4 requires Arrow built with LZ4");
#    endif
        }
        if (compression == "zlib")
        {
#    if defined(ARROW_WITH_ZLIB)
            options->compression = ::arrow::Compression::GZIP;
            return arrow::Status::OK();
#    else
            return arrow::Status::Invalid("orc-compression=zlib requires Arrow built with ZLIB");
#    endif
        }
        return arrow::Status::Invalid("Unsupported orc-compression: ", compression);
    }
};
#endif

#if defined(ENABLE_VORTEX)
class VortexFormatDriver final : public IFormatDriver
{
public:
    std::string_view Name() const override { return "vortex"; }
    OutputFormat Format() const override { return OutputFormat::Vortex; }

    void RegisterCliOptions(po::options_description & desc) const override
    {
        const VortexWriterOptions defaults{};
        desc.add_options()(
            "vortex-max-partition-rows",
            po::value<std::int64_t>()->default_value(defaults.max_partition_rows),
            "Max rows per output Vortex partition file (<=0 disables partition splitting)");
    }

    arrow::Result<WriterOptions> BuildWriterOptions(const po::variables_map & vm) const override
    {
        auto vortex_options = std::make_shared<VortexWriterOptions>();
        vortex_options->max_partition_rows = vm["vortex-max-partition-rows"].as<std::int64_t>();
        if (vortex_options->max_partition_rows < 0)
        {
            return arrow::Status::Invalid("vortex-max-partition-rows must be >= 0");
        }

        WriterOptions options;
        options.format = OutputFormat::Vortex;
        options.format_options = vortex_options;
        return options;
    }

    std::int32_t ResolvePartCount(const TableMetadata & table, const ScaleConfig & scale, const WriterOptions & options) const override
    {
        const auto & vortex_options = VortexTableWriter::ResolveOptions(options);
        return ResolvePartCountByTargetRows(scale.RowCount(table), vortex_options.max_partition_rows);
    }

    std::unique_ptr<ITableWriter> CreateWriter() const override { return std::make_unique<VortexTableWriter>(); }
};
#endif

const std::vector<const IFormatDriver *> & EnabledDrivers()
{
    static const std::vector<const IFormatDriver *> drivers = [] {
        std::vector<const IFormatDriver *> out;
#if defined(ARROW_PARQUET)
        static const ParquetFormatDriver parquet_driver;
        out.push_back(&parquet_driver);
#endif
#if defined(ARROW_ORC)
        static const OrcFormatDriver orc_driver;
        out.push_back(&orc_driver);
#endif
#if defined(ENABLE_VORTEX)
        static const VortexFormatDriver vortex_driver;
        out.push_back(&vortex_driver);
#endif
        return out;
    }();
    return drivers;
}
} // namespace

std::vector<std::string> SupportedFormatNames()
{
    std::vector<std::string> names;
    const auto & drivers = EnabledDrivers();
    names.reserve(drivers.size());
    for (const auto * driver : drivers)
    {
        names.emplace_back(driver->Name());
    }
    return names;
}

void RegisterFormatCliOptions(po::options_description & desc)
{
    for (const auto * driver : EnabledDrivers())
    {
        driver->RegisterCliOptions(desc);
    }
}

arrow::Result<const IFormatDriver *> ResolveFormatDriver(std::string_view format_name)
{
    const auto requested = ToLower(std::string(format_name));
    for (const auto * driver : EnabledDrivers())
    {
        if (requested == driver->Name())
        {
            return driver;
        }
    }

    std::string supported;
    const auto names = SupportedFormatNames();
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        if (i > 0)
        {
            supported += ", ";
        }
        supported += names[i];
    }
    if (supported.empty())
    {
        supported = "none";
    }
    return arrow::Status::Invalid("Unsupported output format: ", std::string(format_name), ". Supported formats: ", supported);
}
