#include <mln/gfx/rendering_stats.hpp>

#include <gtest/gtest.h>

using namespace mln;

// 142 the wait for a drawable and the GPU's time add up over frames like the CPU's
TEST(RenderingStats, WaitAndGpuTimeAddUp) {
    gfx::RenderingStats total;
    gfx::RenderingStats frame;
    frame.encodingTime = 0.010;
    frame.drawableWaitTime = 0.004;
    frame.gpuTime = 0.006;
    total += frame;
    total += frame;
    EXPECT_DOUBLE_EQ(total.drawableWaitTime, 0.008);
    EXPECT_DOUBLE_EQ(total.gpuTime, 0.012);
    EXPECT_DOUBLE_EQ(total.encodingTime, 0.020);
}

// 142 a new frame's stats start with neither, so a backend without them reports nothing
TEST(RenderingStats, WaitAndGpuTimeStartAtZero) {
    const gfx::RenderingStats fresh;
    EXPECT_EQ(fresh.drawableWaitTime, 0.0);
    EXPECT_EQ(fresh.gpuTime, 0.0);
    EXPECT_EQ(fresh.toString(" ").find("gpuTime"), std::string::npos);
}

// 142 the printed stats name both once they are measured
TEST(RenderingStats, WaitAndGpuTimeArePrinted) {
    gfx::RenderingStats frame;
    frame.drawableWaitTime = 0.004;
    frame.gpuTime = 0.006;
    const auto printed = frame.toString(" ");
    EXPECT_NE(printed.find("drawableWaitTime"), std::string::npos);
    EXPECT_NE(printed.find("gpuTime"), std::string::npos);
}
