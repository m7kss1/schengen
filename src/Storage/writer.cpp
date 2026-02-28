#include "Storage/writer.h"

#include "Common/partition.h"
#include "Storage/parquet_writer.h"

#if defined(ENABLE_VORTEX)
#    include "Storage/vortex_writer.h"
#endif

#include <algorithm>
#include <limits>

std::int32_t ResolveWriterPartCount(const TableMetadata & table, const ScaleConfig & scale, const WriterOptions & options)
{
    if (options.format != OutputFormat::Parquet)
    {
        return 1;
    }

    const auto & parquet_options = ParquetTableWriter::ResolveOptions(options);
    const auto row_group_rows = ParquetTableWriter::ResolveRowGroupRows(table, parquet_options);
    if (row_group_rows <= 0)
    {
        return 1;
    }

    const auto total_rows = scale.RowCount(table);
    const auto row_group_rows_u = static_cast<std::uint64_t>(row_group_rows);
    const auto parts_u = (total_rows + row_group_rows_u - 1) / row_group_rows_u;
    const auto max_parts = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    return static_cast<std::int32_t>(std::min(parts_u, max_parts));
}

std::unique_ptr<ITableWriter> MakeTableWriter(OutputFormat format)
{
    switch (format)
    {
        case OutputFormat::Parquet:
            return std::make_unique<ParquetTableWriter>();
#if defined(ENABLE_VORTEX)
        case OutputFormat::Vortex:
            return std::make_unique<VortexTableWriter>();
#endif
        default:
            return nullptr;
    }
}
