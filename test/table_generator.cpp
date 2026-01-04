#include <gtest/gtest.h>

#include "customer.h"
#include "nation.h"
#include "orders.h"
#include "part.h"
#include "partsupp.h"
#include "region.h"
#include "rands.h"
#include "supplier.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

static const TextPool & text_pool = TextPool::Default();

static std::string ExpectedPath(const char * filename)
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::path(__FILE__).parent_path();
    return (dir / "expected" / filename).string();
}

static std::vector<std::string> ReadLinesOrDie(const char * path)
{
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error(std::string("failed to open expected file: ") + path);
    }

    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            lines.emplace_back(std::move(line));
        }
    }
    return lines;
}

static std::string FormatRegionRow(const RegionRow & row)
{
    std::string out;
    out.reserve(16 + row.r_name.size() + row.r_comment.size());
    out.append(std::to_string(row.r_regionkey));
    out.push_back('|');
    out.append(row.r_name.data(), row.r_name.size());
    out.push_back('|');
    out.append(row.r_comment.data(), row.r_comment.size());
    out.push_back('|');
    return out;
}

static std::string FormatNationRow(const NationRow & row)
{
    std::string out;
    out.reserve(16 + row.n_name.size() + row.n_comment.size());
    out.append(std::to_string(row.n_nationkey));
    out.push_back('|');
    out.append(row.n_name.data(), row.n_name.size());
    out.push_back('|');
    out.append(std::to_string(row.n_regionkey));
    out.push_back('|');
    out.append(row.n_comment.data(), row.n_comment.size());
    out.push_back('|');
    return out;
}

static std::string FormatDecimalCents(std::int32_t cents)
{
    const bool negative = cents < 0;
    const std::int32_t abs_cents = negative ? -cents : cents;
    const std::int32_t int_digits = abs_cents / 100;
    const std::int32_t dec_digits = abs_cents % 100;

    std::string out;
    out.reserve(32);
    if (negative) {
        out.push_back('-');
    }
    out.append(std::to_string(int_digits));
    out.push_back('.');
    if (dec_digits < 10) {
        out.push_back('0');
    }
    out.append(std::to_string(dec_digits));
    return out;
}

static std::string FormatSupplierRow(const SupplierRow & row)
{
    std::string out;
    out.reserve(128 + row.s_name.size() + row.s_address.size() + row.s_phone.size() + row.s_comment.size());
    out.append(std::to_string(row.s_suppkey));
    out.push_back('|');
    out.append(row.s_name.data(), row.s_name.size());
    out.push_back('|');
    out.append(row.s_address.data(), row.s_address.size());
    out.push_back('|');
    out.append(std::to_string(row.s_nationkey));
    out.push_back('|');
    out.append(row.s_phone.data(), row.s_phone.size());
    out.push_back('|');
    const auto acctbal = FormatDecimalCents(row.s_acctbal_cents);
    out.append(acctbal.data(), acctbal.size());
    out.push_back('|');
    out.append(row.s_comment.data(), row.s_comment.size());
    out.push_back('|');
    return out;
}

static std::string FormatPartSuppRow(const PartSuppRow & row)
{
    std::string out;
    out.reserve(128 + row.ps_comment.size());
    out.append(std::to_string(row.ps_partkey));
    out.push_back('|');
    out.append(std::to_string(row.ps_suppkey));
    out.push_back('|');
    out.append(std::to_string(row.ps_availqty));
    out.push_back('|');
    const auto supplycost = FormatDecimalCents(row.ps_supplycost_cents);
    out.append(supplycost);
    out.push_back('|');
    out.append(row.ps_comment.data(), row.ps_comment.size());
    out.push_back('|');
    return out;
}

static std::string FormatPartRow(const PartRow & row)
{
    std::string out;
    out.reserve(256 + row.p_name.size() + row.p_comment.size());
    out.append(std::to_string(row.p_partkey));
    out.push_back('|');
    out.append(row.p_name.data(), row.p_name.size());
    out.push_back('|');
    out.append(row.p_mfgr.data(), row.p_mfgr.size());
    out.push_back('|');
    out.append(row.p_brand.data(), row.p_brand.size());
    out.push_back('|');
    out.append(row.p_type.data(), row.p_type.size());
    out.push_back('|');
    out.append(std::to_string(row.p_size));
    out.push_back('|');
    out.append(row.p_container.data(), row.p_container.size());
    out.push_back('|');
    const auto price = FormatDecimalCents(row.p_retailprice_cents);
    out.append(price);
    out.push_back('|');
    out.append(row.p_comment.data(), row.p_comment.size());
    out.push_back('|');
    return out;
}

static std::string FormatCustomerRow(const CustomerRow & row)
{
    std::string out;
    out.reserve(256 + row.c_name.size() + row.c_address.size() + row.c_comment.size());
    out.append(std::to_string(row.c_custkey));
    out.push_back('|');
    out.append(row.c_name.data(), row.c_name.size());
    out.push_back('|');
    out.append(row.c_address.data(), row.c_address.size());
    out.push_back('|');
    out.append(std::to_string(row.c_nationkey));
    out.push_back('|');
    out.append(row.c_phone.data(), row.c_phone.size());
    out.push_back('|');
    const auto acctbal = FormatDecimalCents(row.c_acctbal_cents);
    out.append(acctbal);
    out.push_back('|');
    out.append(row.c_mktsegment.data(), row.c_mktsegment.size());
    out.push_back('|');
    out.append(row.c_comment.data(), row.c_comment.size());
    out.push_back('|');
    return out;
}

