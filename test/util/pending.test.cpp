#include <mln/util/pending.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

using namespace mln::util;

// 96 a value set before anyone waits is there at once, and the first one set stays
TEST(Pending, KeepsTheFirstValue) {
    Pending<std::string> pending;
    EXPECT_FALSE(pending.isSet());
    EXPECT_TRUE(pending.set("vertex"));
    EXPECT_FALSE(pending.set("fragment"));
    EXPECT_TRUE(pending.isSet());
    EXPECT_EQ("vertex", pending.wait());
}

// 96 a shader the draw needs before Metal is done waits for the compile's thread
TEST(Pending, WaitsForAnotherThread) {
    Pending<int> pending;
    std::atomic<bool> waited{false};
    std::thread reader([&] {
        EXPECT_EQ(42, pending.wait());
        waited = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(waited);
    pending.set(42);
    reader.join();
    EXPECT_TRUE(waited);
}

// 96 every draw waiting on one shader is let go by the single compile
TEST(Pending, ReleasesEveryWaiter) {
    Pending<int> pending;
    std::atomic<int> released{0};
    std::thread first([&] { released += pending.wait(); });
    std::thread second([&] { released += pending.wait(); });
    pending.set(1);
    first.join();
    second.join();
    EXPECT_EQ(2, released);
}
