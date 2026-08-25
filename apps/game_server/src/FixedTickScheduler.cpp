#include <dxa/game_server/FixedTickScheduler.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace dxa::game_server
{
namespace
{
constexpr std::uint64_t NanosecondsPerSecond = 1000000000ULL;
}

FixedTickScheduler::FixedTickScheduler(
    const std::uint32_t tickRate,
    const std::uint32_t maximumCatchUpTicks)
    : tickRate_{tickRate},
      maximumCatchUpTicks_{maximumCatchUpTicks}
{
    if (tickRate_ == 0U || maximumCatchUpTicks_ == 0U)
    {
        throw std::invalid_argument{
            "fixed tick rate and catch-up limit must be positive"};
    }
}

void FixedTickScheduler::Start(
    const std::chrono::steady_clock::time_point now)
{
    if (epoch_.has_value())
    {
        throw std::logic_error{"fixed tick scheduler can start only once"};
    }
    epoch_ = now;
    nextOrdinal_ = 1U;
}

TickAdvanceResult FixedTickScheduler::Advance(
    const std::chrono::steady_clock::time_point now)
{
    if (!epoch_.has_value())
    {
        throw std::logic_error{"fixed tick scheduler has not started"};
    }

    TickAdvanceResult result;
    while (result.ticksDue < maximumCatchUpTicks_
           && NextDeadline() <= now)
    {
        ++result.ticksDue;
        if (nextOrdinal_ == std::numeric_limits<std::uint64_t>::max())
        {
            throw std::overflow_error{"fixed tick ordinal exhausted"};
        }
        ++nextOrdinal_;
    }

    const auto firstUnprocessedDeadline = NextDeadline();
    if (result.ticksDue == maximumCatchUpTicks_
        && firstUnprocessedDeadline <= now)
    {
        result.rebased = true;
        result.lateness = now - firstUnprocessedDeadline;
        epoch_ = now;
        nextOrdinal_ = 1U;
    }
    return result;
}

std::chrono::steady_clock::time_point
FixedTickScheduler::NextDeadline() const
{
    if (!epoch_.has_value())
    {
        throw std::logic_error{"fixed tick scheduler has not started"};
    }
    return *epoch_ + OffsetFor(nextOrdinal_);
}

std::chrono::steady_clock::duration FixedTickScheduler::OffsetFor(
    const std::uint64_t ordinal) const
{
    const std::uint64_t wholeSeconds = ordinal / tickRate_;
    if (wholeSeconds
        > static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max()))
    {
        throw std::overflow_error{"fixed tick deadline exhausted"};
    }
    const std::uint64_t remainder = ordinal % tickRate_;
    const std::uint64_t fractionalNanoseconds =
        (NanosecondsPerSecond * remainder) / tickRate_;
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
               std::chrono::seconds{
                   static_cast<std::int64_t>(wholeSeconds)})
        + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
               std::chrono::nanoseconds{
                   static_cast<std::int64_t>(fractionalNanoseconds)});
}
} // namespace dxa::game_server
