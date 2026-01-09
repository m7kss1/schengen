#pragma once

#include <cstdint>
#include <cstdio>
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
    /* Used for Arrow::date32 */
    std::int32_t o_orderdate_epoch = 0;
    std::string_view o_orderpriority;
    std::string_view o_clerk;
    std::int32_t o_shippriority = 0;
    std::string_view o_comment;
};

class OrderRowIterator
{
public:
    void Reset(std::uint64_t start_row, std::uint64_t end_row, double scale_factor, const TextPool & text_pool)
    {
        next_row_ = start_row;
        end_row_ = end_row;
        if (next_row_ > end_row_)
        {
            next_row_ = end_row_;
        }

        scale_factor_ = scale_factor;

        order_date_random_ = CreateOrderDateRandom();
        line_count_random_ = CreateLineCountRandom();

        max_customer_key_ = static_cast<std::int64_t>(static_cast<double>(kCustomer.rows_at_sf1) * scale_factor);
        if (max_customer_key_ < 1)
        {
            max_customer_key_ = 1;
        }

        customer_key_random_ = RandomBoundedLong(kCustomerKeySeed, scale_factor >= 30000.0, 1, max_customer_key_);

        order_priority_random_ = RandomString(kOrderPrioritySeed, kOrderPrioritiesDist);

        const std::int32_t max_clerk = std::max(static_cast<std::int32_t>(scale_factor * kClerkScaleBase), kClerkScaleBase);
        clerk_random_ = RandomBoundedInt(kClerkSeed, 1, max_clerk);

        comment_random_ = RandomText(kCommentSeed, text_pool, static_cast<double>(kCommentAverageLength));

        line_quantity_random_ = CreateQuantityRandom();
        line_discount_random_ = CreateDiscountRandom();
        line_tax_random_ = CreateTaxRandom();
        line_part_key_random_ = CreatePartKeyRandom(scale_factor);
        line_ship_date_random_ = CreateShipDateRandom();

        if (next_row_ > 0)
        {
            const auto rows = static_cast<std::int64_t>(next_row_);
            order_date_random_.AdvanceRows(rows);
            line_count_random_.AdvanceRows(rows);
            customer_key_random_.AdvanceRows(rows);
            order_priority_random_.AdvanceRows(rows);
            clerk_random_.AdvanceRows(rows);
            comment_random_.AdvanceRows(rows);

            line_quantity_random_.AdvanceRows(rows);
            line_discount_random_.AdvanceRows(rows);
            line_tax_random_.AdvanceRows(rows);
            line_part_key_random_.AdvanceRows(rows);
            line_ship_date_random_.AdvanceRows(rows);
        }
    }

    bool Next(OrderRow * out)
    {
        if (next_row_ >= end_row_)
        {
            return false;
        }

        const std::int64_t order_index = static_cast<std::int64_t>(next_row_ + 1);
        const std::int64_t order_key = MakeOrderKey(order_index);

        const std::int32_t order_date = order_date_random_.NextValue();

        std::int64_t customer_key = customer_key_random_.NextValue();
        std::int64_t delta = 1;
        while (customer_key % kCustomerMortality == 0)
        {
            customer_key += delta;
            if (customer_key > max_customer_key_)
            {
                customer_key = max_customer_key_;
            }
            delta *= -1;
        }

        std::int64_t total_price = 0;
        std::int32_t shipped_count = 0;

        const std::int32_t line_count = line_count_random_.NextValue();
        for (std::int32_t i = 0; i < line_count; ++i)
        {
            const std::int32_t quantity = line_quantity_random_.NextValue();
            const std::int32_t discount = line_discount_random_.NextValue();
            const std::int32_t tax = line_tax_random_.NextValue();

            const std::int64_t part_key = line_part_key_random_.NextValue();
            const std::int64_t part_price = CalculatePartPrice(part_key);
            const std::int64_t extended_price = part_price * quantity;
            const std::int64_t discounted_price = extended_price * (100 - discount);
            total_price += ((discounted_price / 100) * (100 + tax)) / 100;

            const std::int32_t ship_date = line_ship_date_random_.NextValue() + order_date;
            if (TPCHDate::IsInPast(ship_date))
            {
                ++shipped_count;
            }
        }

        const std::string_view order_status
            = shipped_count == line_count ? kStatusFulfilled : (shipped_count > 0 ? kStatusPending : kStatusOpen);

        const std::int32_t clerk_id = clerk_random_.NextValue();
        clerk_buffer_ = FormatClerk(clerk_id);

        if (out != nullptr)
        {
            out->o_orderkey = static_cast<std::int32_t>(order_key);
            out->o_custkey = static_cast<std::int32_t>(customer_key);
            out->o_orderstatus = order_status;
            out->o_totalprice_cents = static_cast<std::int32_t>(total_price);
            out->o_totalprice = static_cast<double>(total_price) / 100.0;
            out->o_orderdate = order_date;
            out->o_orderdate_epoch = TPCHDate::ToUnixEpoch(order_date);
            out->o_orderpriority = order_priority_random_.NextValue();
            out->o_clerk = std::string_view(clerk_buffer_);
            out->o_shippriority = 0;
            out->o_comment = comment_random_.NextValue();
        }

        order_date_random_.RowFinished();
        line_count_random_.RowFinished();
        customer_key_random_.RowFinished();
        order_priority_random_.RowFinished();
        clerk_random_.RowFinished();
        comment_random_.RowFinished();

        line_quantity_random_.RowFinished();
        line_discount_random_.RowFinished();
        line_tax_random_.RowFinished();
        line_part_key_random_.RowFinished();
        line_ship_date_random_.RowFinished();

        ++next_row_;
        return true;
    }