static std::string FormatOrderRow(const OrderRow & row)
{
    std::string out;
    out.reserve(256 + row.o_comment.size());
    out.append(std::to_string(row.o_orderkey));
    out.push_back('|');
    out.append(std::to_string(row.o_custkey));
    out.push_back('|');
    out.append(row.o_orderstatus.data(), row.o_orderstatus.size());
    out.push_back('|');
    const auto totalprice = FormatDecimalCents(row.o_totalprice_cents);
    out.append(totalprice);
    out.push_back('|');
    const auto orderdate = TPCHDate::Format(row.o_orderdate);
    out.append(orderdate);
    out.push_back('|');
    out.append(row.o_orderpriority.data(), row.o_orderpriority.size());
    out.push_back('|');
    out.append(row.o_clerk.data(), row.o_clerk.size());
    out.push_back('|');
    out.append(std::to_string(row.o_shippriority));
    out.push_back('|');
    out.append(row.o_comment.data(), row.o_comment.size());
    out.push_back('|');
    return out;
}

TEST(RegionGeneratorTest, MatchesReferenceTable)
{
    const auto expected =
        ReadLinesOrDie(ExpectedPath("region.tbl").c_str());

    RegionRowIterator iter;
    iter.Reset(/*start_row=*/0, /*end_row=*/expected.size(), text_pool);

    RegionRow row;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        ASSERT_TRUE(iter.Next(&row));
        EXPECT_EQ(expected[i], FormatRegionRow(row)) << "row " << i;
    }

    EXPECT_FALSE(iter.Next(&row));
}

TEST(NationGeneratorTest, MatchesReferenceTable)
{
    const auto expected =
        ReadLinesOrDie(ExpectedPath("nation.tbl").c_str());

    NationRowIterator iter;
    iter.Reset(/*start_row=*/0, /*end_row=*/expected.size(), text_pool);

    NationRow row;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        ASSERT_TRUE(iter.Next(&row));
        EXPECT_EQ(expected[i], FormatNationRow(row)) << "row " << i;
    }

    EXPECT_FALSE(iter.Next(&row));
}

TEST(SupplierGeneratorTest, MatchesReferenceTable)
{
    const auto expected =
        ReadLinesOrDie(ExpectedPath("supplier.tbl").c_str());

    SupplierRowIterator iter;
    iter.Reset(/*start_row=*/0, /*end_row=*/expected.size(), text_pool);

    SupplierRow row;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        ASSERT_TRUE(iter.Next(&row));
        EXPECT_EQ(expected[i], FormatSupplierRow(row)) << "row " << i;
    }

    EXPECT_FALSE(iter.Next(&row));
}

TEST(PartSuppGeneratorTest, MatchesReferenceTable)
{
    const auto expected =
        ReadLinesOrDie(ExpectedPath("partsupp.tbl").c_str());

    const std::size_t part_count = expected.size() / 4; // 4 suppliers per part

    PartSuppRowIterator iter;
    iter.Reset(/*start_part=*/0, /*end_part=*/part_count, /*scale_factor=*/1.0, text_pool);

    PartSuppRow row;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        ASSERT_TRUE(iter.Next(&row));
        EXPECT_EQ(expected[i], FormatPartSuppRow(row)) << "row " << i;
    }

    EXPECT_FALSE(iter.Next(&row));
}

TEST(PartGeneratorTest, MatchesReferenceTable)
{
    const auto expected =
        ReadLinesOrDie(ExpectedPath("part.tbl").c_str());

    PartRowIterator iter;
    iter.Reset(/*start_row=*/0, /*end_row=*/expected.size(), text_pool);

    PartRow row;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        ASSERT_TRUE(iter.NextValue(&row));
        EXPECT_EQ(expected[i], FormatPartRow(row)) << "row " << i;
    }

    EXPECT_FALSE(iter.NextValue(&row));
}

TEST(CustomerGeneratorTest, MatchesReferenceTable)
{
    const auto expected =
        ReadLinesOrDie(ExpectedPath("customer.tbl").c_str());

    CustomerRowIterator iter;
    iter.Reset(/*start_row=*/0, /*end_row=*/expected.size(), text_pool);

    CustomerRow row;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        ASSERT_TRUE(iter.Next(&row));
        EXPECT_EQ(expected[i], FormatCustomerRow(row)) << "row " << i;
    }

    EXPECT_FALSE(iter.Next(&row));
}

TEST(OrderGeneratorTest, MatchesReferenceTable)
{
    const auto expected =
        ReadLinesOrDie(ExpectedPath("orders.tbl").c_str());

    OrderRowIterator iter;
    iter.Reset(/*start_row=*/0, /*end_row=*/expected.size(), /*scale_factor=*/1.0, text_pool);

    OrderRow row;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        ASSERT_TRUE(iter.Next(&row));
        EXPECT_EQ(expected[i], FormatOrderRow(row)) << "row " << i;
    }

    EXPECT_FALSE(iter.Next(&row));
}
