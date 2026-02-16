#pragma once

#include <cstdint>
#include <string_view>

#include "Common/dates.h"
#include "Common/distribution.h"
#include "Common/generator.h"
#include "Common/rands.h"
#include "Tables/table.h"

struct LineitemRow
{
    std::int32_t l_orderkey = 0;
    std::int32_t l_partkey = 0;
    std::int32_t l_suppkey = 0;
    std::int32_t l_linenumber = 0;
    std::int32_t l_quantity = 0;
    std::int32_t l_extendedprice_cents = 0;
    double l_extendedprice = 0.0;
    std::int32_t l_discount_percent = 0;
    double l_discount = 0.0;
    std::int32_t l_tax_percent = 0;
    double l_tax = 0.0;
    std::string_view l_returnflag;
    std::string_view l_linestatus;
    std::int32_t l_shipdate = 0;
    std::int32_t l_shipdate_epoch = 0;
    std::int32_t l_commitdate = 0;
    std::int32_t l_commitdate_epoch = 0;
    std::int32_t l_receiptdate = 0;
    std::int32_t l_receiptdate_epoch = 0;
    std::string_view l_shipinstruct;
    std::string_view l_shipmode;
    std::string_view l_comment;
};

class LineitemRowIterator
{
public:
    void Reset(std::uint64_t start_order, std::uint64_t end_order, double scale_factor, const TextPool & text_pool);
    bool Next(LineitemRow * out);
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
    static std::int32_t SelectPartSupplier(std::int64_t part_key, std::int32_t supplier_number, double scale_factor);

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
    static constexpr std::int32_t kCommitDateMin = 30;
    static constexpr std::int32_t kCommitDateMax = 90;
    static constexpr std::int32_t kReceiptDateMin = 1;
    static constexpr std::int32_t kReceiptDateMax = 30;
    static constexpr std::int32_t kCommentAverageLength = 27;
    static constexpr std::int32_t kSuppliersPerPart = 4;
    static constexpr std::int32_t kSupplierScaleBase = 10'000;
    static constexpr std::int32_t kOrderKeySparseBits = 2;
    static constexpr std::int32_t kOrderKeySparseKeep = 3;
    static constexpr std::int32_t kOrderDateMin = TPCHDate::kMinGenerateDate;
    static constexpr std::int32_t kItemShipDays = kShipDateMax + kReceiptDateMax;
    static constexpr std::int32_t kOrderDateMax = kOrderDateMin + (TPCHDate::kTotalDateRange - kItemShipDays - 1);

    static constexpr std::int64_t kOrderDateSeed = 1'066'728'069;
    static constexpr std::int64_t kLineCountSeed = 1'434'868'289;
    static constexpr std::int64_t kLineQuantitySeed = 209'208'115;
    static constexpr std::int64_t kLineDiscountSeed = 554'590'007;
    static constexpr std::int64_t kLineTaxSeed = 721'958'466;
    static constexpr std::int64_t kLinePartKeySeed = 1'808'217'256;
    static constexpr std::int64_t kSupplierNumberSeed = 2'095'021'727;
    static constexpr std::int64_t kLineShipDateSeed = 1'769'349'045;
    static constexpr std::int64_t kCommitDateSeed = 904'914'315;
    static constexpr std::int64_t kReceiptDateSeed = 373'135'028;
    static constexpr std::int64_t kReturnFlagSeed = 717'419'739;
    static constexpr std::int64_t kShipInstructSeed = 1'371'272'478;
    static constexpr std::int64_t kShipModeSeed = 675'466'456;
    static constexpr std::int64_t kCommentSeed = 1'095'462'486;

    static constexpr std::string_view kReturnFlagNone = "N";
    static constexpr std::string_view kLineStatusFulfilled = "F";
    static constexpr std::string_view kLineStatusOpen = "O";

    std::uint64_t start_order_ = 0;
    std::uint64_t end_order_ = 0;
    std::uint64_t order_count_ = 0;
    std::uint64_t index_ = 0;
    std::int32_t order_date_ = 0;
    std::int32_t line_count_ = 0;
    std::int32_t line_number_ = 0;
    double scale_factor_ = 1.0;

    RandomBoundedInt order_date_random_{};
    RandomBoundedInt line_count_random_{};
    RandomBoundedInt quantity_random_{};
    RandomBoundedInt discount_random_{};
    RandomBoundedInt tax_random_{};
    RandomBoundedLong part_key_random_{};
    RandomBoundedInt supplier_number_random_{};
    RandomBoundedInt ship_date_random_{};
    RandomBoundedInt commit_date_random_{};
    RandomBoundedInt receipt_date_random_{};
    RandomString return_flag_random_{};
    RandomString ship_instruct_random_{};
    RandomString ship_mode_random_{};
    RandomText comment_random_{};
};

class LineitemGenerator final : public ITableGenerator
{
public:
    explicit LineitemGenerator(arrow::MemoryPool * pool);
    const TableMetadata & GetTableMetadata() const override;
    void Reset(const GeneratorContext & ctx) override;
    bool NextBatch(std::uint64_t max_rows, TableBatch * out) override;

private:
    GeneratorContext ctx_{};
    BuilderFactory factory_;
    LineitemColumns columns_;
    LineitemRowIterator row_iter_;
};
