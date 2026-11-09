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

// 97 a draw whose shader is still compiling gives up after its short wait
TEST(Pending, WaitForGivesUpWhileUnset) {
    Pending<int> pending;
    const auto began = std::chrono::steady_clock::now();
    EXPECT_EQ(nullptr, pending.waitFor(std::chrono::milliseconds(5)));
    EXPECT_GE(std::chrono::steady_clock::now() - began, std::chrono::milliseconds(5));
}

// 97 a compile that lands within the wait still draws in the same frame
TEST(Pending, WaitForTakesAValueSetMeanwhile) {
    Pending<int> pending;
    std::thread compiler([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        pending.set(7);
    });
    const auto* value = pending.waitFor(std::chrono::seconds(5));
    compiler.join();
    ASSERT_NE(nullptr, value);
    EXPECT_EQ(7, *value);
}
