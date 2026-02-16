#include "Tables/nation.h"

#include <algorithm>

void NationRowIterator::Reset(std::uint64_t start_row, std::uint64_t end_row, const TextPool & text_pool)
{
    next_row_ = start_row;
    end_row_ = std::min<std::uint64_t>(end_row, kNationCount);
    if (next_row_ > end_row_)
    {
        next_row_ = end_row_;
    }

    region_cum_sum_ = 0;
    for (std::uint64_t i = 0; i < next_row_; ++i)
    {
        region_cum_sum_ += kNations[static_cast<std::size_t>(i)].weight;
    }

    comment_random_ = RandomText(kCommentSeed, text_pool, static_cast<double>(kCommentAverageLength));
    if (next_row_ > 0)
    {
        comment_random_.AdvanceRows(static_cast<std::int64_t>(next_row_));
    }
}

bool NationRowIterator::Next(NationRow * out)
{
    if (next_row_ >= end_row_)
    {
        return false;
    }

    const std::size_t index = static_cast<std::size_t>(next_row_);
    region_cum_sum_ += kNations[index].weight;

    if (out != nullptr)
    {
        out->n_nationkey = static_cast<std::int32_t>(next_row_);
        out->n_name = kNations[index].token;
        out->n_regionkey = region_cum_sum_;
        out->n_comment = comment_random_.NextValue();
    }
    comment_random_.RowFinished();
    ++next_row_;
    return true;
}

bool NationRowIterator::Done() const
{
    return next_row_ >= end_row_;
}

std::uint64_t NationRowIterator::NextRowId() const
{
    return next_row_;
}

NationGenerator::NationGenerator(arrow::MemoryPool * pool)
    : factory_(pool)
    , columns_(factory_)
{
}

const TableMetadata & NationGenerator::GetTableMetadata() const
{
    return kNation;
}

void NationGenerator::Reset(const GeneratorContext & ctx)
{
    ctx_ = ctx;
    columns_.ClearAll();
    const TextPool & text_pool = ctx.text_pool != nullptr ? *ctx.text_pool : TextPool::Default();
    row_iter_.Reset(ctx.partition.range.start_row, ctx.partition.range.end_row, text_pool);
}

bool NationGenerator::NextBatch(std::uint64_t max_rows, TableBatch * out)
{
    if (max_rows == 0 || row_iter_.Done())
    {
        return false;
    }

    columns_.ClearAll();

    const std::uint64_t batch_start = row_iter_.NextRowId();
    std::uint64_t batch_count = 0;

    NationRow row;
    while (batch_count < max_rows && row_iter_.Next(&row))
    {
        columns_.n_nationkey.Append(row.n_nationkey);
        columns_.n_name.Append(row.n_name);
        columns_.n_regionkey.Append(row.n_regionkey);
        columns_.n_comment.Append(row.n_comment);
        ++batch_count;
    }

    if (out != nullptr)
    {
        out->metadata = &kNation;
        out->first_row_id = batch_start;
        out->row_count = batch_count;
        out->columns = {
            columns_.n_nationkey.Finish(),
            columns_.n_name.Finish(),
            columns_.n_regionkey.Finish(),
            columns_.n_comment.Finish(),
        };
    }

    return true;
}