    bool Done() const { return next_row_ >= end_row_; }
    std::uint64_t NextRowId() const { return next_row_; }

private:
    static RandomBoundedInt CreateOrderDateRandom() { return RandomBoundedInt(kOrderDateSeed, kOrderDateMin, kOrderDateMax); }

    static RandomBoundedInt CreateLineCountRandom() { return RandomBoundedInt(kLineCountSeed, kLineCountMin, kLineCountMax); }

    static RandomBoundedInt CreateQuantityRandom()
    {
        return RandomBoundedInt(kLineQuantitySeed, kQuantityMin, kQuantityMax, kLineCountMax);
    }

    static RandomBoundedInt CreateDiscountRandom()
    {
        return RandomBoundedInt(kLineDiscountSeed, kDiscountMin, kDiscountMax, kLineCountMax);
    }

    static RandomBoundedInt CreateTaxRandom() { return RandomBoundedInt(kLineTaxSeed, kTaxMin, kTaxMax, kLineCountMax); }

    static RandomBoundedLong CreatePartKeyRandom(double scale_factor)
    {
        const std::int64_t max_part_key = static_cast<std::int64_t>(static_cast<double>(kPart.rows_at_sf1) * scale_factor);
        return RandomBoundedLong(kLinePartKeySeed, scale_factor >= 30000.0, kPartKeyMin, max_part_key, kLineCountMax);
    }

    static RandomBoundedInt CreateShipDateRandom()
    {
        return RandomBoundedInt(kLineShipDateSeed, kShipDateMin, kShipDateMax, kLineCountMax);
    }

    static std::int64_t MakeOrderKey(std::int64_t order_index)
    {
        const std::int64_t low_bits = order_index & ((1LL << kOrderKeySparseKeep) - 1);
        std::int64_t ok = order_index;
        ok >>= kOrderKeySparseKeep;
        ok <<= kOrderKeySparseBits;
        ok <<= kOrderKeySparseKeep;
        ok += low_bits;
        return ok;
    }

    static std::int64_t CalculatePartPrice(std::int64_t part_key)
    {
        std::int64_t price = 90000;
        price += (part_key / 10) % 20001;
        price += (part_key % 1000) * 100;
        return price;
    }

    static std::string FormatClerk(std::int32_t clerk_id)
    {
        char buffer[32];
        const int n = std::snprintf(buffer, sizeof(buffer), "Clerk#%09d", clerk_id);
        return std::string(buffer, buffer + (n > 0 ? n : 0));
    }

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
    /*
     * Anchored by the calendar window minus the
     * maximum shipping/receipt offsets. It does not slide with scale factor—no matter
     * how many rows we generate, the last permissible order date remains the same 
     */
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
    explicit OrderGenerator(arrow::MemoryPool * pool)
        : factory_(pool)
        , columns_(factory_)
    {
    }

    const TableMetadata & GetTableMetadata() const override { return kOrders; }

    void Reset(const GeneratorContext & ctx) override
    {
        ctx_ = ctx;
        columns_.ClearAll();

        const TextPool & text_pool = ctx.text_pool != nullptr ? *ctx.text_pool : TextPool::Default();

        row_iter_.Reset(ctx.partition.range.start_row, ctx.partition.range.end_row, ctx.scale.factor, text_pool);
    }

    bool NextBatch(std::uint64_t max_rows, TableBatch * out) override
    {
        if (max_rows == 0 || row_iter_.Done())
        {
            return false;
        }

        columns_.ClearAll();

        const std::uint64_t batch_start = row_iter_.NextRowId();
        std::uint64_t batch_count = 0;

        OrderRow row;
        while (batch_count < max_rows && row_iter_.Next(&row))
        {
            columns_.o_orderkey.Append(row.o_orderkey);
            columns_.o_custkey.Append(row.o_custkey);
            columns_.o_orderstatus.Append(row.o_orderstatus);
            columns_.o_totalprice.Append(row.o_totalprice);
            columns_.o_orderdate.Append(row.o_orderdate_epoch);
            columns_.o_orderpriority.Append(row.o_orderpriority);
            columns_.o_clerk.Append(row.o_clerk);
            columns_.o_shippriority.Append(row.o_shippriority);
            columns_.o_comment.Append(row.o_comment);
            ++batch_count;
        }

        if (out != nullptr)
        {
            out->metadata = &kOrders;
            out->first_row_id = batch_start;
            out->row_count = batch_count;
            out->columns = {
                columns_.o_orderkey.Finish(),
                columns_.o_custkey.Finish(),
                columns_.o_orderstatus.Finish(),
                columns_.o_totalprice.Finish(),
                columns_.o_orderdate.Finish(),
                columns_.o_orderpriority.Finish(),
                columns_.o_clerk.Finish(),
                columns_.o_shippriority.Finish(),
                columns_.o_comment.Finish(),
            };
        }

        return true;
    }

private:
    GeneratorContext ctx_{};
    BuilderFactory factory_;
    OrdersColumns columns_;
    OrderRowIterator row_iter_;
};
