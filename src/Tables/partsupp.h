#pragma once

#include <cstdint>
#include <string_view>

#include "Common/generator.h"
#include "Common/rands.h"
#include "Tables/table.h"

struct PartSuppRow
{
    std::int32_t ps_partkey = 0;
    std::int32_t ps_suppkey = 0;
    std::int32_t ps_availqty = 0;
    std::int32_t ps_supplycost_cents = 0;
    double ps_supplycost = 0.0;
    std::string_view ps_comment;
};

class PartSuppRowIterator
{
public:
    void Reset(std::uint64_t start_part, std::uint64_t end_part, double scale_factor, const TextPool & text_pool);
    bool Next(PartSuppRow * out);
    bool Done() const;
    std::uint64_t NextRowId() const;

private:
    static std::int32_t SelectPartSupplier(std::int32_t part_key, std::int32_t supplier_number, double scale_factor);

    static constexpr std::int32_t kSuppliersPerPart = 4;
    static constexpr std::int32_t kAvailableQuantityMin = 1;
    static constexpr std::int32_t kAvailableQuantityMax = 9'999;
    static constexpr std::int32_t kSupplyCostMin = 100;
    static constexpr std::int32_t kSupplyCostMax = 100'000;
    static constexpr std::int32_t kCommentAverageLength = 124;
    static constexpr std::int32_t kSupplierScaleBase = 10'000;

    static constexpr std::int64_t kAvailableQuantitySeed = 1'671'059'989;
    static constexpr std::int64_t kSupplyCostSeed = 1'051'288'424;
    static constexpr std::int64_t kCommentSeed = 1'961'692'154;

    std::uint64_t next_part_ = 0;
    std::uint64_t end_part_ = 0;
    std::int32_t part_supplier_number_ = 0;
    double scale_factor_ = 1.0;

    RandomBoundedInt available_quantity_random_{};
    RandomBoundedInt supply_cost_random_{};
    RandomText comment_random_{};
};

class PartSuppGenerator final : public ITableGenerator
{
public:
    explicit PartSuppGenerator(arrow::MemoryPool * pool);
    const TableMetadata & GetTableMetadata() const override;
    void Reset(const GeneratorContext & ctx) override;
    bool NextBatch(std::uint64_t max_rows, TableBatch * out) override;

private:
    GeneratorContext ctx_{};
    BuilderFactory factory_;
    PartSuppColumns columns_;
    PartSuppRowIterator row_iter_;
};
