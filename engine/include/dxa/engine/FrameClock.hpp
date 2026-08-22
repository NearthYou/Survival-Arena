#pragma once

#include <chrono>
#include <cstdint>

namespace dxa::engine
{
struct FrameTiming
{
    double deltaSeconds = 0.0;
    double totalSeconds = 0.0;
    std::uint64_t frameIndex = 0;
};

class FrameClock
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    explicit FrameClock(
        const TimePoint start,
        const Duration maxDelta = std::chrono::milliseconds{250}) noexcept
        : start_(start), last_(start), maxDelta_(maxDelta)
    {
    }

    [[nodiscard]] FrameTiming Tick(const TimePoint now) noexcept
    {
        ++frameIndex_;

        if (now < last_)
        {
            return FrameTiming{0.0, totalSeconds_, frameIndex_};
        }

        const Duration elapsed = now - last_;
        const Duration clamped = elapsed > maxDelta_ ? maxDelta_ : elapsed;
        last_ = now;
        totalSeconds_ = std::chrono::duration<double>(now - start_).count();

        return FrameTiming{
            std::chrono::duration<double>(clamped).count(),
            totalSeconds_,
            frameIndex_};
    }

private:
    TimePoint start_;
    TimePoint last_;
    Duration maxDelta_;
    double totalSeconds_ = 0.0;
    std::uint64_t frameIndex_ = 0;
};
} // namespace dxa::engine
