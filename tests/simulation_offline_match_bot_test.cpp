#include <dxa/simulation/OfflineBotController.hpp>

#include <dxa/simulation/MatchConfig.hpp>
#include <dxa/simulation/NavMesh.hpp>
#include <dxa/simulation/OfflineMatch.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
using dxa::simulation::ActorId;
using dxa::simulation::ActorRole;
using dxa::simulation::ActorSnapshot;
using dxa::simulation::AiArchetype;
using dxa::simulation::BehaviorTreeAiController;
using dxa::simulation::BotDecision;
using dxa::simulation::BotDecisionReason;
using dxa::simulation::BotPerception;
using dxa::simulation::DecideContender;
using dxa::simulation::DecideNeutral;
using dxa::simulation::DefaultMatchConfig;
using dxa::simulation::EvaluateSafeZone;
using dxa::simulation::LootSnapshot;
using dxa::simulation::LootType;
using dxa::simulation::MatchConfig;
using dxa::simulation::MatchEndReason;
using dxa::simulation::MatchEventType;
using dxa::simulation::MatchPhase;
using dxa::simulation::MatchSnapshot;
using dxa::simulation::NavMesh;
using dxa::simulation::NavTriangleIndices;
using dxa::simulation::NeutralArchetype;
using dxa::simulation::OfflineMatch;
using dxa::simulation::SafeZoneState;
using dxa::simulation::Vec2;
using dxa::simulation::WeaponType;

[[nodiscard]] ActorSnapshot Contender(
    const ActorId id,
    const Vec2 position,
    const int health = 100,
    const WeaponType weapon = WeaponType::Blade)
{
    ActorSnapshot actor;
    actor.id = id;
    actor.role = ActorRole::Contender;
    actor.position = position;
    actor.health = health;
    actor.weapon = weapon;
    return actor;
}

[[nodiscard]] ActorSnapshot Neutral(
    const ActorId id,
    const Vec2 position,
    const NeutralArchetype archetype,
    const WeaponType weapon)
{
    ActorSnapshot actor;
    actor.id = id;
    actor.role = ActorRole::Neutral;
    actor.neutralArchetype = archetype;
    actor.position = position;
    actor.health = archetype == NeutralArchetype::Melee ? 60 : 45;
    actor.weapon = weapon;
    return actor;
}

[[nodiscard]] LootSnapshot Loot(
    const std::uint32_t id,
    const LootType type,
    const Vec2 position)
{
    LootSnapshot loot;
    loot.id = id;
    loot.type = type;
    loot.position = position;
    return loot;
}

[[nodiscard]] SafeZoneState Zone(const float radius = 128.0F)
{
    SafeZoneState zone;
    zone.radius = radius;
    return zone;
}

TEST(OfflineMatchBot, SafeZoneReturnOverridesLootAndCombat)
{
    const ActorSnapshot self = Contender(1U, {10.0F, 0.0F}, 40);
    const std::array actors{self, Contender(2U, {11.0F, 0.0F})};
    const std::array loot{Loot(1U, LootType::MedKit, {9.0F, 0.0F})};

    const BotDecision decision = DecideContender(
        self,
        BotPerception{actors, loot, Zone(5.0F)},
        DefaultMatchConfig());

    EXPECT_EQ(BotDecisionReason::ReturnToZone, decision.reason);
    EXPECT_EQ((Vec2{0.0F, 0.0F}), decision.command.moveDestination);
    EXPECT_FALSE(decision.command.attackTarget.has_value());
}

TEST(OfflineMatchBot, LowHealthSeeksNearestMedKitBeforeEnemy)
{
    const ActorSnapshot self = Contender(1U, {0.0F, 0.0F}, 45);
    const std::array actors{self, Contender(2U, {1.0F, 0.0F})};
    const std::array loot{
        Loot(8U, LootType::MedKit, {4.0F, 0.0F}),
        Loot(3U, LootType::MedKit, {2.0F, 0.0F})};

    const BotDecision decision = DecideContender(
        self,
        BotPerception{actors, loot, Zone()},
        DefaultMatchConfig());

    EXPECT_EQ(BotDecisionReason::UseMedKit, decision.reason);
    EXPECT_EQ((Vec2{2.0F, 0.0F}), decision.command.moveDestination);
}

