#include <dxa/simulation/OfflineMatch.hpp>

#include <dxa/simulation/Math2.hpp>
#include <dxa/simulation/NavMesh.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

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
using dxa::simulation::MatchEvent;
using dxa::simulation::MatchEventType;
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
            {-128.0F, -128.0F},
            {128.0F, -128.0F},
            {-128.0F, 128.0F},
            {128.0F, 128.0F}
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

[[nodiscard]] NavMesh MakeLargeNavMesh()
{
    return NavMesh::Build(
        {
            {-256.0F, -256.0F},
            {256.0F, -256.0F},
            {-256.0F, 256.0F},
            {256.0F, 256.0F}
        },
        {
            NavTriangleIndices{{0U, 1U, 2U}},
            NavTriangleIndices{{1U, 3U, 2U}}
        },
        4.0F);
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

[[nodiscard]] MatchConfig SmallMatchConfig()
{
    MatchConfig config = DefaultMatchConfig();
    config.contenderCount = 2U;
    config.meleeNeutralCount = 0U;
    config.rangedNeutralCount = 0U;
    config.rifleLootCount = 0U;
    config.arcPulseLootCount = 0U;
    config.medKitLootCount = 0U;
    config.enableInternalBots = false;
    config.contenderSpawnInnerRadius = 8.0F;
    config.contenderSpawnOuterRadius = 10.0F;
    config.contenderSpawnSpacing = 3.0F;
    return config;
}

[[nodiscard]] OfflineMatch StartedSmallMatch()
{
    OfflineMatch match = OfflineMatch::Create(MakeArenaNavMesh(), SmallMatchConfig());
    match.Start();
    return match;
}

[[nodiscard]] MatchConfig CloseCombatConfig(const std::uint32_t contenderCount = 2U)
{
    MatchConfig config = SmallMatchConfig();
    config.contenderCount = contenderCount;
    config.contenderSpawnInnerRadius = 0.5F;
    config.contenderSpawnOuterRadius = 0.6F;
    config.contenderSpawnSpacing = 0.2F;
    return config;
}

[[nodiscard]] OfflineMatch StartedCloseCombatMatch(
    const std::uint32_t contenderCount = 2U)
{
    OfflineMatch match = OfflineMatch::Create(
        MakeArenaNavMesh(),
        CloseCombatConfig(contenderCount));
    match.Start();
    return match;
}

[[nodiscard]] const ActorSnapshot& ActorById(
    const MatchSnapshot& snapshot,
    const ActorId id)
{
    const auto found = std::find_if(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        [id](const ActorSnapshot& actor) { return actor.id == id; });
    if (found == snapshot.actors.end())
    {
        throw std::logic_error{"expected actor is missing"};
    }
    return *found;
}

[[nodiscard]] MatchCommand MoveCommand(const ActorId actor, const dxa::simulation::Vec2 target)
{
    MatchCommand command;
    command.actor = actor;
    command.moveDestination = target;
    return command;
}

[[nodiscard]] MatchCommand AttackCommand(const ActorId actor, const ActorId target)
{
    MatchCommand command;
    command.actor = actor;
    command.attackTarget = target;
    return command;
}

[[nodiscard]] float Cross(
    const dxa::simulation::Vec2 left,
    const dxa::simulation::Vec2 right) noexcept
{
    return left.x * right.z - left.z * right.x;
}

void StepUntilActorDies(
    OfflineMatch& match,
    const ActorId attacker,
    const ActorId target,
    const std::uint32_t maximumTicks = 200U)
{
    for (std::uint32_t attempt = 0; attempt < maximumTicks; ++attempt)
    {
        const MatchSnapshot snapshot = match.Snapshot();
        if (!ActorById(snapshot, target).alive)
        {
            return;
        }
        if (ActorById(snapshot, attacker).cooldownTicksRemaining == 0U)
        {
            match.Submit(AttackCommand(attacker, target));
        }
        match.Step();
    }
    throw std::runtime_error{"actor did not die within the test tick budget"};
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
    EXPECT_FLOAT_EQ(128.0F, snapshot.safeZoneRadius);
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

TEST(OfflineMatch, UsesLastValidCommandPerActorInTick)
{
    OfflineMatch match = StartedSmallMatch();
    const MatchSnapshot beforeSnapshot = match.Snapshot();
    const dxa::simulation::Vec2 before = ActorById(beforeSnapshot, 0U).position;
    const dxa::simulation::Vec2 firstDestination{before.z, -before.x};
    const dxa::simulation::Vec2 lastDestination{0.0F, 0.0F};
    match.Submit(MoveCommand(0U, firstDestination));
    match.Submit(MoveCommand(0U, lastDestination));

    match.Step();

    const MatchSnapshot afterSnapshot = match.Snapshot();
    const dxa::simulation::Vec2 moved =
        ActorById(afterSnapshot, 0U).position - before;
    const dxa::simulation::Vec2 lastDirection = lastDestination - before;
    const dxa::simulation::Vec2 firstDirection = firstDestination - before;
    EXPECT_EQ(1U, afterSnapshot.tick);
    EXPECT_DOUBLE_EQ(1.0 / 30.0, afterSnapshot.elapsedSeconds);
    EXPECT_NEAR(0.2F, std::sqrt(moved.x * moved.x + moved.z * moved.z), 0.0001F);
    EXPECT_GT(dxa::simulation::Dot(moved, lastDirection), 0.0F);
    EXPECT_NEAR(0.0F, Cross(moved, lastDirection), 0.0001F);
    EXPECT_GT(std::abs(Cross(moved, firstDirection)), 0.01F);
}

TEST(OfflineMatch, LastAttackOnlyCommandDiscardsEarlierSameTickMovement)
{
    OfflineMatch match = StartedSmallMatch();
    const dxa::simulation::Vec2 before = ActorById(match.Snapshot(), 0U).position;
    match.Submit(MoveCommand(0U, {0.0F, 0.0F}));
    match.Submit(AttackCommand(0U, 1U));

    match.Step();

    const dxa::simulation::Vec2 after = ActorById(match.Snapshot(), 0U).position;
    EXPECT_FLOAT_EQ(before.x, after.x);
    EXPECT_FLOAT_EQ(before.z, after.z);
}

TEST(OfflineMatch, KeepsEarlierValidCommandWhenLaterCommandIsInvalid)
{
    OfflineMatch match = StartedSmallMatch();
    const dxa::simulation::Vec2 before = ActorById(match.Snapshot(), 0U).position;
    match.Submit(MoveCommand(0U, {0.0F, 0.0F}));
    match.Submit(MoveCommand(0U, {200.0F, 200.0F}));

    match.Step();

    const dxa::simulation::Vec2 after = ActorById(match.Snapshot(), 0U).position;
    EXPECT_GT(dxa::simulation::Dot(after - before, before * -1.0F), 0.0F);
    const std::vector<MatchEvent> events = match.DrainEvents();
    ASSERT_EQ(1U, events.size());
    EXPECT_EQ(MatchEventType::CommandRejected, events[0].type);
    EXPECT_EQ(0U, events[0].actor);
}

TEST(OfflineMatch, RejectsOffMeshAndNonFiniteMoveWithoutStoppingMatch)
{
    OfflineMatch match = StartedSmallMatch();
    match.Submit(MoveCommand(0U, {200.0F, 200.0F}));
    match.Submit(MoveCommand(
        1U,
        {std::numeric_limits<float>::quiet_NaN(), 0.0F}));

    match.Step();

    EXPECT_EQ(MatchPhase::Running, match.Snapshot().phase);
    const std::vector<MatchEvent> events = match.DrainEvents();
    ASSERT_EQ(2U, events.size());
    EXPECT_EQ(0U, events[0].actor);
    EXPECT_EQ(1U, events[1].actor);
    EXPECT_TRUE(std::all_of(events.begin(), events.end(), [](const MatchEvent& event) {
        return event.type == MatchEventType::CommandRejected;
    }));
}

TEST(OfflineMatch, RejectsMissingActorTargetSelfTargetAndEmptyCommand)
{
    OfflineMatch match = StartedSmallMatch();
    match.Submit(MoveCommand(99U, {0.0F, 0.0F}));
    match.Submit(AttackCommand(0U, 99U));
    match.Submit(AttackCommand(1U, 1U));
    MatchCommand empty;
    empty.actor = 0U;
    match.Submit(empty);

    match.Step();

    const std::vector<MatchEvent> events = match.DrainEvents();
    ASSERT_EQ(4U, events.size());
    EXPECT_TRUE(std::all_of(events.begin(), events.end(), [](const MatchEvent& event) {
        return event.type == MatchEventType::CommandRejected;
    }));
    EXPECT_TRUE(std::is_sorted(
        events.begin(),
        events.end(),
        [](const MatchEvent& left, const MatchEvent& right) {
            return left.actor < right.actor;
        }));
}

TEST(OfflineMatch, MovesOneFixedTickAndRemainsOnNavMesh)
{
    const NavMesh navMesh = MakeArenaNavMesh();
    OfflineMatch match = OfflineMatch::Create(navMesh, SmallMatchConfig());
    match.Start();
    const dxa::simulation::Vec2 before = ActorById(match.Snapshot(), 0U).position;
    match.Submit(MoveCommand(0U, before * -1.0F));

    match.Step();

    const dxa::simulation::Vec2 after = ActorById(match.Snapshot(), 0U).position;
    EXPECT_NEAR(0.2F, Distance(before, after), 0.0001F);
    EXPECT_TRUE(navMesh.FindContainingTriangleGrid(after).triangle.has_value());

    for (std::uint32_t tick = 0; tick < 400U; ++tick)
    {
        match.Step();
        EXPECT_TRUE(navMesh.FindContainingTriangleGrid(
            ActorById(match.Snapshot(), 0U).position).triangle.has_value());
    }
}

TEST(OfflineMatch, PicksLootAutomaticallyAndSortsSameTickEvents)
{
    MatchConfig config = SmallMatchConfig();
    config.rifleLootCount = 1U;
    OfflineMatch match = OfflineMatch::Create(MakeArenaNavMesh(), config);
    match.Start();
    const MatchSnapshot initial = match.Snapshot();
    ASSERT_EQ(1U, initial.loot.size());

    const auto closest = std::min_element(
        initial.actors.begin(),
        initial.actors.end(),
        [&initial](const ActorSnapshot& left, const ActorSnapshot& right) {
            return Distance(left.position, initial.loot[0].position)
                < Distance(right.position, initial.loot[0].position);
        });
    ASSERT_NE(initial.actors.end(), closest);
    match.Submit(MoveCommand(closest->id, initial.loot[0].position));

    bool pickedUp = false;
    for (std::uint32_t tick = 0; tick < 600U && !pickedUp; ++tick)
    {
        match.Submit(MoveCommand(999U, {0.0F, 0.0F}));
        match.Step();
        const std::vector<MatchEvent> events = match.DrainEvents();
        if (!match.Snapshot().loot[0].active)
        {
            pickedUp = true;
            ASSERT_EQ(3U, events.size());
            EXPECT_EQ(MatchEventType::CommandRejected, events[0].type);
            EXPECT_EQ(MatchEventType::LootPickedUp, events[1].type);
            EXPECT_EQ(MatchEventType::WeaponChanged, events[2].type);
            EXPECT_EQ(initial.loot[0].id, events[1].loot);
            EXPECT_EQ(initial.loot[0].id, events[2].loot);
            EXPECT_EQ(WeaponType::Rifle, events[2].weapon);
        }
    }

    EXPECT_TRUE(pickedUp);
    const MatchSnapshot finished = match.Snapshot();
    EXPECT_EQ(1U, static_cast<std::size_t>(std::count_if(
        finished.actors.begin(),
        finished.actors.end(),
        [](const ActorSnapshot& actor) { return actor.weapon == WeaponType::Rifle; })));
}

TEST(OfflineMatch, ClearsCommandQueueAndKeepsCumulativeEventChecksum)
{
    OfflineMatch match = StartedSmallMatch();
    const std::uint64_t initialChecksum = match.Snapshot().eventChecksum;

    match.Step();
    EXPECT_EQ(initialChecksum, match.Snapshot().eventChecksum);
    EXPECT_TRUE(match.DrainEvents().empty());

    match.Submit(MoveCommand(99U, {0.0F, 0.0F}));
    match.Step();
    const std::uint64_t rejectedChecksum = match.Snapshot().eventChecksum;
    EXPECT_NE(initialChecksum, rejectedChecksum);
    ASSERT_EQ(1U, match.DrainEvents().size());
    EXPECT_EQ(rejectedChecksum, match.Snapshot().eventChecksum);

    match.Step();
    EXPECT_TRUE(match.DrainEvents().empty());
    EXPECT_EQ(rejectedChecksum, match.Snapshot().eventChecksum);
}

TEST(OfflineMatch, AppliesAttackBeforeZoneAndStartsWeaponCooldown)
{
    OfflineMatch match = StartedCloseCombatMatch();
    match.Submit(AttackCommand(0U, 1U));

    match.Step();

    const MatchSnapshot snapshot = match.Snapshot();
    EXPECT_EQ(76, ActorById(snapshot, 1U).health);
    EXPECT_EQ(21U, ActorById(snapshot, 0U).cooldownTicksRemaining);
    const std::vector<MatchEvent> events = match.DrainEvents();
    ASSERT_EQ(1U, events.size());
    EXPECT_EQ(MatchEventType::DamageApplied, events[0].type);
    EXPECT_EQ(1U, events[0].actor);
    EXPECT_EQ(0U, events[0].subject);
    EXPECT_EQ(24, events[0].amount);
}

TEST(OfflineMatch, CooldownBlocksConsecutiveAttackCommand)
{
    OfflineMatch match = StartedCloseCombatMatch();
    match.Submit(AttackCommand(0U, 1U));
    match.Step();
    (void)match.DrainEvents();
    match.Submit(AttackCommand(0U, 1U));

    match.Step();

    EXPECT_EQ(76, ActorById(match.Snapshot(), 1U).health);
    EXPECT_EQ(20U, ActorById(match.Snapshot(), 0U).cooldownTicksRemaining);
    EXPECT_TRUE(match.DrainEvents().empty());
}

TEST(OfflineMatch, FinishesImmediatelyWithOneLastSurvivor)
{
    OfflineMatch match = StartedCloseCombatMatch();

    StepUntilActorDies(match, 0U, 1U);

    const MatchSnapshot snapshot = match.Snapshot();
    ASSERT_TRUE(snapshot.result.has_value());
    EXPECT_EQ(MatchPhase::Finished, snapshot.phase);
    EXPECT_EQ(1U, snapshot.aliveContenders);
    EXPECT_EQ(0U, snapshot.result->winner);
    EXPECT_EQ(dxa::simulation::MatchEndReason::LastSurvivor, snapshot.result->reason);
    EXPECT_EQ(snapshot.tick, snapshot.result->finishedTick);
    const std::vector<MatchEvent> events = match.DrainEvents();
    EXPECT_EQ(1U, static_cast<std::size_t>(std::count_if(
        events.begin(), events.end(), [](const MatchEvent& event) {
            return event.type == MatchEventType::ActorDied && event.actor == 1U;
        })));
    EXPECT_EQ(1U, static_cast<std::size_t>(std::count_if(
        events.begin(), events.end(), [](const MatchEvent& event) {
            return event.type == MatchEventType::MatchFinished && event.actor == 0U;
        })));
    EXPECT_THROW(match.Step(), std::logic_error);
}

TEST(OfflineMatch, CreditsLowerIdForEqualLethalContributions)
{
    OfflineMatch match = StartedCloseCombatMatch(3U);
    for (std::uint32_t tick = 0; tick < 200U && ActorById(match.Snapshot(), 2U).alive;
         ++tick)
    {
        const MatchSnapshot snapshot = match.Snapshot();
        if (ActorById(snapshot, 0U).cooldownTicksRemaining == 0U)
        {
            match.Submit(AttackCommand(0U, 2U));
        }
        if (ActorById(snapshot, 1U).cooldownTicksRemaining == 0U)
        {
            match.Submit(AttackCommand(1U, 2U));
        }
        match.Step();
    }

    const std::vector<MatchEvent> events = match.DrainEvents();
    const auto death = std::find_if(
        events.begin(), events.end(), [](const MatchEvent& event) {
            return event.type == MatchEventType::ActorDied && event.actor == 2U;
        });
    ASSERT_NE(events.end(), death);
    ASSERT_TRUE(death->subject.has_value());
    EXPECT_EQ(0U, *death->subject);
    EXPECT_EQ(1U, ActorById(match.Snapshot(), 0U).eliminations);
    EXPECT_EQ(MatchPhase::Running, match.Snapshot().phase);
}

TEST(OfflineMatch, CombatWipePreservesRankedContender)
{
    OfflineMatch match = StartedCloseCombatMatch();
    for (std::uint32_t tick = 0; tick < 200U && match.Snapshot().phase == MatchPhase::Running;
         ++tick)
    {
        const MatchSnapshot snapshot = match.Snapshot();
        if (ActorById(snapshot, 0U).cooldownTicksRemaining == 0U)
        {
            match.Submit(AttackCommand(0U, 1U));
        }
        if (ActorById(snapshot, 1U).cooldownTicksRemaining == 0U)
        {
            match.Submit(AttackCommand(1U, 0U));
        }
        match.Step();
    }

    const MatchSnapshot snapshot = match.Snapshot();
    ASSERT_TRUE(snapshot.result.has_value());
    EXPECT_EQ(0U, snapshot.result->winner);
    EXPECT_TRUE(ActorById(snapshot, 0U).alive);
    EXPECT_EQ(1, ActorById(snapshot, 0U).health);
    EXPECT_FALSE(ActorById(snapshot, 1U).alive);
}

TEST(OfflineMatch, NeutralDeathDoesNotFinishContenderMatch)
{
    MatchConfig config = CloseCombatConfig();
    config.meleeNeutralCount = 1U;
    OfflineMatch match = OfflineMatch::Create(MakeArenaNavMesh(), config);
    match.Start();
    const dxa::simulation::Vec2 neutralPosition = ActorById(match.Snapshot(), 2U).position;
    match.Submit(MoveCommand(0U, neutralPosition));
    for (std::uint32_t tick = 0;
         tick < 600U
         && Distance(ActorById(match.Snapshot(), 0U).position, neutralPosition) > 2.0F;
         ++tick)
    {
        match.Step();
    }

    StepUntilActorDies(match, 0U, 2U);

    EXPECT_FALSE(ActorById(match.Snapshot(), 2U).alive);
    EXPECT_EQ(2U, match.Snapshot().aliveContenders);
    EXPECT_EQ(MatchPhase::Running, match.Snapshot().phase);
    EXPECT_FALSE(match.Snapshot().result.has_value());
}

TEST(OfflineMatch, RejectsCommandFromDeadActor)
{
    OfflineMatch match = StartedCloseCombatMatch(3U);
    StepUntilActorDies(match, 0U, 2U);
    (void)match.DrainEvents();
    const dxa::simulation::Vec2 before = ActorById(match.Snapshot(), 2U).position;
    match.Submit(MoveCommand(2U, {0.0F, 0.0F}));

    match.Step();

    EXPECT_EQ(before, ActorById(match.Snapshot(), 2U).position);
    const std::vector<MatchEvent> events = match.DrainEvents();
    ASSERT_EQ(1U, events.size());
    EXPECT_EQ(MatchEventType::CommandRejected, events[0].type);
    EXPECT_EQ(2U, events[0].actor);
}

TEST(OfflineMatch, ArcPulseEmitsDamageForEveryAffectedTarget)
{
    MatchConfig config = CloseCombatConfig();
    config.arcPulseLootCount = 1U;
    OfflineMatch match = OfflineMatch::Create(MakeArenaNavMesh(), config);
    match.Start();
    const MatchSnapshot initial = match.Snapshot();
    const auto closest = std::min_element(
        initial.actors.begin(), initial.actors.end(), [&initial](
            const ActorSnapshot& left, const ActorSnapshot& right) {
            return Distance(left.position, initial.loot[0].position)
                < Distance(right.position, initial.loot[0].position);
        });
    ASSERT_NE(initial.actors.end(), closest);
    match.Submit(MoveCommand(closest->id, initial.loot[0].position));
    for (std::uint32_t tick = 0;
         tick < 600U && ActorById(match.Snapshot(), closest->id).weapon != WeaponType::ArcPulse;
         ++tick)
    {
        match.Step();
        (void)match.DrainEvents();
    }
    ASSERT_EQ(WeaponType::ArcPulse, ActorById(match.Snapshot(), closest->id).weapon);
    const ActorId target = closest->id == 0U ? 1U : 0U;
    match.Submit(MoveCommand(closest->id, ActorById(match.Snapshot(), target).position));
    for (std::uint32_t tick = 0;
         tick < 600U
         && Distance(
                ActorById(match.Snapshot(), closest->id).position,
                ActorById(match.Snapshot(), target).position) > 10.0F;
         ++tick)
    {
        match.Step();
        (void)match.DrainEvents();
    }
    (void)match.DrainEvents();
    match.Submit(AttackCommand(closest->id, target));

    match.Step();

    const std::vector<MatchEvent> events = match.DrainEvents();
    const auto damage = std::find_if(
        events.begin(), events.end(), [target](const MatchEvent& event) {
            return event.type == MatchEventType::DamageApplied && event.actor == target;
        });
    ASSERT_NE(events.end(), damage);
    EXPECT_EQ(18, damage->amount);
    EXPECT_EQ(closest->id, damage->subject);
}

TEST(OfflineMatch, AppliesZoneDamageOncePerSecond)
{
    MatchConfig config = SmallMatchConfig();
    config.arenaHalfExtent = 256.0F;
    config.contenderSpawnInnerRadius = 160.0F;
    config.contenderSpawnOuterRadius = 164.0F;
    config.contenderSpeed = 0.001F;
    OfflineMatch match = OfflineMatch::Create(MakeLargeNavMesh(), config);
    match.Start();

    for (std::uint32_t tick = 0; tick < 29U; ++tick)
    {
        match.Step();
    }
    EXPECT_EQ(100, ActorById(match.Snapshot(), 0U).health);
    match.Step();

    EXPECT_EQ(98, ActorById(match.Snapshot(), 0U).health);
    EXPECT_EQ(98, ActorById(match.Snapshot(), 1U).health);
    const std::vector<MatchEvent> events = match.DrainEvents();
    EXPECT_EQ(2U, static_cast<std::size_t>(std::count_if(
        events.begin(), events.end(), [](const MatchEvent& event) {
            return event.type == MatchEventType::DamageApplied
                && !event.subject.has_value()
                && event.amount == 2;
        })));
}

TEST(OfflineMatch, ZoneWipePreservesRankedContenderAtOneHealth)
{
    MatchConfig config = SmallMatchConfig();
    config.arenaHalfExtent = 256.0F;
    config.contenderSpawnInnerRadius = 160.0F;
    config.contenderSpawnOuterRadius = 164.0F;
    config.contenderSpeed = 0.001F;
    OfflineMatch match = OfflineMatch::Create(MakeLargeNavMesh(), config);
    match.Start();

    for (std::uint32_t tick = 0;
         tick < 2000U && match.Snapshot().phase == MatchPhase::Running;
         ++tick)
    {
        match.Step();
    }

    const MatchSnapshot snapshot = match.Snapshot();
    ASSERT_TRUE(snapshot.result.has_value());
    EXPECT_EQ(dxa::simulation::MatchEndReason::LastSurvivor, snapshot.result->reason);
    const auto survivor = std::find_if(
        snapshot.actors.begin(), snapshot.actors.end(), [](const ActorSnapshot& actor) {
            return actor.role == ActorRole::Contender && actor.alive;
        });
    ASSERT_NE(snapshot.actors.end(), survivor);
    EXPECT_EQ(snapshot.result->winner, survivor->id);
    EXPECT_EQ(1, survivor->health);
    EXPECT_EQ(1U, snapshot.aliveContenders);
}

TEST(OfflineMatch, EntersSuddenDeathAndForcesTimeoutWinner)
{
    OfflineMatch match = StartedCloseCombatMatch();
    match.Submit(MoveCommand(0U, {0.0F, 0.0F}));
    match.Submit(MoveCommand(1U, {0.0F, 0.0F}));

    while (match.Snapshot().tick < 14400U)
    {
        match.Step();
    }
    EXPECT_EQ(MatchPhase::SuddenDeath, match.Snapshot().phase);
    EXPECT_EQ(2U, match.Snapshot().aliveContenders);

    while (match.Snapshot().tick < 18000U)
    {
        match.Step();
    }

    const MatchSnapshot snapshot = match.Snapshot();
    ASSERT_TRUE(snapshot.result.has_value());
    EXPECT_EQ(MatchPhase::Finished, snapshot.phase);
    EXPECT_EQ(dxa::simulation::MatchEndReason::TimeLimit, snapshot.result->reason);
    EXPECT_EQ(0U, snapshot.result->winner);
    EXPECT_EQ(18000U, snapshot.result->finishedTick);
    EXPECT_EQ(1U, snapshot.aliveContenders);
    EXPECT_EQ(1, ActorById(snapshot, 0U).health);
}

TEST(OfflineMatch, RepeatsCombatEventsAndChecksumForSameInputs)
{
    OfflineMatch first = StartedCloseCombatMatch();
    OfflineMatch repeated = StartedCloseCombatMatch();
    first.Submit(AttackCommand(0U, 1U));
    repeated.Submit(AttackCommand(0U, 1U));

    first.Step();
    repeated.Step();

    EXPECT_EQ(first.Snapshot(), repeated.Snapshot());
    EXPECT_EQ(first.DrainEvents(), repeated.DrainEvents());
}
} // namespace
