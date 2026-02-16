#include "Tables/lineitem.h"

#include "Common/partition.h"

void LineitemRowIterator::Reset(std::uint64_t start_order, std::uint64_t end_order, double scale_factor, const TextPool & text_pool)
{
    start_order_ = start_order;
    end_order_ = end_order;
    if (start_order_ > end_order_)
    {
        start_order_ = end_order_;
    }

    scale_factor_ = scale_factor;
    order_count_ = end_order_ - start_order_;
    index_ = 0;
    line_number_ = 0;

    order_date_random_ = CreateOrderDateRandom();
    line_count_random_ = CreateLineCountRandom();

    quantity_random_ = CreateQuantityRandom();
    discount_random_ = CreateDiscountRandom();
    tax_random_ = CreateTaxRandom();

    part_key_random_ = CreatePartKeyRandom(scale_factor);
    supplier_number_random_ = RandomBoundedInt(kSupplierNumberSeed, 0, kSuppliersPerPart - 1, kLineCountMax);

    ship_date_random_ = CreateShipDateRandom();
    commit_date_random_ = RandomBoundedInt(kCommitDateSeed, kCommitDateMin, kCommitDateMax, kLineCountMax);
    receipt_date_random_ = RandomBoundedInt(kReceiptDateSeed, kReceiptDateMin, kReceiptDateMax, kLineCountMax);

    return_flag_random_ = RandomString(kReturnFlagSeed, kReturnFlagsDist, kLineCountMax);
    ship_instruct_random_ = RandomString(kShipInstructSeed, kShipInstructionsDist, kLineCountMax);
    ship_mode_random_ = RandomString(kShipModeSeed, kShipModesDist, kLineCountMax);
    comment_random_ = RandomText(kCommentSeed, text_pool, static_cast<double>(kCommentAverageLength), kLineCountMax);

    if (start_order_ > 0)
    {
        const auto rows = static_cast<std::int64_t>(start_order_);
        order_date_random_.AdvanceRows(rows);
        line_count_random_.AdvanceRows(rows);

        quantity_random_.AdvanceRows(rows);
        discount_random_.AdvanceRows(rows);
        tax_random_.AdvanceRows(rows);

        part_key_random_.AdvanceRows(rows);
        supplier_number_random_.AdvanceRows(rows);

        ship_date_random_.AdvanceRows(rows);
        commit_date_random_.AdvanceRows(rows);
        receipt_date_random_.AdvanceRows(rows);

        return_flag_random_.AdvanceRows(rows);
        ship_instruct_random_.AdvanceRows(rows);
        ship_mode_random_.AdvanceRows(rows);
        comment_random_.AdvanceRows(rows);
    }

    if (order_count_ > 0)
    {
        order_date_ = order_date_random_.NextValue();
        line_count_ = line_count_random_.NextValue() - 1;
    }
}

