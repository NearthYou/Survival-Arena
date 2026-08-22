#include <dxa/engine/FrameClock.hpp>

#include <gtest/gtest.h>

#include <chrono>

namespace
{
using namespace std::chrono_literals;
using dxa::engine::FrameClock;

TEST(FrameClock, ReportsElapsedTimeAndFrameIndex)
{
    const FrameClock::TimePoint start{};
    FrameClock clock{start};

    const auto first = clock.Tick(start + 16ms);
    const auto second = clock.Tick(start + 36ms);

    EXPECT_NEAR(0.016, first.deltaSeconds, 0.000001);
    EXPECT_NEAR(0.036, second.totalSeconds, 0.000001);
    EXPECT_EQ(1U, first.frameIndex);
    EXPECT_EQ(2U, second.frameIndex);
}

TEST(FrameClock, ClampsLongFrameWithoutHidingWallTime)
{
    const FrameClock::TimePoint start{};
    FrameClock clock{start, 250ms};

    const auto timing = clock.Tick(start + 1s);

    EXPECT_NEAR(0.250, timing.deltaSeconds, 0.000001);
    EXPECT_NEAR(1.000, timing.totalSeconds, 0.000001);
}

TEST(FrameClock, IgnoresTimestampThatMovesBackward)
{
    const FrameClock::TimePoint start{};
    FrameClock clock{start};
    (void)clock.Tick(start + 20ms);

    const auto timing = clock.Tick(start + 10ms);

    EXPECT_DOUBLE_EQ(0.0, timing.deltaSeconds);
    EXPECT_NEAR(0.020, timing.totalSeconds, 0.000001);
}
} // namespace

