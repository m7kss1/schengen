#include <gtest/gtest.h>

#include "nation.h"
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
