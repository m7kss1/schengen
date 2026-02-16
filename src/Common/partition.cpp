#include "Common/partition.h"

#include <stdexcept>

auto ScaleConfig::RowCount(const TableMetadata & table) const -> std::uint64_t
{
    return static_cast<std::uint64_t>(static_cast<double>(table.rows_at_sf1) * factor);
}

void ValidatePart(std::int32_t part_num, std::int32_t part_count)
{
    if (part_count < 1)
    {
        throw std::invalid_argument("part_count must be >= 1");
    }
    if (part_num < 1 || part_num > part_count)
    {
        throw std::invalid_argument("part must be within [1, part_count]");
    }
}

std::uint64_t CalculateRowCount(std::uint64_t total_rows, std::int32_t part_num, std::int32_t part_count)
{
    ValidatePart(part_num, part_count);
    std::uint64_t row_count = total_rows / (1UL * part_count);
    if (part_num == part_count)
    {
        row_count += total_rows % (1UL * part_count);
    }
    return row_count;
}

std::uint64_t CalculateStartIndex(std::uint64_t total_rows, std::int32_t part_num, std::int32_t part_count)
{
    ValidatePart(part_num, part_count);
    const std::uint64_t rows_per_part = total_rows / (1UL * part_count);
    return rows_per_part * static_cast<std::uint64_t>(part_num - 1);
}

PartitionRange MakePartitionRange(std::uint64_t total_rows, std::int32_t part_num, std::int32_t part_count)
{
    const auto start = CalculateStartIndex(total_rows, part_num, part_count);
    const auto count = CalculateRowCount(total_rows, part_num, part_count);
    return PartitionRange{start, start + count};
}

PartitionPlan MakePartitionPlan(const TableMetadata & table, const ScaleConfig & scale, std::int32_t part_num, std::int32_t part_count)
{
    const auto total_rows = scale.RowCount(table);
    return PartitionPlan{
        .table = &table,
        .range = MakePartitionRange(total_rows, part_num, part_count),
        .part_num = part_num,
        .part_count = part_count,
    };
}

std::vector<PartitionPlan> BuildPartitionPlans(const TableMetadata & table, const ScaleConfig & scale, std::int32_t part_count)
{
    std::vector<PartitionPlan> plans;
    plans.reserve(1UL * part_count);
    for (std::int32_t part = 1; part <= part_count; ++part)
    {
        plans.emplace_back(MakePartitionPlan(table, scale, part, part_count));
    }
    return plans;
}
