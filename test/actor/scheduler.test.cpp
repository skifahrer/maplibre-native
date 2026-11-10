#include <mln/actor/scheduler.hpp>

#include <gtest/gtest.h>

#include <mapbox/std/weak.hpp>

using namespace mln;

namespace {

/// Runs every task at once, so a reply follows its task on the same call.
class InlineScheduler : public Scheduler {
public:
    void schedule(std::function<void()>&& fn) override { fn(); }
    void schedule(const util::SimpleIdentity, std::function<void()>&& fn) override { fn(); }
    mapbox::base::WeakPtr<Scheduler> makeWeakPtr() override { return weakFactory.makeWeakPtr(); }
    void waitForEmpty(const util::SimpleIdentity) override {}

private:
    mapbox::base::WeakPtrFactory<Scheduler> weakFactory{this};
};

struct Counted {
    explicit Counted(int* copies_) : copies(copies_) {}
    Counted(const Counted& other) : copies(other.copies) { ++*copies; }
    Counted(Counted&&) noexcept = default;
    Counted& operator=(const Counted&) = delete;
    Counted& operator=(Counted&&) = delete;
    int* copies;
};

} // namespace

// 108 TestFlight ACoV: a GeoJSON tile's features were copied again for the reply.
TEST(Scheduler, ReplyTakesTheTaskResultWithoutACopy) {
    InlineScheduler scheduler;
    int copies = 0;
    bool replied = false;
    Scheduler::SetCurrent(&scheduler);
    scheduler.scheduleAndReplyValue(
        util::SimpleIdentity::Empty, [&copies] { return Counted(&copies); }, [&replied](Counted) { replied = true; });
    Scheduler::SetCurrent(nullptr);
    EXPECT_TRUE(replied);
    EXPECT_EQ(0, copies);
}
