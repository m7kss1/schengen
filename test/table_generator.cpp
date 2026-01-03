#include <gtest/gtest.h>

#include "region.h"
#include "rands.h"

#include <array>
#include <string_view>

static const TextPool & text_pool = TextPool::Default();

TEST(RegionGeneratorTest, MatchesReferenceComments)
{
    
    RegionRowIterator iter;
    iter.Reset(/*start_row=*/0, /*end_row=*/5, text_pool);

    const std::array<std::string_view, 5> expected_names = {
        "AFRICA",
        "AMERICA",
        "ASIA",
        "EUROPE",
        "MIDDLE EAST",
    };
    const std::array<std::string_view, 5> expected_comment_prefix = {
        "lar deposits. blithely final packages cajole. regular waters are final requests. regular accounts are according to",
        "hs use ironic, even requests. s",
        "ges. thinly even pinto beans ca",
        "ly final courts cajole furiously final excuse",
        "uickly special accounts cajole carefully blithely close requests. carefully final asymptotes haggle furiousl",
    };

    RegionRow row;
    for (std::size_t i = 0; i < expected_names.size(); ++i) {
        ASSERT_TRUE(iter.Next(&row));
        EXPECT_EQ(static_cast<std::int32_t>(i), row.r_regionkey);
        EXPECT_EQ(expected_names[i], row.r_name);
        ASSERT_GE(row.r_comment.size(), expected_comment_prefix[i].size());
        EXPECT_EQ(0,
                  row.r_comment.compare(0,
                                        expected_comment_prefix[i].size(),
                                        expected_comment_prefix[i]));
    }

    EXPECT_FALSE(iter.Next(&row));
}
