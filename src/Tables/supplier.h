#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

#include "Common/distribution.h"
#include "Common/generator.h"
#include "Common/rands.h"
#include "Tables/table.h"

struct SupplierRow
{
    std::int32_t s_suppkey = 0;
    std::string_view s_name;
    std::string_view s_address;
    std::int32_t s_nationkey = 0;
    std::string_view s_phone;
    std::int32_t s_acctbal_cents = 0;
    double s_acctbal = 0.0;
    std::string_view s_comment;
};

class SupplierRowIterator
{
public:
    void Reset(std::uint64_t start_row, std::uint64_t end_row, const TextPool & text_pool)
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
        comment_random_ = RandomText(kCommentSeed, text_pool, static_cast<double>(kCommentAverageLength));
        bbb_comment_random_ = RandomBoundedInt(kBbbCommentSeed, 1, kScaleBase);
        bbb_junk_random_ = RowRandomInt(kBbbJunkSeed, /*seeds_per_row=*/1);
        bbb_offset_random_ = RowRandomInt(kBbbOffsetSeed, /*seeds_per_row=*/1);
        bbb_type_random_ = RandomBoundedInt(kBbbTypeSeed, 0, 100);

        if (next_row_ > 0)
        {
            const auto rows = static_cast<std::int64_t>(next_row_);
            address_random_.AdvanceRows(rows);
            nation_key_random_.AdvanceRows(rows);
            phone_random_.AdvanceRows(rows);
            account_balance_random_.AdvanceRows(rows);
            comment_random_.AdvanceRows(rows);
            bbb_comment_random_.AdvanceRows(rows);
            bbb_junk_random_.AdvanceRows(rows);
            bbb_offset_random_.AdvanceRows(rows);
            bbb_type_random_.AdvanceRows(rows);
        }
    }

    bool Next(SupplierRow * out)
    {
        if (next_row_ >= end_row_)
        {
            return false;
        }

        const std::int32_t supplier_key = static_cast<std::int32_t>(next_row_ + 1);

        if (out != nullptr)
        {
            name_buffer_ = FormatSupplierName(supplier_key);

            const auto address = address_random_.NextValue();
            address.ToString(address_buffer_);

            const std::int32_t nation_key = nation_key_random_.NextValue();
            const auto phone = phone_random_.NextValue(nation_key);
            phone.ToString(phone_buffer_);

            const std::int32_t acctbal_cents = account_balance_random_.NextValue();

            /* 
             * NOTE: Here returns view on TextPool data
             * The comment is copied into buffer for optional BBB mutations
             */
            {
                const std::string_view comment_view = comment_random_.NextValue();
                comment_buffer_.assign(comment_view.data(), comment_view.size());
            }

            /* Add some BBB mutation in comment if needed */
            const std::int32_t bbb_value = bbb_comment_random_.NextValue();
            if (bbb_value <= kBbbCommentsPerScaleBase)
            {
                ApplyBBBComment(comment_buffer_);
            }

            out->s_suppkey = supplier_key;
            out->s_name = std::string_view(name_buffer_);
            out->s_address = std::string_view(address_buffer_);
            out->s_nationkey = nation_key;
            out->s_phone = std::string_view(phone_buffer_);
            out->s_acctbal_cents = acctbal_cents;
            out->s_acctbal = static_cast<double>(acctbal_cents) / 100.0;
            out->s_comment = std::string_view(comment_buffer_);
        }

        address_random_.RowFinished();
        nation_key_random_.RowFinished();
        phone_random_.RowFinished();
        account_balance_random_.RowFinished();
        comment_random_.RowFinished();
        bbb_comment_random_.RowFinished();
        bbb_junk_random_.RowFinished();
        bbb_offset_random_.RowFinished();
        bbb_type_random_.RowFinished();

        ++next_row_;
        return true;
    }

    bool Done() const { return next_row_ >= end_row_; }
    std::uint64_t NextRowId() const { return next_row_; }

