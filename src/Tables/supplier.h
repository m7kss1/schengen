#pragma once

#include <cstdint>
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
    void Reset(std::uint64_t start_row, std::uint64_t end_row, const TextPool & text_pool);
    bool Next(SupplierRow * out);
    bool Done() const;
    std::uint64_t NextRowId() const;

private:
    static std::string FormatSupplierName(std::int32_t supplier_key);
    void ApplyBBBComment(std::string & comment);

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
    explicit SupplierGenerator(arrow::MemoryPool * pool);
    const TableMetadata & GetTableMetadata() const override;
    void Reset(const GeneratorContext & ctx) override;
    bool NextBatch(std::uint64_t max_rows, TableBatch * out) override;

private:
    GeneratorContext ctx_{};
    BuilderFactory factory_;
    SupplierColumns columns_;
    SupplierRowIterator row_iter_;
};
