#include <mln/util/retiring.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace mln;

// 156 the 14 November hang freed a whole view's drapes in one frame
TEST(Retiring, NoFrameFreesMoreThanItsShare) {
    util::Retiring<std::shared_ptr<int>> retiring;
    std::vector<std::weak_ptr<int>> watched;
    for (int i = 0; i < 10; ++i) {
        auto item = std::make_shared<int>(i);
        watched.push_back(item);
        retiring.add(std::move(item));
    }

    EXPECT_EQ(retiring.release(4), 4u);
    EXPECT_EQ(retiring.size(), 6u);
    EXPECT_TRUE(watched[3].expired());
    EXPECT_FALSE(watched[4].expired()) << "the oldest go first";

    EXPECT_EQ(retiring.release(4), 4u);
    EXPECT_EQ(retiring.release(4), 2u);
    EXPECT_EQ(retiring.size(), 0u);
    EXPECT_EQ(retiring.release(4), 0u);
}

// 156 a memory warning still lets go of everything at once
TEST(Retiring, ZeroFreesThemAll) {
    util::Retiring<std::shared_ptr<int>> retiring;
    for (int i = 0; i < 7; ++i) {
        retiring.add(std::make_shared<int>(i));
    }

    EXPECT_EQ(retiring.release(0), 7u);
    EXPECT_EQ(retiring.size(), 0u);
}
