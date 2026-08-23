#pragma once

#include <dxa/engine/RenderPass.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace dxa::engine::benchmark
{
struct GpuPassDurations
{
    std::optional<double> totalMilliseconds;
    std::optional<double> forwardMilliseconds;
    std::optional<double> shadowMilliseconds;
    std::optional<double> gBufferMilliseconds;
    std::optional<double> lightingMilliseconds;
    std::optional<double> transparentMilliseconds;
};

struct TimestampSequence
{
    std::uint64_t frequency = 0;
    std::uint64_t start = 0;
    std::array<std::uint64_t, 4> markers{};
    std::uint64_t end = 0;
    std::size_t markerCount = 0;
};

[[nodiscard]] GpuPassDurations CalculatePassDurations(
    const TimestampSequence& sequence,
    std::span<const dxa::engine::RenderPass> passes);
} // namespace dxa::engine::benchmark
