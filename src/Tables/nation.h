#pragma once

#include <cstdint>
#include <string_view>

#include "Common/distribution.h"
#include "Common/generator.h"
#include "Common/rands.h"
#include "Tables/table.h"

struct NationRow
{
    std::int32_t n_nationkey = 0;
    std::string_view n_name;
    std::int32_t n_regionkey = 0;
    std::string_view n_comment;
};

class NationRowIterator
{
public:
    void Reset(std::uint64_t start_row, std::uint64_t end_row, const TextPool & text_pool);
    bool Next(NationRow * out);
    bool Done() const;
    std::uint64_t NextRowId() const;

private:
    static constexpr std::int32_t kCommentAverageLength = 72;
    static constexpr std::int64_t kCommentSeed = 606179079;
    static constexpr std::uint64_t kNationCount = kNations.size();

    std::uint64_t next_row_ = 0;
    std::uint64_t end_row_ = 0;
    std::int32_t region_cum_sum_ = 0;
    RandomText comment_random_;
};

class NationGenerator final : public ITableGenerator
{
public:
    explicit NationGenerator(arrow::MemoryPool * pool);
    const TableMetadata & GetTableMetadata() const override;
    void Reset(const GeneratorContext & ctx) override;
    bool NextBatch(std::uint64_t max_rows, TableBatch * out) override;

private:
    GeneratorContext ctx_{};
    BuilderFactory factory_;
    NationColumns columns_;
    NationRowIterator row_iter_;
};
