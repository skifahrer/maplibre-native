#include <mln/test/util.hpp>

#include <mln/renderer/drape_turns.hpp>

#include <algorithm>

using namespace mln;

namespace {
/// The longest any target waits when every target changes every frame.
uint32_t longestWait(std::size_t targets, std::size_t budget, int frames) {
    std::vector<uint32_t> held(targets, 0);
    uint32_t longest = 0;
    for (int frame = 0; frame < frames; ++frame) {
        std::size_t left = budget;
        for (const std::size_t turn : drapeTurns(held)) {
            if (left > 0) {
                held[turn] = 0;
                --left;
            } else {
                longest = std::max(longest, ++held[turn]);
            }
        }
    }
    return longest;
}
} // namespace

// 127 A moving route line left stale tiles past the first four of every frame (ANI1).
TEST(DrapeTurns, EveryChangingTargetIsRedrawnInTurn) {
    EXPECT_EQ(1u, longestWait(8, 4, 60));
    EXPECT_EQ(2u, longestWait(12, 4, 60));
}

// 127 A target held back goes before every target redrawn since.
TEST(DrapeTurns, TheLongestHeldGoesFirst) {
    const std::vector<std::size_t> expected{2, 0, 3, 1};
    EXPECT_EQ(expected, drapeTurns({1, 0, 3, 1}));
}

// 127 With nothing held the targets keep their own order.
TEST(DrapeTurns, NothingHeldKeepsTheOrder) {
    const std::vector<std::size_t> expected{0, 1, 2};
    EXPECT_EQ(expected, drapeTurns({0, 0, 0}));
}