private:
    static std::string FormatSupplierName(std::int32_t supplier_key)
    {
        char buffer[32];
        const int n = std::snprintf(buffer, sizeof(buffer), "Supplier#%09d", supplier_key);
        return std::string(buffer, buffer + (n > 0 ? n : 0));
    }

    void ApplyBBBComment(std::string & comment)
    {
        if (comment.size() < kBbbCommentLength)
        {
            return;
        }

        const std::int32_t max_noise = static_cast<std::int32_t>(comment.size() - kBbbCommentLength);
        const std::int32_t noise = bbb_junk_random_.NextInt(0, max_noise);

        const std::int32_t max_offset = static_cast<std::int32_t>(comment.size() - (kBbbCommentLength + noise));
        const std::int32_t offset = bbb_offset_random_.NextInt(0, max_offset);

        const char * type_text = (bbb_type_random_.NextValue() < kBbbComplaintPercent) ? kBbbComplaintText : kBbbRecommendText;

        std::string modified;
        modified.reserve(comment.size());

        const std::size_t off = static_cast<std::size_t>(offset);
        const std::size_t base = std::strlen(kBbbBaseText);
        const std::size_t type_len = std::strlen(type_text);
        const std::size_t n = static_cast<std::size_t>(noise);

        modified.append(comment.data(), off);
        modified.append(kBbbBaseText, base);
        modified.append(comment.data() + off + base, n);
        modified.append(type_text, type_len);
        modified.append(comment.data() + off + base + n + type_len, comment.size() - (off + base + n + type_len));

        comment.swap(modified);
    }

    static constexpr std::int32_t kScaleBase = 10'000;
    static constexpr std::int32_t kAccountBalanceMin = -99'999;
    static constexpr std::int32_t kAccountBalanceMax = 999'999;
    static constexpr std::int32_t kAddressAverageLength = 25;
    static constexpr std::int32_t kCommentAverageLength = 63;

    static constexpr const char * kBbbBaseText = "Customer ";
    static constexpr const char * kBbbComplaintText = "Complaints";
    static constexpr const char * kBbbRecommendText = "Recommends";
    static constexpr std::int32_t kBbbCommentLength = 9 /* strlen("Customer ") */ + 10 /* strlen("Complaints") */;
    static constexpr std::int32_t kBbbCommentsPerScaleBase = 10;
    static constexpr std::int32_t kBbbComplaintPercent = 50;

    static constexpr std::int64_t kAddressSeed = 706178559;
    static constexpr std::int64_t kNationKeySeed = 110356601;
    static constexpr std::int64_t kPhoneSeed = 884434366;
    static constexpr std::int64_t kAccountBalanceSeed = 962338209;
    static constexpr std::int64_t kCommentSeed = 1341315363;
    static constexpr std::int64_t kBbbCommentSeed = 202794285;
    static constexpr std::int64_t kBbbJunkSeed = 263032577;
    static constexpr std::int64_t kBbbOffsetSeed = 715851524;
    static constexpr std::int64_t kBbbTypeSeed = 753643799;

    std::uint64_t next_row_ = 0;
    std::uint64_t end_row_ = 0;

    RandomAlphaNumeric address_random_{};
    RandomBoundedInt nation_key_random_{};
    RandomPhoneNumber phone_random_{};
    RandomBoundedInt account_balance_random_{};
    RandomText comment_random_{};
    RandomBoundedInt bbb_comment_random_{};
    RowRandomInt bbb_junk_random_{};
    RowRandomInt bbb_offset_random_{};
    RandomBoundedInt bbb_type_random_{};

    std::string name_buffer_;
    std::string address_buffer_;
    std::string phone_buffer_;
    std::string comment_buffer_;
};

class SupplierGenerator final : public ITableGenerator
{
public:
    explicit SupplierGenerator(arrow::MemoryPool * pool)
        : factory_(pool)
        , columns_(factory_)
    {
    }

    const TableMetadata & GetTableMetadata() const override { return kSupplier; }

    void Reset(const GeneratorContext & ctx) override
    {
        ctx_ = ctx;
        columns_.ClearAll();

        const TextPool & text_pool = ctx.text_pool != nullptr ? *ctx.text_pool : TextPool::Default();

        row_iter_.Reset(ctx.partition.range.start_row, ctx.partition.range.end_row, text_pool);
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

        SupplierRow row;
        while (batch_count < max_rows && row_iter_.Next(&row))
        {
            columns_.s_suppkey.Append(row.s_suppkey);
            columns_.s_name.Append(row.s_name);
            columns_.s_address.Append(row.s_address);
            columns_.s_nationkey.Append(row.s_nationkey);
            columns_.s_phone.Append(row.s_phone);
            columns_.s_acctbal.Append(row.s_acctbal);
            columns_.s_comment.Append(row.s_comment);
            ++batch_count;
        }

        if (out != nullptr)
        {
            out->metadata = &kSupplier;
            out->first_row_id = batch_start;
            out->row_count = batch_count;
            out->columns = {
                columns_.s_suppkey.Finish(),
                columns_.s_name.Finish(),
                columns_.s_address.Finish(),
                columns_.s_nationkey.Finish(),
                columns_.s_phone.Finish(),
                columns_.s_acctbal.Finish(),
                columns_.s_comment.Finish(),
            };
        }

        return true;
    }

private:
    GeneratorContext ctx_{};
    BuilderFactory factory_;
    SupplierColumns columns_;
    SupplierRowIterator row_iter_;
};