TEST(OfflineMatchBot, BladeSeeksWeaponBeforeAttacking)
{
    const ActorSnapshot self = Contender(1U, {0.0F, 0.0F});
    const std::array actors{self, Contender(2U, {1.0F, 0.0F})};
    const std::array loot{
        Loot(2U, LootType::Rifle, {3.0F, 0.0F})};

    const BotDecision decision = DecideContender(
        self,
        BotPerception{actors, loot, Zone()},
        DefaultMatchConfig());

    EXPECT_EQ(BotDecisionReason::SeekWeapon, decision.reason);
    EXPECT_EQ((Vec2{3.0F, 0.0F}), decision.command.moveDestination);
    EXPECT_FALSE(decision.command.attackTarget.has_value());
}

TEST(OfflineMatchBot, ChoosesLowerIdOnEqualTargetDistance)
{
    const ActorSnapshot self = Contender(10U, {0.0F, 0.0F});
    const std::array actors{
        self,
        Contender(7U, {-1.0F, 0.0F}),
        Contender(3U, {1.0F, 0.0F})};

    const BotDecision decision = DecideContender(
        self,
        BotPerception{actors, std::span<const LootSnapshot>{}, Zone()},
        DefaultMatchConfig());

    EXPECT_EQ(BotDecisionReason::Attack, decision.reason);
    EXPECT_EQ(3U, decision.command.attackTarget);
}

TEST(OfflineMatchBot, ChasesClosestEnemyOutsideWeaponRange)
{
    const ActorSnapshot self = Contender(1U, {0.0F, 0.0F});
    const std::array actors{self, Contender(2U, {5.0F, 0.0F})};

    const BotDecision decision = DecideContender(
        self,
        BotPerception{actors, std::span<const LootSnapshot>{}, Zone()},
        DefaultMatchConfig());

    EXPECT_EQ(BotDecisionReason::Chase, decision.reason);
    EXPECT_EQ((Vec2{5.0F, 0.0F}), decision.command.moveDestination);
}

TEST(OfflineMatchBot, RifleAttacksAtExtendedRange)
{
    const ActorSnapshot self = Contender(1U, {0.0F, 0.0F}, 100, WeaponType::Rifle);
    const std::array actors{self, Contender(2U, {10.0F, 0.0F})};

    const BotDecision decision = DecideContender(
        self,
        BotPerception{actors, std::span<const LootSnapshot>{}, Zone()},
        DefaultMatchConfig());

    EXPECT_EQ(BotDecisionReason::Attack, decision.reason);
    EXPECT_EQ(2U, decision.command.attackTarget);
}

TEST(OfflineMatchBot, NeutralIgnoresCloserNeutralTarget)
{
    const ActorSnapshot self = Neutral(
        10U,
        {0.0F, 0.0F},
        NeutralArchetype::Melee,
        WeaponType::Blade);
    const std::array actors{
        self,
        Neutral(11U, {0.5F, 0.0F}, NeutralArchetype::Melee, WeaponType::Blade),
        Contender(2U, {2.0F, 0.0F})};
    const BehaviorTreeAiController controller{AiArchetype::Melee};

    const BotDecision decision = DecideNeutral(
        self,
        BotPerception{actors, std::span<const LootSnapshot>{}, Zone()},
        DefaultMatchConfig(),
        controller);

    EXPECT_EQ(BotDecisionReason::Attack, decision.reason);
    EXPECT_EQ(2U, decision.command.attackTarget);
}

TEST(OfflineMatchBot, RangedNeutralRetreatsFromCloseContender)
{
    const ActorSnapshot self = Neutral(
        10U,
        {0.0F, 0.0F},
        NeutralArchetype::Ranged,
        WeaponType::Rifle);
    const std::array actors{self, Contender(2U, {1.0F, 0.0F})};
    const BehaviorTreeAiController controller{AiArchetype::Ranged};

    const BotDecision decision = DecideNeutral(
        self,
        BotPerception{actors, std::span<const LootSnapshot>{}, Zone()},
        DefaultMatchConfig(),
        controller);

    EXPECT_EQ(BotDecisionReason::Retreat, decision.reason);
    ASSERT_TRUE(decision.command.moveDestination.has_value());
    EXPECT_LT(decision.command.moveDestination->x, self.position.x);
    EXPECT_FALSE(decision.command.attackTarget.has_value());
}

