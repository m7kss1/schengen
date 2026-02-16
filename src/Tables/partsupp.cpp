#include "Tables/partsupp.h"

#include "Common/partition.h"

void PartSuppRowIterator::Reset(std::uint64_t start_part, std::uint64_t end_part, double scale_factor, const TextPool & text_pool)
{
    next_part_ = start_part;
    end_part_ = end_part;
    if (next_part_ > end_part_)
    {
        next_part_ = end_part_;
    }

    scale_factor_ = scale_factor;

    available_quantity_random_ = RandomBoundedInt(kAvailableQuantitySeed, kAvailableQuantityMin, kAvailableQuantityMax, kSuppliersPerPart);
    supply_cost_random_ = RandomBoundedInt(kSupplyCostSeed, kSupplyCostMin, kSupplyCostMax, kSuppliersPerPart);
    comment_random_ = RandomText(kCommentSeed, text_pool, static_cast<double>(kCommentAverageLength), kSuppliersPerPart);

    if (next_part_ > 0)
    {
        const auto rows = static_cast<std::int64_t>(next_part_);
        available_quantity_random_.AdvanceRows(rows);
        supply_cost_random_.AdvanceRows(rows);
        comment_random_.AdvanceRows(rows);
    }
}

bool PartSuppRowIterator::Next(PartSuppRow * out)
{
    if (next_part_ >= end_part_)
    {
        return false;
    }

    const std::int32_t part_key = static_cast<std::int32_t>(next_part_ + 1);
    const std::int32_t supplier_key = SelectPartSupplier(part_key, part_supplier_number_, scale_factor_);

    if (out != nullptr)
    {
        const std::int32_t availqty = available_quantity_random_.NextValue();
        const std::int32_t supplycost_cents = supply_cost_random_.NextValue();
        const std::string_view comment = comment_random_.NextValue();

        out->ps_partkey = part_key;
        out->ps_suppkey = supplier_key;
        out->ps_availqty = availqty;
        out->ps_supplycost_cents = supplycost_cents;
        out->ps_supplycost = static_cast<double>(supplycost_cents) / 100.0;
        out->ps_comment = comment;
    }

    /* 
     * We emit 4 suppliers for every part. Generator row ends only after the 4th supplier 
     *
     * ┌──────────────────────────────────────────────────────────────────────────────────────────┐
     * │ ps_partkey │ ps_suppkey │ ps_availqty │ ps_supplycost │           ps_comment             │
     * │   int64    │   int64    │    int64    │    decimal    │             varchar              │ 
     * ├────────────┼────────────┼─────────────┼───────────────┼──────────────────────────────────┤
     * │          1 │          2 │        3325 │        771.64 │ blithely regular theodolites ... │
     * │          1 │       2502 │        8076 │        993.49 │ ts boost carefully ironic ...    │
     * │          1 │       5002 │        3956 │        337.09 │  fluffily regular multipliers ...  │
     * │          1 │       7502 │        4069 │        357.84 │ press deposits. special ...      │
     * │          2 │          3 │        8895 │        378.49 │ sits. furiously regular ...      │
     * │          2 │       2503 │        4969 │        915.27 │ deposits doze. slyly ...         │
     * ├────────────┴────────────┴─────────────┴───────────────┴──────────────────────────────────┤
     */

    ++part_supplier_number_;
    if (part_supplier_number_ >= kSuppliersPerPart)
    {
        available_quantity_random_.RowFinished();
        supply_cost_random_.RowFinished();
        comment_random_.RowFinished();

        part_supplier_number_ = 0;
        ++next_part_;
    }

    return true;
}

bool PartSuppRowIterator::Done() const
{
    return next_part_ >= end_part_;
}

std::uint64_t PartSuppRowIterator::NextRowId() const
{
    return next_part_ * static_cast<std::uint64_t>(kSuppliersPerPart) + static_cast<std::uint64_t>(part_supplier_number_);
}

/*
 * Deterministic mapping from {part_key, supplier_number} -> supplier key
 * supplier_number is always {0..3} (4 distinct suppliers per part)
 */
std::int32_t PartSuppRowIterator::SelectPartSupplier(std::int32_t part_key, std::int32_t supplier_number, double scale_factor)
{
    const auto supplier_key_max_value = static_cast<std::int64_t>(kSupplierScaleBase * scale_factor);

    const std::int64_t part_key_i = part_key;
    const std::int64_t supplier_number_i = supplier_number;

    const std::int64_t supplier_key
        = ((part_key_i + (supplier_number_i * ((supplier_key_max_value / kSuppliersPerPart) + ((part_key_i - 1) / supplier_key_max_value))))
           % supplier_key_max_value)
        + 1;

    return static_cast<std::int32_t>(supplier_key);
}

PartSuppGenerator::PartSuppGenerator(arrow::MemoryPool * pool)
    : factory_(pool)
    , columns_(factory_)
{
}

const TableMetadata & PartSuppGenerator::GetTableMetadata() const
{
    return kPartsupp;
}

void PartSuppGenerator::Reset(const GeneratorContext & ctx)
{
    ctx_ = ctx;
    columns_.ClearAll();

    const TextPool & text_pool = ctx.text_pool != nullptr ? *ctx.text_pool : TextPool::Default();

    const auto total_parts = ctx.scale.RowCount(kPart);
    const auto part_range = MakePartitionRange(total_parts, ctx.partition.part_num, ctx.partition.part_count);

    row_iter_.Reset(part_range.start_row, part_range.end_row, ctx.scale.factor, text_pool);
}

bool PartSuppGenerator::NextBatch(std::uint64_t max_rows, TableBatch * out)
{
    if (max_rows == 0 || row_iter_.Done())
    {
        return false;
    }

    columns_.ClearAll();

    const std::uint64_t batch_start = row_iter_.NextRowId();
    std::uint64_t batch_count = 0;

    PartSuppRow row;
    while (batch_count < max_rows && row_iter_.Next(&row))
    {
        columns_.ps_partkey.Append(row.ps_partkey);
        columns_.ps_suppkey.Append(row.ps_suppkey);
        columns_.ps_availqty.Append(row.ps_availqty);
        columns_.ps_supplycost.Append(row.ps_supplycost);
        columns_.ps_comment.Append(row.ps_comment);
        ++batch_count;
    }

    if (out != nullptr)
    {
        out->metadata = &kPartsupp;
        out->first_row_id = batch_start;
        out->row_count = batch_count;
        out->columns = {
            columns_.ps_partkey.Finish(),
            columns_.ps_suppkey.Finish(),
            columns_.ps_availqty.Finish(),
            columns_.ps_supplycost.Finish(),
            columns_.ps_comment.Finish(),
        };
    }

    return true;
}
