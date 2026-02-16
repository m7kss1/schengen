#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "Common/generator.h"
#include "Common/rands.h"
#include "Tables/table.h"

struct RegionRow
{
    std::int32_t r_regionkey = 0;
    std::string_view r_name;
    std::string_view r_comment;
};

class RegionRowIterator
{
public:
    void Reset(std::uint64_t start_row, std::uint64_t end_row, const TextPool & text_pool);
    bool Next(RegionRow * out);
    bool Done() const;
    std::uint64_t NextRowId() const;

private:
    static constexpr std::array<const char *, 5> kRegionNames = {
        "AFRICA",
        "AMERICA",
        "ASIA",
        "EUROPE",
        "MIDDLE EAST",
    };
    static constexpr std::int64_t kCommentSeed = 1500869201;
    static constexpr std::int32_t kCommentAverageLength = 72;
    static constexpr std::uint64_t kRegionCount = kRegionNames.size();

    std::uint64_t next_row_ = 0;
    std::uint64_t end_row_ = 0;
    RandomText comment_random_;
};

class RegionGenerator final : public ITableGenerator
{
public:
    explicit RegionGenerator(arrow::MemoryPool * pool);
    const TableMetadata & GetTableMetadata() const override;
    void Reset(const GeneratorContext & ctx) override;
    bool NextBatch(std::uint64_t max_rows, TableBatch * out) override;

private:
    GeneratorContext ctx_{};
    BuilderFactory factory_;
    RegionColumns columns_;
    RegionRowIterator row_iter_;
};