TEST(OfflineMatchBot, NoVisibleTargetProducesIdleCommand)
{
    const ActorSnapshot self = Contender(1U, {0.0F, 0.0F}, 100, WeaponType::Rifle);
    const std::array actors{self};

    const BotDecision decision = DecideContender(
        self,
        BotPerception{actors, std::span<const LootSnapshot>{}, Zone()},
        DefaultMatchConfig());

    EXPECT_EQ(BotDecisionReason::Idle, decision.reason);
    EXPECT_FALSE(decision.command.moveDestination.has_value());
    EXPECT_FALSE(decision.command.attackTarget.has_value());
}

TEST(OfflineMatchBot, RejectsNonFiniteActorState)
{
    ActorSnapshot self = Contender(1U, {0.0F, 0.0F});
    self.position.x = std::numeric_limits<float>::quiet_NaN();
    const std::array actors{self};

    EXPECT_THROW(
        (void)DecideContender(
            self,
            BotPerception{actors, std::span<const LootSnapshot>{}, Zone()},
            DefaultMatchConfig()),
        std::invalid_argument);
}

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

[[nodiscard]] const ActorSnapshot& FindActor(
    const MatchSnapshot& snapshot,
    const ActorId id)
{
    const auto actor = std::find_if(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        [id](const ActorSnapshot& candidate) { return candidate.id == id; });
    if (actor == snapshot.actors.end())
    {
        throw std::logic_error{"canonical actor is missing"};
    }
    return *actor;
}

struct MatchSummary
{
    std::optional<ActorId> winner;
    MatchEndReason reason = MatchEndReason::LastSurvivor;
    std::uint32_t finishedTick = 0;
    std::uint32_t aliveContenders = 0;
    std::uint64_t eventChecksum = 0;
    bool allValuesFinite = false;
    std::uint32_t damageEvents = 0;
    std::uint32_t deathEvents = 0;
    std::uint32_t combatDeaths = 0;
    std::uint32_t zoneDeaths = 0;
    std::uint32_t rejectedCommands = 0;
    std::uint32_t firstDeathTick = 0;
    std::uint32_t contenderDeaths = 0;
    std::uint32_t neutralDeaths = 0;
    std::uint32_t damageFromContenders = 0;
    std::uint32_t damageFromNeutrals = 0;

    [[nodiscard]] bool operator==(const MatchSummary&) const = default;
};

[[nodiscard]] MatchSummary RunCanonicalMatch(const std::uint32_t seed)
{
    MatchConfig config = DefaultMatchConfig();
    config.seed = seed;
    OfflineMatch match = OfflineMatch::Create(MakeArenaNavMesh(), config);
    match.Start();
    std::uint32_t damageEvents = 0;
    std::uint32_t deathEvents = 0;
    std::uint32_t combatDeaths = 0;
    std::uint32_t zoneDeaths = 0;
    std::uint32_t rejectedCommands = 0;
    std::uint32_t firstDeathTick = 0;
    std::uint32_t contenderDeaths = 0;
    std::uint32_t neutralDeaths = 0;
    std::uint32_t damageFromContenders = 0;
    std::uint32_t damageFromNeutrals = 0;

    while (match.Snapshot().phase != MatchPhase::Finished)
    {
        const MatchSnapshot snapshot = match.Snapshot();
        if (snapshot.tick % config.botDecisionIntervalTicks == 0U)
        {
            const ActorSnapshot& controlled = FindActor(snapshot, 0U);
            const SafeZoneState zone{
                snapshot.safeZoneStage,
                snapshot.safeZoneCenter,
                snapshot.safeZoneRadius,
                EvaluateSafeZone(snapshot.tick, config.tickRate).damagePerSecond};
            const BotDecision decision = DecideContender(
                controlled,
                BotPerception{snapshot.actors, snapshot.loot, zone},
                config);
            if (decision.command.moveDestination.has_value()
                || decision.command.attackTarget.has_value())
            {
                match.Submit(decision.command);
            }
        }
        match.Step();
        const MatchSnapshot afterStep = match.Snapshot();
        const auto events = match.DrainEvents();
        for (const auto& event : events)
        {
            if (event.type == MatchEventType::DamageApplied)
            {
                ++damageEvents;
                if (event.subject.has_value())
                {
                    const ActorRole sourceRole = FindActor(afterStep, *event.subject).role;
                    if (sourceRole == ActorRole::Contender)
                    {
                        ++damageFromContenders;
                    }
                    else
                    {
                        ++damageFromNeutrals;
                    }
                }
            }
            else if (event.type == MatchEventType::ActorDied)
            {
                ++deathEvents;
                if (firstDeathTick == 0U)
                {
                    firstDeathTick = event.tick;
                }
                if (event.subject.has_value())
                {
                    ++combatDeaths;
                }
                else
                {
                    ++zoneDeaths;
                }
                if (FindActor(afterStep, event.actor).role == ActorRole::Contender)
                {
                    ++contenderDeaths;
                }
                else
                {
                    ++neutralDeaths;
                }
            }
            else if (event.type == MatchEventType::CommandRejected)
            {
                ++rejectedCommands;
            }
        }
    }

    const MatchSnapshot result = match.Snapshot();
    bool finite = std::isfinite(result.elapsedSeconds)
        && std::isfinite(result.safeZoneRadius);
    for (const ActorSnapshot& actor : result.actors)
    {
        finite = finite
            && std::isfinite(actor.position.x)
            && std::isfinite(actor.position.z)
            && actor.health >= 0
            && actor.health <= 100;
    }
    return MatchSummary{
        result.result.has_value()
            ? std::optional<ActorId>{result.result->winner}
            : std::nullopt,
        result.result.has_value()
            ? result.result->reason
            : MatchEndReason::LastSurvivor,
        result.tick,
        result.aliveContenders,
        result.eventChecksum,
        finite,
        damageEvents,
        deathEvents,
        combatDeaths,
        zoneDeaths,
        rejectedCommands,
        firstDeathTick,
        contenderDeaths,
        neutralDeaths,
        damageFromContenders,
        damageFromNeutrals};
}

