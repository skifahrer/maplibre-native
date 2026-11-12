#include <mln/test/util.hpp>

#include <mln/renderer/cover_reach.hpp>

#include <cmath>
#include <limits>

using namespace mln;

namespace {
constexpr double none = std::numeric_limits<double>::infinity();
constexpr double fogEnd = 20000.0;
constexpr double frame = 1.0 / 30.0;
} // namespace

// 113 A tile dropped one frame and kept the next turned the fog on and off.
TEST(CoverReach, AFlickeringDropHoldsTheFogStill) {
    CoverReach reach;
    double seconds = 0.0;
    reach.update(none, seconds, fogEnd);
    for (int i = 0; i < 90; ++i) {
        seconds += frame;
        reach.update(i % 2 ? none : 5000.0, seconds, fogEnd);
    }
    const double held = reach.value();
    EXPECT_NEAR(5000.0, held, 5.0);
    for (int i = 0; i < 90; ++i) {
        seconds += frame;
        EXPECT_NEAR(held, reach.update(i % 2 ? none : 5000.0, seconds, fogEnd), 5.0);
    }
}

// 113 The fog came in a whole cover's depth in one frame.
TEST(CoverReach, TheFogComesInOverAFewFrames) {
    CoverReach reach;
    reach.update(none, 0.0, fogEnd);
    const double next = reach.update(5000.0, frame, fogEnd);
    EXPECT_GT(next, 5000.0);
    EXPECT_LT(next, fogEnd);
    EXPECT_NEAR(5000.0, reach.update(5000.0, 2.0, fogEnd), 1.0);
}

// 113 Once nothing is dropped for a while, the fog goes back out and the cap lifts.
TEST(CoverReach, AFullCoverLetsTheFogOut) {
    CoverReach reach;
    reach.update(5000.0, 0.0, fogEnd);
    EXPECT_EQ(5000.0, reach.update(none, CoverReach::hold / 2, fogEnd));
    EXPECT_FALSE(std::isfinite(reach.update(none, 30.0, fogEnd)));
}
