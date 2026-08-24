#include <dxa/game_server/FixedTickScheduler.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>

namespace
{
using dxa::game_server::FixedTickScheduler;

[[nodiscard]] std::chrono::steady_clock::time_point TimeNs(
    const std::uint64_t nanoseconds)
{
    return std::chrono::steady_clock::time_point{
        std::chrono::nanoseconds{static_cast<std::int64_t>(nanoseconds)}};
}
} // namespace

TEST(FixedTickScheduler, ProducesThirtyTicksAtOneSecondWithoutAccumulatedDrift)
{
    FixedTickScheduler scheduler{30U, 5U};
    scheduler.Start(TimeNs(0U));
    std::uint32_t total = 0U;
    for (std::uint32_t frame = 1U; frame <= 30U; ++frame)
    {
        const auto result = scheduler.Advance(
            TimeNs((1000000000ULL * frame) / 30ULL));
        total += result.ticksDue;
        EXPECT_FALSE(result.rebased);
    }

    EXPECT_EQ(30U, total);
    EXPECT_EQ(TimeNs(1033333333ULL), scheduler.NextDeadline());
}

TEST(FixedTickScheduler, EarlyTimerWaitsUntilExactFirstDeadline)
{
    FixedTickScheduler scheduler{30U, 5U};
    scheduler.Start(TimeNs(100U));

    const auto early = scheduler.Advance(TimeNs(33333432ULL));
    EXPECT_EQ(0U, early.ticksDue);
    EXPECT_FALSE(early.rebased);
    EXPECT_EQ(std::chrono::steady_clock::duration::zero(), early.lateness);
    EXPECT_EQ(TimeNs(33333433ULL), scheduler.NextDeadline());

    const auto exact = scheduler.Advance(TimeNs(33333433ULL));
    EXPECT_EQ(1U, exact.ticksDue);
    EXPECT_FALSE(exact.rebased);
    EXPECT_EQ(TimeNs(66666766ULL), scheduler.NextDeadline());
}

TEST(FixedTickScheduler, TwoLateTicksCatchUpWithoutRebase)
{
    FixedTickScheduler scheduler{30U, 5U};
    scheduler.Start(TimeNs(0U));

    const auto result = scheduler.Advance(TimeNs(80000000ULL));

    EXPECT_EQ(2U, result.ticksDue);
    EXPECT_FALSE(result.rebased);
    EXPECT_EQ(std::chrono::steady_clock::duration::zero(), result.lateness);
    EXPECT_EQ(TimeNs(100000000ULL), scheduler.NextDeadline());
}

TEST(FixedTickScheduler, CapsCatchUpAndRebasesAfterOverrun)
{
    FixedTickScheduler scheduler{30U, 5U};
    scheduler.Start(TimeNs(0U));

    const auto result = scheduler.Advance(TimeNs(1000000000ULL));

    EXPECT_EQ(5U, result.ticksDue);
    EXPECT_TRUE(result.rebased);
    EXPECT_EQ(std::chrono::milliseconds{800}, result.lateness);
    EXPECT_EQ(TimeNs(1033333333ULL), scheduler.NextDeadline());

    const auto firstAfterRebase = scheduler.Advance(scheduler.NextDeadline());
    EXPECT_EQ(1U, firstAfterRebase.ticksDue);
    EXPECT_FALSE(firstAfterRebase.rebased);
}

TEST(FixedTickScheduler, SupportsAnotherIntegralTickRateWithoutDrift)
{
    FixedTickScheduler scheduler{60U, 5U};
    scheduler.Start(TimeNs(0U));
    std::uint32_t total = 0U;
    for (std::uint32_t frame = 1U; frame <= 60U; ++frame)
    {
        total += scheduler.Advance(
            TimeNs((1000000000ULL * frame) / 60ULL)).ticksDue;
    }

    EXPECT_EQ(60U, total);
}

TEST(FixedTickScheduler, RejectsInvalidConstructionAndLifecycleCalls)
{
    EXPECT_THROW((void)FixedTickScheduler(0U, 5U), std::invalid_argument);
    EXPECT_THROW((void)FixedTickScheduler(30U, 0U), std::invalid_argument);

    FixedTickScheduler scheduler{30U, 5U};
    EXPECT_THROW((void)scheduler.Advance(TimeNs(0U)), std::logic_error);
    EXPECT_THROW((void)scheduler.NextDeadline(), std::logic_error);
    scheduler.Start(TimeNs(0U));
    EXPECT_THROW(scheduler.Start(TimeNs(1U)), std::logic_error);
}
