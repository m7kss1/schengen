#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "Common/distribution.h"
#include "Common/generator.h"
#include "Common/rands.h"
#include "Tables/table.h"

struct CustomerRow
{
    std::int32_t c_custkey = 0;
    std::string_view c_name;
    std::string_view c_address;
    std::int32_t c_nationkey = 0;
    std::string_view c_phone;
    std::int32_t c_acctbal_cents = 0;
    double c_acctbal = 0.0;
    std::string_view c_mktsegment;
    std::string_view c_comment;
};

class CustomerRowIterator
{
public:
    void Reset(std::uint64_t start_row, std::uint64_t end_row, const TextPool & text_pool);
    bool Next(CustomerRow * out);
    bool Done() const;
    std::uint64_t NextRowId() const;

private:
    static std::string FormatCustomerName(std::int32_t customer_key);

    static constexpr std::int32_t kAccountBalanceMin = -99'999;
    static constexpr std::int32_t kAccountBalanceMax = 999'999;
    static constexpr std::int32_t kAddressAverageLength = 25;
    static constexpr std::int32_t kCommentAverageLength = 73;

    static constexpr std::int64_t kAddressSeed = 881'155'353;
    static constexpr std::int64_t kNationKeySeed = 1'489'529'863;
    static constexpr std::int64_t kPhoneSeed = 1'521'138'112;
    static constexpr std::int64_t kAccountBalanceSeed = 298'370'230;
    static constexpr std::int64_t kMarketSegmentSeed = 1'140'279'430;
    static constexpr std::int64_t kCommentSeed = 1'335'826'707;

    std::uint64_t next_row_ = 0;
    std::uint64_t end_row_ = 0;

    RandomAlphaNumeric address_random_{};
    RandomBoundedInt nation_key_random_{};
    RandomPhoneNumber phone_random_{};
    RandomBoundedInt account_balance_random_{};
    RandomString market_segment_random_{};
    RandomText comment_random_{};

    std::string name_buffer_;
    std::string address_buffer_;
    std::string phone_buffer_;
};

class CustomerGenerator final : public ITableGenerator
{
public:
    explicit CustomerGenerator(arrow::MemoryPool * pool);
    const TableMetadata & GetTableMetadata() const override;
    void Reset(const GeneratorContext & ctx) override;
    bool NextBatch(std::uint64_t max_rows, TableBatch * out) override;

private:
    GeneratorContext ctx_{};
    BuilderFactory factory_;
    CustomerColumns columns_;
    CustomerRowIterator row_iter_;
};
