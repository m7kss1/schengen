#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "Common/dates.h"
#include "Common/distribution.h"
#include "Common/generator.h"
#include "Common/rands.h"
#include "Tables/table.h"

struct OrderRow
{
    std::int32_t o_orderkey = 0;
    std::int32_t o_custkey = 0;
    std::string_view o_orderstatus;
    std::int32_t o_totalprice_cents = 0;
    double o_totalprice = 0.0;
    /* 
     * Order date stored and encoded 
     * as TPCH-like date (YYDDD)
     * For more: Check out TPCHDate comments
     */
    std::int32_t o_orderdate = 1;
    std::int32_t o_orderdate_epoch = 0;
    std::string_view o_orderpriority;
    std::string_view o_clerk;
    std::int32_t o_shippriority = 0;
    std::string_view o_comment;
};

class OrderRowIterator
{
public:
    void Reset(std::uint64_t start_row, std::uint64_t end_row, double scale_factor, const TextPool & text_pool);
    bool Next(OrderRow * out);
    bool Done() const;
    std::uint64_t NextRowId() const;

private:
    static RandomBoundedInt CreateOrderDateRandom();
    static RandomBoundedInt CreateLineCountRandom();
    static RandomBoundedInt CreateQuantityRandom();
    static RandomBoundedInt CreateDiscountRandom();
    static RandomBoundedInt CreateTaxRandom();
    static RandomBoundedLong CreatePartKeyRandom(double scale_factor);
    static RandomBoundedInt CreateShipDateRandom();
    static std::int64_t MakeOrderKey(std::int64_t order_index);
    static std::int64_t CalculatePartPrice(std::int64_t part_key);
    static std::string FormatClerk(std::int32_t clerk_id);

    static constexpr std::int32_t kCustomerMortality = 3;
    static constexpr std::int32_t kClerkScaleBase = 1000;
    static constexpr std::int32_t kLineCountMin = 1;
    static constexpr std::int32_t kLineCountMax = 7;
    static constexpr std::int32_t kQuantityMin = 1;
    static constexpr std::int32_t kQuantityMax = 50;
    static constexpr std::int32_t kDiscountMin = 0;
    static constexpr std::int32_t kDiscountMax = 10;
    static constexpr std::int32_t kTaxMin = 0;
    static constexpr std::int32_t kTaxMax = 8;
    static constexpr std::int32_t kPartKeyMin = 1;
    static constexpr std::int32_t kShipDateMin = 1;
    static constexpr std::int32_t kShipDateMax = 121;
    static constexpr std::int32_t kReceiptDateMax = 30;
    static constexpr std::int32_t kItemShipDays = kShipDateMax + kReceiptDateMax;
    static constexpr std::int32_t kOrderDateMin = TPCHDate::kMinGenerateDate;
    static constexpr std::int32_t kOrderDateMax = kOrderDateMin + (TPCHDate::kTotalDateRange - kItemShipDays - 1);
    static constexpr std::int32_t kCommentAverageLength = 49;
    static constexpr std::int32_t kOrderKeySparseBits = 2;
    static constexpr std::int32_t kOrderKeySparseKeep = 3;
    static constexpr std::int64_t kOrderDateSeed = 1'066'728'069;
    static constexpr std::int64_t kLineCountSeed = 1'434'868'289;
    static constexpr std::int64_t kCustomerKeySeed = 851'767'375;
    static constexpr std::int64_t kOrderPrioritySeed = 591'449'447;
    static constexpr std::int64_t kClerkSeed = 1'171'034'773;
    static constexpr std::int64_t kCommentSeed = 276'090'261;
    static constexpr std::int64_t kLineQuantitySeed = 209'208'115;
    static constexpr std::int64_t kLineDiscountSeed = 554'590'007;
    static constexpr std::int64_t kLineTaxSeed = 721'958'466;
    static constexpr std::int64_t kLinePartKeySeed = 1'808'217'256;
    static constexpr std::int64_t kLineShipDateSeed = 1'769'349'045;
    static constexpr std::string_view kStatusFulfilled = "F";
    static constexpr std::string_view kStatusPending = "P";
    static constexpr std::string_view kStatusOpen = "O";

    std::uint64_t next_row_ = 0;
    std::uint64_t end_row_ = 0;
    double scale_factor_ = 1.0;
    std::int64_t max_customer_key_ = 0;

    RandomBoundedInt order_date_random_{};
    RandomBoundedInt line_count_random_{};
    RandomBoundedLong customer_key_random_{};
    RandomString order_priority_random_{};
    RandomBoundedInt clerk_random_{};
    RandomText comment_random_{};

    RandomBoundedInt line_quantity_random_{};
    RandomBoundedInt line_discount_random_{};
    RandomBoundedInt line_tax_random_{};
    RandomBoundedLong line_part_key_random_{};
    RandomBoundedInt line_ship_date_random_{};

    std::string clerk_buffer_;
};

class OrderGenerator final : public ITableGenerator
{
public:
    explicit OrderGenerator(arrow::MemoryPool * pool);
    const TableMetadata & GetTableMetadata() const override;
    void Reset(const GeneratorContext & ctx) override;
    bool NextBatch(std::uint64_t max_rows, TableBatch * out) override;

private:
    GeneratorContext ctx_{};
    BuilderFactory factory_;
    OrdersColumns columns_;
    OrderRowIterator row_iter_;
};
