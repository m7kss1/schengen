#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>
#include <cassert>

#include "table.h"

struct ScaleConfig
{
    double factor = 1.0;

    /* Estimate the number of rows after scaling from sf-1 */
    auto RowCount(const TableMetadata & table) const
    {
        return static_cast<std::uint64_t>(static_cast<double>(table.rows_at_sf1) * factor);
    }
};

/* Represent partition range: [m_start, m_end) */
struct PartitionRange
{
    std::uint64_t start_row = 0;
    std::uint64_t end_row = 0;
};

struct PartitionPlan
{
    const TableMetadata * table = nullptr;
    PartitionRange range{};
    /* 1-indexed partition number */
    std::int32_t part_num = 1;
    std::int32_t part_count = 1;
};

inline void ValidatePart(std::int32_t part_num, std::int32_t part_count)
{
    if (part_count < 1) {
        throw std::invalid_argument("part_count must be >= 1");
    }
    if (part_num < 1 || part_num > part_count) {
        throw std::invalid_argument("part must be within [1, part_count]");
    }
}

/* Calculate the number of rows for a given partition */
inline std::uint64_t CalculateRowCount(std::uint64_t total_rows,
                                       std::int32_t part_num,
                                       std::int32_t part_count)
{
    ValidatePart(part_num, part_count);
    std::uint64_t row_count = total_rows / (1UL * part_count);
    if (part_num == part_count) {
        row_count += total_rows % (1UL * part_count);
    }
    return row_count;
}

/* Calculate the starting row index for a given partition */
inline std::uint64_t CalculateStartIndex(std::uint64_t total_rows,
                                         std::int32_t part_num,
                                         std::int32_t part_count)
{
    ValidatePart(part_num, part_count);
    const std::uint64_t rows_per_part = total_rows / (1UL * part_count);
    return rows_per_part * static_cast<std::uint64_t>(part_num - 1);
}

inline PartitionRange MakePartitionRange(std::uint64_t total_rows,
                                         std::int32_t part_num,
                                         std::int32_t part_count)
{
    const auto start = CalculateStartIndex(total_rows, part_num, part_count);
    const auto count = CalculateRowCount(total_rows, part_num, part_count);
    return PartitionRange{start, start + count};
}

inline PartitionPlan MakePartitionPlan(const TableMetadata & table,
                                       const ScaleConfig & scale,
                                       std::int32_t part_num,
                                       std::int32_t part_count)
{
    const auto total_rows = scale.RowCount(table);
    return PartitionPlan{
        .table = &table,
        .range = MakePartitionRange(total_rows, part_num, part_count),
        .part_num = part_num,
        .part_count = part_count,
    };
}

inline std::vector<PartitionPlan> BuildPartitionPlans(const TableMetadata & table,
                                                      const ScaleConfig & scale,
                                                      std::int32_t part_count)
{
    std::vector<PartitionPlan> plans;
    plans.reserve(1UL * part_count);
    for (std::int32_t part = 1; part <= part_count; ++part) {
        plans.emplace_back(MakePartitionPlan(table, scale, part, part_count));
    }

    return plans;
}
