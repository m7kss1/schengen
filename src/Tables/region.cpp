#include "Tables/region.h"

#include <algorithm>

void RegionRowIterator::Reset(std::uint64_t start_row, std::uint64_t end_row, const TextPool & text_pool)
{
    next_row_ = start_row;
    end_row_ = std::min<std::uint64_t>(end_row, kRegionCount);
    if (next_row_ > end_row_)
    {
        next_row_ = end_row_;
    }
    comment_random_ = RandomText(kCommentSeed, text_pool, static_cast<double>(kCommentAverageLength));
    if (next_row_ > 0)
    {
        comment_random_.AdvanceRows(static_cast<std::int64_t>(next_row_));
    }
}

bool RegionRowIterator::Next(RegionRow * out)
{
    if (next_row_ >= end_row_)
    {
        return false;
    }

    const std::size_t index = static_cast<std::size_t>(next_row_);
    if (out != nullptr)
    {
        out->r_regionkey = static_cast<std::int32_t>(next_row_);
        out->r_name = kRegionNames[index];
        out->r_comment = comment_random_.NextValue();
    }
    comment_random_.RowFinished();
    ++next_row_;
    return true;
}

bool RegionRowIterator::Done() const
{
    return next_row_ >= end_row_;
}

std::uint64_t RegionRowIterator::NextRowId() const
{
    return next_row_;
}

RegionGenerator::RegionGenerator(arrow::MemoryPool * pool)
    : factory_(pool)
    , columns_(factory_)
{
}

const TableMetadata & RegionGenerator::GetTableMetadata() const
{
    return kRegion;
}

void RegionGenerator::Reset(const GeneratorContext & ctx)
{
    ctx_ = ctx;
    columns_.ClearAll();
    const TextPool & text_pool = ctx.text_pool != nullptr ? *ctx.text_pool : TextPool::Default();
    row_iter_.Reset(ctx.partition.range.start_row, ctx.partition.range.end_row, text_pool);
}

bool RegionGenerator::NextBatch(std::uint64_t max_rows, TableBatch * out)
{
    if (max_rows == 0 || row_iter_.Done())
    {
        return false;
    }

    columns_.ClearAll();

    const std::uint64_t batch_start = row_iter_.NextRowId();
    std::uint64_t batch_count = 0;

    RegionRow row;
    while (batch_count < max_rows && row_iter_.Next(&row))
    {
        columns_.r_regionkey.Append(row.r_regionkey);
        columns_.r_name.Append(row.r_name);
        columns_.r_comment.Append(row.r_comment);
        ++batch_count;
    }

    if (out != nullptr)
    {
        out->metadata = &kRegion;
        out->first_row_id = batch_start;
        out->row_count = batch_count;
        out->columns = {
            columns_.r_regionkey.Finish(),
            columns_.r_name.Finish(),
            columns_.r_comment.Finish(),
        };
    }

    return true;
}
