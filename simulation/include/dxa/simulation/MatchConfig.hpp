#pragma once

#include <cstdint>

namespace dxa::simulation
{
struct MatchConfig
{
    std::uint32_t tickRate = 30U;
    std::uint32_t contenderCount = 24U;
    std::uint32_t meleeNeutralCount = 50U;
    std::uint32_t rangedNeutralCount = 50U;
    std::uint32_t rifleLootCount = 24U;
    std::uint32_t arcPulseLootCount = 12U;
    std::uint32_t medKitLootCount = 24U;
    std::uint32_t botDecisionIntervalTicks = 6U;
    bool enableInternalBots = true;
    std::uint32_t maximumSpawnAttempts = 4096U;
    std::uint32_t suddenDeathTick = 14400U;
    std::uint32_t hardTimeoutTick = 18000U;
    std::uint32_t seed = 20260823U;
    float contenderSpeed = 6.0F;
    float neutralSpeed = 4.5F;
    float contenderPerceptionRadius = 18.0F;
    float neutralPerceptionRadius = 10.0F;
    float pickupRadius = 1.0F;
    float contenderSpawnSpacing = 3.0F;
    float neutralSpawnSpacing = 0.75F;
    float arenaHalfExtent = 128.0F;
    float contenderSpawnInnerRadius = 80.0F;
    float contenderSpawnOuterRadius = 104.0F;

    [[nodiscard]] bool operator==(const MatchConfig&) const = default;
};

[[nodiscard]] MatchConfig DefaultMatchConfig() noexcept;
void ValidateMatchConfig(const MatchConfig& config);
} // namespace dxa::simulation
