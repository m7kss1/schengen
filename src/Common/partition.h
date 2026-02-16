#pragma once

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "Tables/table.h"

struct ScaleConfig
{
    double factor = 1.0;

    /* Estimate the number of rows after scaling from sf-1 */
    auto RowCount(const TableMetadata & table) const -> std::uint64_t;
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

void ValidatePart(std::int32_t part_num, std::int32_t part_count);

/* Calculate the number of rows for a given partition */
std::uint64_t CalculateRowCount(std::uint64_t total_rows, std::int32_t part_num, std::int32_t part_count);

/* Calculate the starting row index for a given partition */
std::uint64_t CalculateStartIndex(std::uint64_t total_rows, std::int32_t part_num, std::int32_t part_count);

PartitionRange MakePartitionRange(std::uint64_t total_rows, std::int32_t part_num, std::int32_t part_count);

PartitionPlan MakePartitionPlan(const TableMetadata & table, const ScaleConfig & scale, std::int32_t part_num, std::int32_t part_count);

std::vector<PartitionPlan> BuildPartitionPlans(const TableMetadata & table, const ScaleConfig & scale, std::int32_t part_count);
