#include <dxa/protocol/DatagramShaper.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace dxa::protocol
{
namespace
{
inline constexpr std::uint64_t DirectionMultiplier =
    0xD6E8FEB86659FD93ULL;
inline constexpr std::uint64_t PeerMultiplier =
    0xA0761D6478BD642FULL;
inline constexpr std::uint64_t OrdinalMultiplier =
    0xE7037ED1A0B428DBULL;

[[nodiscard]] constexpr std::uint64_t SplitMix64(
    std::uint64_t value) noexcept
{
    std::uint64_t mixed = value + 0x9E3779B97F4A7C15ULL;
    mixed = (mixed ^ (mixed >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    mixed = (mixed ^ (mixed >> 27U)) * 0x94D049BB133111EBULL;
    return mixed ^ (mixed >> 31U);
}

[[nodiscard]] bool IsValidDirection(
    const DatagramDirection direction) noexcept
{
    return direction == DatagramDirection::ClientToServer
        || direction == DatagramDirection::ServerToClient;
}
} // namespace

DatagramShaper::DatagramShaper(
    DatagramShaperConfig config,
    const DatagramDirection direction)
    : config_{std::move(config)},
      direction_{direction}
{
    constexpr auto MaximumDuration = std::chrono::milliseconds{5000};
    if (!IsValidDirection(direction_)
        || config_.oneWayLatency < std::chrono::milliseconds::zero()
        || config_.jitter < std::chrono::milliseconds::zero()
        || config_.oneWayLatency > MaximumDuration
        || config_.jitter > MaximumDuration
        || config_.lossBasisPoints > 10000U)
    {
        throw std::invalid_argument{"datagram shaper configuration is invalid"};
    }

    enabled_ = config_.oneWayLatency != std::chrono::milliseconds::zero()
        || config_.jitter != std::chrono::milliseconds::zero()
        || config_.lossBasisPoints != 0U;
    if (enabled_ && config_.seed == 0U)
    {
        throw std::invalid_argument{
            "enabled datagram shaper requires a nonzero seed"};
    }
}

ShapedDatagramDecision DatagramShaper::Decide(
    const std::uint64_t peerKey,
    const std::uint64_t ordinal) const noexcept
{
    if (!enabled_)
    {
        return {};
    }

    std::uint64_t input = config_.seed;
    input ^= static_cast<std::uint64_t>(direction_) * DirectionMultiplier;
    input ^= peerKey * PeerMultiplier;
    input ^= ordinal * OrdinalMultiplier;
    const std::uint64_t mixed = SplitMix64(input);

    ShapedDatagramDecision decision;
    decision.drop = mixed % 10000U < config_.lossBasisPoints;
    const std::int64_t jitter = config_.jitter.count();
    const std::uint64_t jitterSpan = static_cast<std::uint64_t>(
        jitter * 2 + 1);
    const std::int64_t jitterOffset = static_cast<std::int64_t>(
        SplitMix64(mixed) % jitterSpan)
        - jitter;
    const std::int64_t delay = std::max<std::int64_t>(
        0,
        config_.oneWayLatency.count() + jitterOffset);
    decision.delay = std::chrono::milliseconds{delay};
    return decision;
}

bool DatagramShaper::Enabled() const noexcept
{
    return enabled_;
}
} // namespace dxa::protocol