TEST(OfflineMatch, InternalBotsNeverOwnControlledActorZero)
{
    MatchConfig config = DefaultMatchConfig();
    config.contenderCount = 3U;
    config.meleeNeutralCount = 0U;
    config.rangedNeutralCount = 0U;
    config.rifleLootCount = 0U;
    config.arcPulseLootCount = 0U;
    config.medKitLootCount = 0U;
    config.contenderSpawnInnerRadius = 8.0F;
    config.contenderSpawnOuterRadius = 10.0F;
    config.contenderSpawnSpacing = 3.0F;
    OfflineMatch match = OfflineMatch::Create(MakeArenaNavMesh(), config);
    match.Start();
    const MatchSnapshot before = match.Snapshot();

    match.Step();

    const MatchSnapshot after = match.Snapshot();
    EXPECT_EQ(FindActor(before, 0U).position, FindActor(after, 0U).position);
    EXPECT_TRUE(
        FindActor(before, 1U).position != FindActor(after, 1U).position
        || FindActor(before, 2U).position != FindActor(after, 2U).position);
}

TEST(OfflineMatch, CanonicalPopulationFinishesBetweenEightAndTenMinutes)
{
    const MatchSummary first = RunCanonicalMatch(20260823U);
    const MatchSummary repeated = RunCanonicalMatch(20260823U);

    SCOPED_TRACE(::testing::Message()
        << "tick=" << first.finishedTick
        << " damage=" << first.damageEvents
        << " deaths=" << first.deathEvents
        << " combat_deaths=" << first.combatDeaths
        << " zone_deaths=" << first.zoneDeaths
        << " rejected=" << first.rejectedCommands
        << " first_death_tick=" << first.firstDeathTick
        << " contender_deaths=" << first.contenderDeaths
        << " neutral_deaths=" << first.neutralDeaths
        << " damage_from_contenders=" << first.damageFromContenders
        << " damage_from_neutrals=" << first.damageFromNeutrals);
    RecordProperty("finished_tick", static_cast<int>(first.finishedTick));
    if (first.winner.has_value())
    {
        RecordProperty("winner", static_cast<int>(*first.winner));
    }
    RecordProperty("damage_events", static_cast<int>(first.damageEvents));
    RecordProperty("death_events", static_cast<int>(first.deathEvents));

    EXPECT_EQ(first, repeated);
    EXPECT_GE(first.finishedTick, 14400U);
    EXPECT_LE(first.finishedTick, 18000U);
    EXPECT_EQ(1U, first.aliveContenders);
    EXPECT_TRUE(first.winner.has_value());
    EXPECT_NE(0U, first.eventChecksum);
    EXPECT_TRUE(first.allValuesFinite);
}
} // namespace
