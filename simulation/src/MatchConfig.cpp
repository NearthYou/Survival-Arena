#include <dxa/simulation/MatchConfig.hpp>

#include <dxa/simulation/MatchTypes.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace dxa::simulation
{
namespace
{
[[nodiscard]] bool IsFinitePositive(const float value) noexcept
{
    return std::isfinite(value) && value > 0.0F;
}

[[nodiscard]] std::uint64_t ActorCount(const MatchConfig& config) noexcept
{
    return static_cast<std::uint64_t>(config.contenderCount)
        + static_cast<std::uint64_t>(config.meleeNeutralCount)
        + static_cast<std::uint64_t>(config.rangedNeutralCount);
}

[[nodiscard]] std::uint64_t LootCount(const MatchConfig& config) noexcept
{
    return static_cast<std::uint64_t>(config.rifleLootCount)
        + static_cast<std::uint64_t>(config.arcPulseLootCount)
        + static_cast<std::uint64_t>(config.medKitLootCount);
}
} // namespace

MatchConfig DefaultMatchConfig() noexcept
{
    return MatchConfig{};
}

void ValidateMatchConfig(const MatchConfig& config)
{
    if (config.tickRate != 30U
        || config.botDecisionIntervalTicks != 6U
        || config.suddenDeathTick != 14400U
        || config.hardTimeoutTick != 18000U)
    {
        throw std::invalid_argument{"match tick contract must remain 30 Hz and 600 seconds"};
    }
    if (config.contenderCount < 2U || config.maximumSpawnAttempts == 0U)
    {
        throw std::invalid_argument{"match requires two contenders and bounded spawn attempts"};
    }

    constexpr std::uint64_t ActorIdCapacity =
        static_cast<std::uint64_t>(std::numeric_limits<ActorId>::max()) + 1ULL;
    constexpr std::uint64_t LootIdCapacity =
        static_cast<std::uint64_t>(std::numeric_limits<LootId>::max()) + 1ULL;
    if (ActorCount(config) > ActorIdCapacity || LootCount(config) > LootIdCapacity)
    {
        throw std::invalid_argument{"match population exceeds numeric ID capacity"};
    }

    if (!IsFinitePositive(config.contenderSpeed)
        || !IsFinitePositive(config.neutralSpeed)
        || !IsFinitePositive(config.contenderPerceptionRadius)
        || !IsFinitePositive(config.neutralPerceptionRadius)
        || !IsFinitePositive(config.pickupRadius)
        || !IsFinitePositive(config.contenderSpawnSpacing)
        || !IsFinitePositive(config.neutralSpawnSpacing))
    {
        throw std::invalid_argument{"match movement and query values must be finite and positive"};
    }

    if (!std::isfinite(config.contenderSpawnInnerRadius)
        || !std::isfinite(config.contenderSpawnOuterRadius)
        || config.contenderSpawnInnerRadius < 0.0F
        || config.contenderSpawnOuterRadius < config.contenderSpawnInnerRadius)
    {
        throw std::invalid_argument{"contender spawn radii are invalid"};
    }
}
} // namespace dxa::simulation
