#include <dxa/simulation/OfflineMatch.hpp>

#include <dxa/simulation/Math2.hpp>
#include <dxa/simulation/NavMesh.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace
{
using dxa::simulation::ActorId;
using dxa::simulation::ActorRole;
using dxa::simulation::ActorSnapshot;
using dxa::simulation::DefaultMatchConfig;
using dxa::simulation::Distance;
using dxa::simulation::LootType;
using dxa::simulation::MatchCommand;
using dxa::simulation::MatchConfig;
using dxa::simulation::MatchPhase;
using dxa::simulation::MatchSnapshot;
using dxa::simulation::NavMesh;
using dxa::simulation::NavTriangleIndices;
using dxa::simulation::NeutralArchetype;
using dxa::simulation::OfflineMatch;
using dxa::simulation::WeaponType;

[[nodiscard]] NavMesh MakeArenaNavMesh()
{
    return NavMesh::Build(
        {
            {-32.0F, -32.0F},
            {32.0F, -32.0F},
            {-32.0F, 32.0F},
            {32.0F, 32.0F}
        },
        {
            NavTriangleIndices{{0U, 1U, 2U}},
            NavTriangleIndices{{1U, 3U, 2U}}
        },
        4.0F);
}

[[nodiscard]] NavMesh MakeTinyNavMesh()
{
    return NavMesh::Build(
        {
            {-0.1F, -0.1F},
            {0.1F, -0.1F},
            {-0.1F, 0.1F},
            {0.1F, 0.1F}
        },
        {
            NavTriangleIndices{{0U, 1U, 2U}},
            NavTriangleIndices{{1U, 3U, 2U}}
        },
        0.1F);
}

[[nodiscard]] std::size_t CountRole(
    const MatchSnapshot& snapshot,
    const ActorRole role)
{
    return static_cast<std::size_t>(std::count_if(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        [role](const ActorSnapshot& actor) { return actor.role == role; }));
}

[[nodiscard]] std::size_t CountNeutral(
    const MatchSnapshot& snapshot,
    const NeutralArchetype archetype)
{
    return static_cast<std::size_t>(std::count_if(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        [archetype](const ActorSnapshot& actor) {
            return actor.neutralArchetype == archetype;
        }));
}

[[nodiscard]] std::size_t CountLoot(
    const MatchSnapshot& snapshot,
    const LootType type)
{
    return static_cast<std::size_t>(std::count_if(
        snapshot.loot.begin(),
        snapshot.loot.end(),
        [type](const auto& loot) { return loot.type == type; }));
}

[[nodiscard]] bool AllNeutralStatsMatch(
    const MatchSnapshot& snapshot,
    const NeutralArchetype archetype,
    const WeaponType weapon,
    const int health)
{
    return std::all_of(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        [=](const ActorSnapshot& actor) {
            return actor.neutralArchetype != archetype
                || (actor.role == ActorRole::Neutral
                    && actor.weapon == weapon
                    && actor.health == health
                    && actor.alive);
        });
}

[[nodiscard]] bool AllSpawnedOnNavMesh(
    const MatchSnapshot& snapshot,
    const NavMesh& navMesh)
{
    const bool actorsOnMesh = std::all_of(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        [&navMesh](const ActorSnapshot& actor) {
            return navMesh.FindContainingTriangleGrid(actor.position).triangle.has_value();
        });
    const bool lootOnMesh = std::all_of(
        snapshot.loot.begin(),
        snapshot.loot.end(),
        [&navMesh](const auto& loot) {
            return navMesh.FindContainingTriangleGrid(loot.position).triangle.has_value();
        });
    return actorsOnMesh && lootOnMesh;
}

[[nodiscard]] MatchSnapshot StartSnapshot(const std::uint32_t seed)
{
    MatchConfig config = DefaultMatchConfig();
    config.seed = seed;
    OfflineMatch match = OfflineMatch::Create(MakeArenaNavMesh(), config);
    match.Start();
    return match.Snapshot();
}

TEST(OfflineMatch, StartsCanonicalPopulationOnNavMesh)
{
    const NavMesh navMesh = MakeArenaNavMesh();
    OfflineMatch match = OfflineMatch::Create(navMesh, DefaultMatchConfig());

    match.Start();
    const MatchSnapshot snapshot = match.Snapshot();

    EXPECT_EQ(MatchPhase::Running, snapshot.phase);
    EXPECT_EQ(0U, snapshot.tick);
    EXPECT_DOUBLE_EQ(0.0, snapshot.elapsedSeconds);
    EXPECT_EQ(24U, snapshot.aliveContenders);
    EXPECT_EQ(24U, CountRole(snapshot, ActorRole::Contender));
    EXPECT_EQ(100U, CountRole(snapshot, ActorRole::Neutral));
    EXPECT_EQ(50U, CountNeutral(snapshot, NeutralArchetype::Melee));
    EXPECT_EQ(50U, CountNeutral(snapshot, NeutralArchetype::Ranged));
    EXPECT_TRUE(AllNeutralStatsMatch(
        snapshot,
        NeutralArchetype::Melee,
        WeaponType::Blade,
        60));
    EXPECT_TRUE(AllNeutralStatsMatch(
        snapshot,
        NeutralArchetype::Ranged,
        WeaponType::Rifle,
        45));
    EXPECT_EQ(60U, snapshot.loot.size());
    EXPECT_EQ(24U, CountLoot(snapshot, LootType::Rifle));
    EXPECT_EQ(12U, CountLoot(snapshot, LootType::ArcPulse));
    EXPECT_EQ(24U, CountLoot(snapshot, LootType::MedKit));
    EXPECT_TRUE(AllSpawnedOnNavMesh(snapshot, navMesh));
    EXPECT_FALSE(snapshot.result.has_value());
    EXPECT_TRUE(match.DrainEvents().empty());
}

TEST(OfflineMatch, KeepsActorZeroAsControlledContender)
{
    const MatchSnapshot snapshot = StartSnapshot(20260823U);
    ASSERT_FALSE(snapshot.actors.empty());
    const ActorSnapshot& controlled = snapshot.actors.front();

    EXPECT_EQ(0U, controlled.id);
    EXPECT_EQ(ActorRole::Contender, controlled.role);
    EXPECT_EQ(NeutralArchetype::None, controlled.neutralArchetype);
    EXPECT_EQ(100, controlled.health);
    EXPECT_EQ(WeaponType::Blade, controlled.weapon);
}

TEST(OfflineMatch, RepeatsSpawnAndLootForSameSeed)
{
    EXPECT_EQ(StartSnapshot(20260823U), StartSnapshot(20260823U));
    EXPECT_NE(StartSnapshot(20260823U), StartSnapshot(7U));
}

TEST(OfflineMatch, SortsActorAndLootSnapshotsByNumericId)
{
    const MatchSnapshot snapshot = StartSnapshot(20260823U);

    for (std::size_t index = 0; index < snapshot.actors.size(); ++index)
    {
        EXPECT_EQ(static_cast<ActorId>(index), snapshot.actors[index].id);
    }
    for (std::size_t index = 0; index < snapshot.loot.size(); ++index)
    {
        EXPECT_EQ(static_cast<std::uint32_t>(index), snapshot.loot[index].id);
    }
}

TEST(OfflineMatch, RespectsConfiguredActorSpawnSpacing)
{
    const MatchConfig config = DefaultMatchConfig();
    const MatchSnapshot snapshot = StartSnapshot(config.seed);

    for (std::size_t left = 0; left < snapshot.actors.size(); ++left)
    {
        for (std::size_t right = left + 1; right < snapshot.actors.size(); ++right)
        {
            const float required =
                snapshot.actors[left].role == ActorRole::Contender
                    && snapshot.actors[right].role == ActorRole::Contender
                ? config.contenderSpawnSpacing
                : config.neutralSpawnSpacing;
            EXPECT_GE(
                Distance(snapshot.actors[left].position, snapshot.actors[right].position),
                required);
        }
    }
}

TEST(OfflineMatch, RejectsLifecycleCallsBeforeStartAndDuplicateStart)
{
    OfflineMatch match = OfflineMatch::Create(MakeArenaNavMesh());
    EXPECT_EQ(MatchPhase::Waiting, match.Snapshot().phase);
    EXPECT_TRUE(match.Snapshot().actors.empty());
    EXPECT_TRUE(match.Snapshot().loot.empty());
    EXPECT_TRUE(match.DrainEvents().empty());
    EXPECT_THROW(match.Submit(MatchCommand{}), std::logic_error);
    EXPECT_THROW(match.Step(), std::logic_error);

    match.Start();
    EXPECT_THROW(match.Start(), std::logic_error);
}

TEST(OfflineMatch, OwnsTemporaryNavMeshForSpawnAndAgents)
{
    OfflineMatch match = OfflineMatch::Create(MakeArenaNavMesh());

    match.Start();

    EXPECT_EQ(124U, match.Snapshot().actors.size());
}

TEST(OfflineMatch, RejectsInvalidConfigAtCreation)
{
    MatchConfig config = DefaultMatchConfig();
    config.contenderCount = 1U;

    EXPECT_THROW(
        (void)OfflineMatch::Create(MakeArenaNavMesh(), config),
        std::invalid_argument);
}

TEST(OfflineMatch, RejectsTooSmallNavMeshWithinSpawnAttemptLimit)
{
    MatchConfig config = DefaultMatchConfig();
    config.maximumSpawnAttempts = 8U;
    OfflineMatch match = OfflineMatch::Create(MakeTinyNavMesh(), config);

    EXPECT_THROW(match.Start(), std::runtime_error);
    EXPECT_EQ(MatchPhase::Waiting, match.Snapshot().phase);
    EXPECT_TRUE(match.Snapshot().actors.empty());
    EXPECT_TRUE(match.Snapshot().loot.empty());
}
} // namespace
