#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace dxa::game_server
{
struct TickAdvanceResult
{
    std::uint32_t ticksDue = 0U;
    bool rebased = false;
    std::chrono::steady_clock::duration lateness{};
};

class FixedTickScheduler
{
public:
    FixedTickScheduler(
        std::uint32_t tickRate,
        std::uint32_t maximumCatchUpTicks);

    void Start(std::chrono::steady_clock::time_point now);
    [[nodiscard]] TickAdvanceResult Advance(
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::chrono::steady_clock::time_point
    NextDeadline() const;

private:
    [[nodiscard]] std::chrono::steady_clock::duration OffsetFor(
        std::uint64_t ordinal) const;

    std::uint32_t tickRate_ = 0U;
    std::uint32_t maximumCatchUpTicks_ = 0U;
    std::optional<std::chrono::steady_clock::time_point> epoch_;
    std::uint64_t nextOrdinal_ = 1U;
};
} // namespace dxa::game_server