bool LineitemRowIterator::Next(LineitemRow * out)
{
    if (index_ >= order_count_)
    {
        return false;
    }

    const std::int64_t order_index = static_cast<std::int64_t>(start_order_ + index_ + 1);
    const std::int64_t order_key = MakeOrderKey(order_index);

    const std::int32_t quantity = quantity_random_.NextValue();
    const std::int32_t discount = discount_random_.NextValue();
    const std::int32_t tax = tax_random_.NextValue();

    const std::int64_t part_key = part_key_random_.NextValue();
    const std::int32_t supplier_number = supplier_number_random_.NextValue();
    const std::int32_t supplier_key = SelectPartSupplier(part_key, supplier_number, scale_factor_);

    const std::int64_t part_price = CalculatePartPrice(part_key);
    const std::int64_t extended_price_cents = part_price * quantity;

    std::int32_t ship_date = ship_date_random_.NextValue();
    ship_date += order_date_;
    std::int32_t commit_date = commit_date_random_.NextValue();
    commit_date += order_date_;
    std::int32_t receipt_date = receipt_date_random_.NextValue();
    receipt_date += ship_date;

    const std::string_view return_flag = TPCHDate::IsInPast(receipt_date) ? return_flag_random_.NextValue() : kReturnFlagNone;
    const std::string_view line_status = TPCHDate::IsInPast(ship_date) ? kLineStatusFulfilled : kLineStatusOpen;
    const std::string_view ship_instruct = ship_instruct_random_.NextValue();
    const std::string_view ship_mode = ship_mode_random_.NextValue();
    const std::string_view comment = comment_random_.NextValue();

    if (out != nullptr)
    {
        out->l_orderkey = static_cast<std::int32_t>(order_key);
        out->l_partkey = static_cast<std::int32_t>(part_key);
        out->l_suppkey = supplier_key;
        out->l_linenumber = line_number_ + 1;
        out->l_quantity = quantity;
        out->l_extendedprice_cents = static_cast<std::int32_t>(extended_price_cents);
        out->l_extendedprice = static_cast<double>(extended_price_cents) / 100.0;
        out->l_discount_percent = discount;
        out->l_discount = static_cast<double>(discount) / 100.0;
        out->l_tax_percent = tax;
        out->l_tax = static_cast<double>(tax) / 100.0;
        out->l_returnflag = return_flag;
        out->l_linestatus = line_status;
        out->l_shipdate = ship_date;
        out->l_shipdate_epoch = TPCHDate::ToUnixEpoch(ship_date);
        out->l_commitdate = commit_date;
        out->l_commitdate_epoch = TPCHDate::ToUnixEpoch(commit_date);
        out->l_receiptdate = receipt_date;
        out->l_receiptdate_epoch = TPCHDate::ToUnixEpoch(receipt_date);
        out->l_shipinstruct = ship_instruct;
        out->l_shipmode = ship_mode;
        out->l_comment = comment;
    }

    ++line_number_;

    if (line_number_ > line_count_)
    {
        order_date_random_.RowFinished();
        line_count_random_.RowFinished();

        quantity_random_.RowFinished();
        discount_random_.RowFinished();
        tax_random_.RowFinished();

        part_key_random_.RowFinished();
        supplier_number_random_.RowFinished();

        ship_date_random_.RowFinished();
        commit_date_random_.RowFinished();
        receipt_date_random_.RowFinished();

        return_flag_random_.RowFinished();
        ship_instruct_random_.RowFinished();
        ship_mode_random_.RowFinished();
        comment_random_.RowFinished();

        ++index_;

        if (index_ < order_count_)
        {
            line_count_ = line_count_random_.NextValue() - 1;
            order_date_ = order_date_random_.NextValue();
            line_number_ = 0;
        }
    }

    return true;
}

bool LineitemRowIterator::Done() const
{
    return index_ >= order_count_;
}

std::uint64_t LineitemRowIterator::NextRowId() const
{
    const std::uint64_t order_index = start_order_ + index_;
    return order_index * static_cast<std::uint64_t>(kLineCountMax) + static_cast<std::uint64_t>(line_number_);
}

RandomBoundedInt LineitemRowIterator::CreateOrderDateRandom()
{
    return RandomBoundedInt(kOrderDateSeed, kOrderDateMin, kOrderDateMax);
}

RandomBoundedInt LineitemRowIterator::CreateLineCountRandom()
{
    return RandomBoundedInt(kLineCountSeed, kLineCountMin, kLineCountMax);
}

RandomBoundedInt LineitemRowIterator::CreateQuantityRandom()
{
    return RandomBoundedInt(kLineQuantitySeed, kQuantityMin, kQuantityMax, kLineCountMax);
}

RandomBoundedInt LineitemRowIterator::CreateDiscountRandom()
{
    return RandomBoundedInt(kLineDiscountSeed, kDiscountMin, kDiscountMax, kLineCountMax);
}

RandomBoundedInt LineitemRowIterator::CreateTaxRandom()
{
    return RandomBoundedInt(kLineTaxSeed, kTaxMin, kTaxMax, kLineCountMax);
}

RandomBoundedLong LineitemRowIterator::CreatePartKeyRandom(double scale_factor)
{
    std::int64_t max_part_key = static_cast<std::int64_t>(static_cast<double>(kPart.rows_at_sf1) * scale_factor);
    if (max_part_key < 1)
    {
        max_part_key = 1;
    }
    return RandomBoundedLong(kLinePartKeySeed, scale_factor >= 30000.0, kPartKeyMin, max_part_key, kLineCountMax);
}

RandomBoundedInt LineitemRowIterator::CreateShipDateRandom()
{
    return RandomBoundedInt(kLineShipDateSeed, kShipDateMin, kShipDateMax, kLineCountMax);
}

