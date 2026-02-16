#include "Tables/customer.h"

#include <cstdio>

void CustomerRowIterator::Reset(std::uint64_t start_row, std::uint64_t end_row, const TextPool & text_pool)
{
    next_row_ = start_row;
    end_row_ = end_row;
    if (next_row_ > end_row_)
    {
        next_row_ = end_row_;
    }

    address_random_ = RandomAlphaNumeric(kAddressSeed, kAddressAverageLength);
    nation_key_random_ = RandomBoundedInt(kNationKeySeed, 0, static_cast<std::int32_t>(kNations.size() - 1));
    phone_random_ = RandomPhoneNumber(kPhoneSeed);
    account_balance_random_ = RandomBoundedInt(kAccountBalanceSeed, kAccountBalanceMin, kAccountBalanceMax);
    market_segment_random_ = RandomString(kMarketSegmentSeed, kMarketSegmentsDist);
    comment_random_ = RandomText(kCommentSeed, text_pool, static_cast<double>(kCommentAverageLength));

    if (next_row_ > 0)
    {
        const auto rows = static_cast<std::int64_t>(next_row_);
        address_random_.AdvanceRows(rows);
        nation_key_random_.AdvanceRows(rows);
        phone_random_.AdvanceRows(rows);
        account_balance_random_.AdvanceRows(rows);
        market_segment_random_.AdvanceRows(rows);
        comment_random_.AdvanceRows(rows);
    }
}

bool CustomerRowIterator::Next(CustomerRow * out)
{
    if (next_row_ >= end_row_)
    {
        return false;
    }

    const std::int32_t customer_key = static_cast<std::int32_t>(next_row_ + 1);

    if (out != nullptr)
    {
        name_buffer_ = FormatCustomerName(customer_key);

        const auto address = address_random_.NextValue();
        address.ToString(address_buffer_);

        const std::int32_t nation_key = nation_key_random_.NextValue();

        const auto phone = phone_random_.NextValue(nation_key);
        phone.ToString(phone_buffer_);

        const std::int32_t acctbal_cents = account_balance_random_.NextValue();

        const std::string_view mktsegment = market_segment_random_.NextValue();
        const std::string_view comment = comment_random_.NextValue();

        out->c_custkey = customer_key;
        out->c_name = std::string_view(name_buffer_);
        out->c_address = std::string_view(address_buffer_);
        out->c_nationkey = nation_key;
        out->c_phone = std::string_view(phone_buffer_);
        out->c_acctbal_cents = acctbal_cents;
        out->c_acctbal = static_cast<double>(acctbal_cents) / 100.0;
        out->c_mktsegment = mktsegment;
        out->c_comment = comment;
    }
    else
    {
        (void)address_random_.NextValue();
        const std::int32_t nation_key = nation_key_random_.NextValue();
        (void)phone_random_.NextValue(nation_key);
        (void)account_balance_random_.NextValue();
        (void)market_segment_random_.NextValue();
        (void)comment_random_.NextValue();
    }

    address_random_.RowFinished();
    nation_key_random_.RowFinished();
    phone_random_.RowFinished();
    account_balance_random_.RowFinished();
    market_segment_random_.RowFinished();
    comment_random_.RowFinished();

    ++next_row_;
    return true;
}

bool CustomerRowIterator::Done() const
{
    return next_row_ >= end_row_;
}

std::uint64_t CustomerRowIterator::NextRowId() const
{
    return next_row_;
}

std::string CustomerRowIterator::FormatCustomerName(std::int32_t customer_key)
{
    char buffer[32];
    const int n = std::snprintf(buffer, sizeof(buffer), "Customer#%09d", customer_key);
    return std::string(buffer, buffer + (n > 0 ? n : 0));
}

CustomerGenerator::CustomerGenerator(arrow::MemoryPool * pool)
    : factory_(pool)
    , columns_(factory_)
{
}

const TableMetadata & CustomerGenerator::GetTableMetadata() const
{
    return kCustomer;
}

void CustomerGenerator::Reset(const GeneratorContext & ctx)
{
    ctx_ = ctx;
    columns_.ClearAll();

    const TextPool & text_pool = ctx.text_pool != nullptr ? *ctx.text_pool : TextPool::Default();
    row_iter_.Reset(ctx.partition.range.start_row, ctx.partition.range.end_row, text_pool);
}

bool CustomerGenerator::NextBatch(std::uint64_t max_rows, TableBatch * out)
{
    if (max_rows == 0 || row_iter_.Done())
    {
        return false;
    }

    columns_.ClearAll();

    const std::uint64_t batch_start = row_iter_.NextRowId();
    std::uint64_t batch_count = 0;

    CustomerRow row;
    while (batch_count < max_rows && row_iter_.Next(&row))
    {
        columns_.c_custkey.Append(row.c_custkey);
        columns_.c_name.Append(row.c_name);
        columns_.c_address.Append(row.c_address);
        columns_.c_nationkey.Append(row.c_nationkey);
        columns_.c_phone.Append(row.c_phone);
        columns_.c_acctbal.Append(row.c_acctbal);
        columns_.c_mktsegment.Append(row.c_mktsegment);
        columns_.c_comment.Append(row.c_comment);
        ++batch_count;
    }

    if (out != nullptr)
    {
        out->metadata = &kCustomer;
        out->first_row_id = batch_start;
        out->row_count = batch_count;
        out->columns = {
            columns_.c_custkey.Finish(),
            columns_.c_name.Finish(),
            columns_.c_address.Finish(),
            columns_.c_nationkey.Finish(),
            columns_.c_phone.Finish(),
            columns_.c_acctbal.Finish(),
            columns_.c_mktsegment.Finish(),
            columns_.c_comment.Finish(),
        };
    }

    return true;
}
