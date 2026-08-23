#include <dxa/engine/benchmark/GpuPassTiming.hpp>

#include <array>
#include <stdexcept>

namespace dxa::engine::benchmark
{
namespace
{
[[nodiscard]] double ToMilliseconds(
    const std::uint64_t start,
    const std::uint64_t end,
    const std::uint64_t frequency)
{
    return static_cast<double>(end - start)
        * 1000.0
        / static_cast<double>(frequency);
}
} // namespace

GpuPassDurations CalculatePassDurations(
    const TimestampSequence& sequence,
    const std::span<const dxa::engine::RenderPass> passes)
{
    if (sequence.frequency == 0
        || sequence.end < sequence.start
        || sequence.markerCount > sequence.markers.size()
        || sequence.markerCount != passes.size())
    {
        throw std::invalid_argument{"invalid GPU timestamp sequence"};
    }

    GpuPassDurations durations;
    durations.totalMilliseconds = ToMilliseconds(
        sequence.start,
        sequence.end,
        sequence.frequency);
    if (sequence.markerCount == 0)
    {
        durations.forwardMilliseconds = durations.totalMilliseconds;
        return durations;
    }

    constexpr std::array ExpectedPasses{
        dxa::engine::RenderPass::Shadow,
        dxa::engine::RenderPass::GBuffer,
        dxa::engine::RenderPass::DeferredLighting,
        dxa::engine::RenderPass::Transparent};
    if (sequence.markerCount != ExpectedPasses.size())
    {
        throw std::invalid_argument{"hybrid GPU timing requires four pass markers"};
    }
    for (std::size_t index = 0; index < ExpectedPasses.size(); ++index)
    {
        if (passes[index] != ExpectedPasses[index])
        {
            throw std::invalid_argument{"hybrid GPU pass markers are out of order"};
        }
    }

    std::uint64_t previous = sequence.start;
    std::array<double, 4> milliseconds{};
    for (std::size_t index = 0; index < sequence.markerCount; ++index)
    {
        const std::uint64_t marker = sequence.markers[index];
        if (marker < previous || marker > sequence.end)
        {
            throw std::invalid_argument{"GPU pass timestamps are not monotonic"};
        }
        milliseconds[index] = ToMilliseconds(previous, marker, sequence.frequency);
        previous = marker;
    }

    durations.shadowMilliseconds = milliseconds[0];
    durations.gBufferMilliseconds = milliseconds[1];
    durations.lightingMilliseconds = milliseconds[2];
    durations.transparentMilliseconds = milliseconds[3];
    return durations;
}
} // namespace dxa::engine::benchmark
