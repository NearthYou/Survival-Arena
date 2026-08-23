#include <dxa/simulation/SafeZone.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace dxa::simulation
{
namespace
{
struct SafeZoneStageDefinition
{
    SafeZoneStage stage = SafeZoneStage::Stage1;
    std::uint32_t startSecond = 0;
    std::uint32_t endSecond = 0;
    float startRadius = 0.0F;
    float endRadius = 0.0F;
    std::int32_t damagePerSecond = 0;
};

constexpr std::array SafeZoneStages{
    SafeZoneStageDefinition{SafeZoneStage::Stage1, 0U, 120U, 32.0F, 24.0F, 2},
    SafeZoneStageDefinition{SafeZoneStage::Stage2, 120U, 240U, 24.0F, 16.0F, 4},
    SafeZoneStageDefinition{SafeZoneStage::Stage3, 240U, 360U, 16.0F, 8.0F, 8},
    SafeZoneStageDefinition{SafeZoneStage::Stage4, 360U, 480U, 8.0F, 2.0F, 16},
    SafeZoneStageDefinition{
        SafeZoneStage::SuddenDeath,
        480U,
        600U,
        2.0F,
        0.0F,
        32}};

void ValidateStage(const SafeZoneStage stage)
{
    switch (stage)
    {
    case SafeZoneStage::Stage1:
    case SafeZoneStage::Stage2:
    case SafeZoneStage::Stage3:
    case SafeZoneStage::Stage4:
    case SafeZoneStage::SuddenDeath:
        return;
    }
    throw std::invalid_argument{"safe-zone stage is invalid"};
}

void ValidateState(const SafeZoneState& state)
{
    ValidateStage(state.stage);
    if (!IsFinite(state.center)
        || !std::isfinite(state.radius)
        || state.radius < 0.0F
        || state.damagePerSecond < 0)
    {
        throw std::invalid_argument{"safe-zone state is invalid"};
    }
}
} // namespace

SafeZoneState EvaluateSafeZone(
    const std::uint32_t tick,
    const std::uint32_t tickRate)
{
    if (tickRate == 0U)
    {
        throw std::invalid_argument{"safe-zone tick rate must be positive"};
    }

    const std::uint64_t currentTick = tick;
    for (const SafeZoneStageDefinition& definition : SafeZoneStages)
    {
        const std::uint64_t startTick =
            static_cast<std::uint64_t>(definition.startSecond) * tickRate;
        const std::uint64_t endTick =
            static_cast<std::uint64_t>(definition.endSecond) * tickRate;
        if (currentTick < endTick)
        {
            const double fraction = static_cast<double>(currentTick - startTick)
                / static_cast<double>(endTick - startTick);
            const double radius = static_cast<double>(definition.startRadius)
                + (static_cast<double>(definition.endRadius)
                   - static_cast<double>(definition.startRadius))
                    * fraction;
            return SafeZoneState{
                definition.stage,
                {0.0F, 0.0F},
                static_cast<float>(radius),
                definition.damagePerSecond};
        }
    }

    const SafeZoneStageDefinition& finalStage = SafeZoneStages.back();
    return SafeZoneState{
        finalStage.stage,
        {0.0F, 0.0F},
        finalStage.endRadius,
        finalStage.damagePerSecond};
}

bool IsOutsideSafeZone(const Vec2 position, const SafeZoneState& state)
{
    if (!IsFinite(position))
    {
        throw std::invalid_argument{"safe-zone query position must be finite"};
    }
    ValidateState(state);

    const double deltaX = static_cast<double>(position.x)
        - static_cast<double>(state.center.x);
    const double deltaZ = static_cast<double>(position.z)
        - static_cast<double>(state.center.z);
    const double radius = state.radius;
    return deltaX * deltaX + deltaZ * deltaZ > radius * radius;
}

std::int32_t SafeZoneDamageForTick(
    const std::uint32_t tick,
    const std::uint32_t tickRate)
{
    if (tickRate == 0U)
    {
        throw std::invalid_argument{"safe-zone tick rate must be positive"};
    }
    if (tick == 0U || tick % tickRate != 0U)
    {
        return 0;
    }
    return EvaluateSafeZone(tick, tickRate).damagePerSecond;
}
} // namespace dxa::simulation
