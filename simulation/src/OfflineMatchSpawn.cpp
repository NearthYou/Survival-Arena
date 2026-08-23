#include "OfflineMatchInternal.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <random>
#include <stdexcept>
#include <vector>

namespace dxa::simulation
{
namespace
{
constexpr float ContenderAngleJitter = 0.05F;
constexpr float AgentStoppingDistance = 0.1F;

class DeterministicRandom
{
public:
    explicit DeterministicRandom(const std::uint32_t seed)
        : engine_{seed}
    {
    }

    [[nodiscard]] std::uint32_t Next()
    {
        static_assert(
            std::mt19937::max() <= std::numeric_limits<std::uint32_t>::max());
        return static_cast<std::uint32_t>(engine_());
    }

    [[nodiscard]] float Range(const float minimum, const float maximum)
    {
        constexpr float InverseRange = 1.0F / 16777216.0F;
        const float unit = static_cast<float>(Next() >> 8U) * InverseRange;
        return minimum + (maximum - minimum) * unit;
    }

private:
    std::mt19937 engine_;
};

[[nodiscard]] bool IsOnNavMesh(const NavMesh& navMesh, const Vec2 position)
{
    return navMesh.FindContainingTriangleGrid(position).triangle.has_value();
}

[[nodiscard]] bool HasSpacing(
    const std::vector<CombatActor>& actors,
    const Vec2 candidate,
    const float minimumSpacing)
{
    for (const CombatActor& actor : actors)
    {
        if (Distance(actor.position, candidate) < minimumSpacing)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] Vec2 SpawnContenderPosition(
    const NavMesh& navMesh,
    const MatchConfig& config,
    DeterministicRandom& random,
    const std::vector<CombatActor>& actors,
    const std::uint32_t contenderIndex)
{
    constexpr float TwoPi = std::numbers::pi_v<float> * 2.0F;
    const float baseAngle = static_cast<float>(contenderIndex)
        / static_cast<float>(config.contenderCount)
        * TwoPi;
    for (std::uint32_t attempt = 0; attempt < config.maximumSpawnAttempts; ++attempt)
    {
        const float angle = baseAngle
            + random.Range(-ContenderAngleJitter, ContenderAngleJitter);
        const float radius = random.Range(
            config.contenderSpawnInnerRadius,
            config.contenderSpawnOuterRadius);
        const Vec2 candidate{
            std::cos(angle) * radius,
            std::sin(angle) * radius};
        if (IsOnNavMesh(navMesh, candidate)
            && HasSpacing(actors, candidate, config.contenderSpawnSpacing))
        {
            return candidate;
        }
    }
    throw std::runtime_error{"contender spawn attempts exhausted"};
}

[[nodiscard]] Vec2 SpawnNeutralPosition(
    const NavMesh& navMesh,
    const MatchConfig& config,
    DeterministicRandom& random,
    const std::vector<CombatActor>& actors)
{
    for (std::uint32_t attempt = 0; attempt < config.maximumSpawnAttempts; ++attempt)
    {
        const Vec2 candidate{
            random.Range(-config.arenaHalfExtent, config.arenaHalfExtent),
            random.Range(-config.arenaHalfExtent, config.arenaHalfExtent)};
        if (IsOnNavMesh(navMesh, candidate)
            && HasSpacing(actors, candidate, config.neutralSpawnSpacing))
        {
            return candidate;
        }
    }
    throw std::runtime_error{"neutral spawn attempts exhausted"};
}

[[nodiscard]] Vec2 SpawnLootPosition(
    const NavMesh& navMesh,
    const MatchConfig& config,
    DeterministicRandom& random)
{
    for (std::uint32_t attempt = 0; attempt < config.maximumSpawnAttempts; ++attempt)
    {
        const Vec2 candidate{
            random.Range(-config.arenaHalfExtent, config.arenaHalfExtent),
            random.Range(-config.arenaHalfExtent, config.arenaHalfExtent)};
        if (IsOnNavMesh(navMesh, candidate))
        {
            return candidate;
        }
    }
    throw std::runtime_error{"loot spawn attempts exhausted"};
}

void SpawnLootType(
    std::vector<LootItem>& loot,
    const LootType type,
    const std::uint32_t count,
    const NavMesh& navMesh,
    const MatchConfig& config,
    DeterministicRandom& random)
{
    for (std::uint32_t index = 0; index < count; ++index)
    {
        static_cast<void>(index);
        loot.push_back(LootItem{
            static_cast<LootId>(loot.size()),
            type,
            SpawnLootPosition(navMesh, config, random),
            true});
    }
}
} // namespace

void OfflineMatch::Impl::Spawn()
{
    if (!actors.empty()
        || !neutralArchetypes.empty()
        || !agents.empty()
        || !neutralControllers.empty()
        || !loot.empty())
    {
        throw std::logic_error{"offline match spawn state must be empty"};
    }

    DeterministicRandom random{config.seed};
    const std::size_t actorCount = static_cast<std::size_t>(config.contenderCount)
        + static_cast<std::size_t>(config.meleeNeutralCount)
        + static_cast<std::size_t>(config.rangedNeutralCount);
    const std::size_t lootCount = static_cast<std::size_t>(config.rifleLootCount)
        + static_cast<std::size_t>(config.arcPulseLootCount)
        + static_cast<std::size_t>(config.medKitLootCount);

    std::vector<CombatActor> spawnedActors;
    std::vector<NeutralArchetype> spawnedArchetypes;
    std::vector<NavAgent> spawnedAgents;
    std::vector<std::unique_ptr<BehaviorTreeAiController>> spawnedControllers;
    std::vector<LootItem> spawnedLoot;
    spawnedActors.reserve(actorCount);
    spawnedArchetypes.reserve(actorCount);
    spawnedAgents.reserve(actorCount);
    spawnedControllers.reserve(actorCount);
    spawnedLoot.reserve(lootCount);

    for (std::uint32_t index = 0; index < config.contenderCount; ++index)
    {
        const Vec2 position = SpawnContenderPosition(
            navMesh,
            config,
            random,
            spawnedActors,
            index);
        spawnedActors.push_back(CombatActor{
            static_cast<ActorId>(spawnedActors.size()),
            ActorRole::Contender,
            position,
            100,
            true,
            WeaponType::Blade});
        spawnedArchetypes.push_back(NeutralArchetype::None);
    }

    const auto spawnNeutrals = [&](
        const std::uint32_t count,
        const NeutralArchetype archetype,
        const std::int32_t health,
        const WeaponType weapon) {
        for (std::uint32_t index = 0; index < count; ++index)
        {
            static_cast<void>(index);
            const Vec2 position = SpawnNeutralPosition(
                navMesh,
                config,
                random,
                spawnedActors);
            spawnedActors.push_back(CombatActor{
                static_cast<ActorId>(spawnedActors.size()),
                ActorRole::Neutral,
                position,
                health,
                true,
                weapon});
            spawnedArchetypes.push_back(archetype);
        }
    };
    spawnNeutrals(
        config.meleeNeutralCount,
        NeutralArchetype::Melee,
        60,
        WeaponType::Blade);
    spawnNeutrals(
        config.rangedNeutralCount,
        NeutralArchetype::Ranged,
        45,
        WeaponType::Rifle);

    for (const CombatActor& actor : spawnedActors)
    {
        const float speed = actor.role == ActorRole::Contender
            ? config.contenderSpeed
            : config.neutralSpeed;
        spawnedAgents.emplace_back(
            navMesh,
            actor.position,
            speed,
            AgentStoppingDistance);
    }
    for (const NeutralArchetype archetype : spawnedArchetypes)
    {
        if (archetype == NeutralArchetype::None)
        {
            spawnedControllers.push_back(nullptr);
            continue;
        }
        const AiArchetype aiArchetype = archetype == NeutralArchetype::Melee
            ? AiArchetype::Melee
            : AiArchetype::Ranged;
        spawnedControllers.push_back(
            std::make_unique<BehaviorTreeAiController>(aiArchetype));
    }

    SpawnLootType(
        spawnedLoot,
        LootType::Rifle,
        config.rifleLootCount,
        navMesh,
        config,
        random);
    SpawnLootType(
        spawnedLoot,
        LootType::ArcPulse,
        config.arcPulseLootCount,
        navMesh,
        config,
        random);
    SpawnLootType(
        spawnedLoot,
        LootType::MedKit,
        config.medKitLootCount,
        navMesh,
        config,
        random);

    actors.swap(spawnedActors);
    neutralArchetypes.swap(spawnedArchetypes);
    agents.swap(spawnedAgents);
    neutralControllers.swap(spawnedControllers);
    loot.swap(spawnedLoot);
}
} // namespace dxa::simulation
