#pragma once

#include <dxa/simulation/MatchTypes.hpp>

#include <cstdint>

namespace dxa::simulation
{
struct SafeZoneState
{
    SafeZoneStage stage = SafeZoneStage::Stage1;
    Vec2 center{0.0F, 0.0F};
    float radius = 128.0F;
    std::int32_t damagePerSecond = 2;

    [[nodiscard]] bool operator==(const SafeZoneState&) const = default;
};

[[nodiscard]] SafeZoneState EvaluateSafeZone(
    std::uint32_t tick,
    std::uint32_t tickRate);
[[nodiscard]] bool IsOutsideSafeZone(Vec2 position, const SafeZoneState& state);
[[nodiscard]] std::int32_t SafeZoneDamageForTick(
    std::uint32_t tick,
    std::uint32_t tickRate);
} // namespace dxa::simulation