std::int64_t LineitemRowIterator::MakeOrderKey(std::int64_t order_index)
{
    const std::int64_t low_bits = order_index & ((1LL << kOrderKeySparseKeep) - 1);
    std::int64_t ok = order_index;
    ok >>= kOrderKeySparseKeep;
    ok <<= kOrderKeySparseBits;
    ok <<= kOrderKeySparseKeep;
    ok += low_bits;
    return ok;
}

std::int64_t LineitemRowIterator::CalculatePartPrice(std::int64_t part_key)
{
    std::int64_t price = 90000;
    price += (part_key / 10) % 20001;
    price += (part_key % 1000) * 100;
    return price;
}

std::int32_t LineitemRowIterator::SelectPartSupplier(std::int64_t part_key, std::int32_t supplier_number, double scale_factor)
{
    const auto supplier_key_max_value = static_cast<std::int64_t>(kSupplierScaleBase * scale_factor);
    const std::int64_t supplier_number_i = supplier_number;
    const std::int64_t supplier_key
        = ((part_key + (supplier_number_i * ((supplier_key_max_value / kSuppliersPerPart) + ((part_key - 1) / supplier_key_max_value))))
           % supplier_key_max_value)
        + 1;

    return static_cast<std::int32_t>(supplier_key);
}

LineitemGenerator::LineitemGenerator(arrow::MemoryPool * pool)
    : factory_(pool)
    , columns_(factory_)
{
}

const TableMetadata & LineitemGenerator::GetTableMetadata() const
{
    return kLineitem;
}

void LineitemGenerator::Reset(const GeneratorContext & ctx)
{
    ctx_ = ctx;
    columns_.ClearAll();

    const TextPool & text_pool = ctx.text_pool != nullptr ? *ctx.text_pool : TextPool::Default();

    const auto total_orders = ctx.scale.RowCount(kOrders);
    const auto order_range = MakePartitionRange(total_orders, ctx.partition.part_num, ctx.partition.part_count);
    row_iter_.Reset(order_range.start_row, order_range.end_row, ctx.scale.factor, text_pool);
}

bool LineitemGenerator::NextBatch(std::uint64_t max_rows, TableBatch * out)
{
    if (max_rows == 0 || row_iter_.Done())
    {
        return false;
    }

    columns_.ClearAll();

    const std::uint64_t batch_start = row_iter_.NextRowId();
    std::uint64_t batch_count = 0;

    LineitemRow row;
    while (batch_count < max_rows && row_iter_.Next(&row))
    {
        columns_.l_orderkey.Append(row.l_orderkey);
        columns_.l_partkey.Append(row.l_partkey);
        columns_.l_suppkey.Append(row.l_suppkey);
        columns_.l_linenumber.Append(row.l_linenumber);
        columns_.l_quantity.Append(static_cast<double>(row.l_quantity));
        columns_.l_extendedprice.Append(row.l_extendedprice);
        columns_.l_discount.Append(row.l_discount);
        columns_.l_tax.Append(row.l_tax);
        columns_.l_returnflag.Append(row.l_returnflag);
        columns_.l_linestatus.Append(row.l_linestatus);
        columns_.l_shipdate.Append(row.l_shipdate_epoch);
        columns_.l_commitdate.Append(row.l_commitdate_epoch);
        columns_.l_receiptdate.Append(row.l_receiptdate_epoch);
        columns_.l_shipinstruct.Append(row.l_shipinstruct);
        columns_.l_shipmode.Append(row.l_shipmode);
        columns_.l_comment.Append(row.l_comment);
        ++batch_count;
    }

    if (out != nullptr)
    {
        out->metadata = &kLineitem;
        out->first_row_id = batch_start;
        out->row_count = batch_count;
        out->columns = {
            columns_.l_orderkey.Finish(),
            columns_.l_partkey.Finish(),
            columns_.l_suppkey.Finish(),
            columns_.l_linenumber.Finish(),
            columns_.l_quantity.Finish(),
            columns_.l_extendedprice.Finish(),
            columns_.l_discount.Finish(),
            columns_.l_tax.Finish(),
            columns_.l_returnflag.Finish(),
            columns_.l_linestatus.Finish(),
            columns_.l_shipdate.Finish(),
            columns_.l_commitdate.Finish(),
            columns_.l_receiptdate.Finish(),
            columns_.l_shipinstruct.Finish(),
            columns_.l_shipmode.Finish(),
            columns_.l_comment.Finish(),
        };
    }

    return true;
}
