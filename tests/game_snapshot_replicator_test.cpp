#include <dxa/game_server/SnapshotReplicator.hpp>

#include <dxa/protocol/ReplicationSnapshotCodec.hpp>
#include <dxa/simulation/ArenaMap.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace
{
using namespace dxa::game_server;
using namespace dxa::protocol;
using dxa::simulation::ArenaMapDefinition;
using dxa::simulation::SurvivalArenaMapDefinition;

[[nodiscard]] ArenaMapDefinition Arena()
{
    return SurvivalArenaMapDefinition();
}

[[nodiscard]] ReplicationConfig Config(const ReplicationMode mode)
{
    ReplicationConfig config;
    config.mode = mode;
    return config;
}

[[nodiscard]] NetworkActorSnapshot Actor(
    const std::uint32_t id,
    const NetworkVec2 position,
    const bool contender = true,
    const bool alive = true)
{
    return NetworkActorSnapshot{
        EntityId{id},
        contender
            ? NetworkActorRole::Contender
            : NetworkActorRole::Neutral,
        contender
            ? NetworkNeutralArchetype::None
            : NetworkNeutralArchetype::Melee,
        position,
        alive ? 100 : 0,
        alive,
        NetworkWeaponType::Rifle,
        id % 30U,
        id % 8U};
}

[[nodiscard]] GameSnapshot World()
{
    GameSnapshot world;
    world.phase = NetworkMatchPhase::Running;
    world.safeZoneStage = NetworkSafeZoneStage::Stage2;
    world.safeZoneCenter = {4.0F, -7.0F};
    world.safeZoneRadius = 96.0F;
    world.aliveContenders = 24U;
    world.eventChecksum = 0x0102030405060708ULL;
    world.actors.reserve(MaxSnapshotActors);
    for (std::uint32_t id = 0U;
         id < static_cast<std::uint32_t>(MaxSnapshotActors);
         ++id)
    {
        const bool contender = id < 24U;
        const float x = static_cast<float>(static_cast<int>(id % 16U) - 8)
            * 15.0F;
        const float z = static_cast<float>(static_cast<int>(id / 16U) - 4)
            * 15.0F;
        world.actors.push_back(Actor(id, {x, z}, contender));
    }
    world.actors[0].position = {};

    world.loot.reserve(MaxSnapshotLoot);
    for (std::uint32_t id = 0U;
         id < static_cast<std::uint32_t>(MaxSnapshotLoot);
         ++id)
    {
        world.loot.push_back(NetworkLootSnapshot{
            id,
            NetworkLootType::MedKit,
            {static_cast<float>(static_cast<int>(id % 10U) - 5) * 20.0F,
             static_cast<float>(static_cast<int>(id / 10U) - 3) * 20.0F},
            true});
    }
    return world;
}

[[nodiscard]] GameSnapshot FinishedSparseWorld()
{
    GameSnapshot world;
    world.phase = NetworkMatchPhase::Finished;
    world.safeZoneStage = NetworkSafeZoneStage::SuddenDeath;
    world.safeZoneRadius = 0.0F;
    world.aliveContenders = 1U;
    world.actors = {
        Actor(7U, {}, true, false),
        Actor(8U, {10.0F, 0.0F}, false, true),
        Actor(9U, {120.0F, 120.0F}, true, true)};
    world.loot = {
        NetworkLootSnapshot{1U, NetworkLootType::Rifle, {5.0F, 0.0F}, true},
        NetworkLootSnapshot{2U, NetworkLootType::MedKit, {110.0F, 110.0F}, true}};
    world.result = {
        EntityId{9U},
        NetworkMatchEndReason::LastSurvivor,
        17430U};
    world.hasResult = true;
    world.eventChecksum = 99U;
    return world;
}

[[nodiscard]] bool ContainsActor(
    const SnapshotPayload& payload,
    const EntityId id)
{
    if (payload.header.valueEncoding == SnapshotValueEncoding::FullPrecision)
    {
        const auto found = std::lower_bound(
            payload.fullPrecision.actors.begin(),
            payload.fullPrecision.actors.end(),
            id,
            [](const NetworkActorSnapshot& actor, const EntityId value) {
                return actor.id < value;
            });
        return found != payload.fullPrecision.actors.end()
            && found->id == id;
    }
    const auto found = std::lower_bound(
        payload.actorValues.begin(),
        payload.actorValues.end(),
        id,
        [](const QuantizedActorValue& actor, const EntityId value) {
            return actor.id < value;
        });
    return found != payload.actorValues.end() && found->id == id;
}

[[nodiscard]] SnapshotPayload RoundTrip(const SnapshotPayload& payload)
{
    const SnapshotPayloadDecodeResult decoded = DecodeSnapshotPayload(
        EncodeSnapshotPayload(payload));
    if (!decoded.payload.has_value())
    {
        throw std::runtime_error{"replication payload did not round trip"};
    }
    return *decoded.payload;
}

[[nodiscard]] NetworkMatchResult ResultFrom(const SnapshotPayload& payload)
{
    return payload.header.valueEncoding == SnapshotValueEncoding::FullPrecision
        ? payload.fullPrecision.result
        : payload.global.result;
}

[[nodiscard]] NetworkActorSnapshot& ActorById(
    GameSnapshot& world,
    const EntityId id)
{
    const auto found = std::lower_bound(
        world.actors.begin(),
        world.actors.end(),
        id,
        [](const NetworkActorSnapshot& actor, const EntityId value) {
            return actor.id < value;
        });
    if (found == world.actors.end() || found->id != id)
    {
        throw std::out_of_range{"actor is missing from test world"};
    }
    return *found;
}

[[nodiscard]] GameSnapshot EntryWorld(const float otherDistance)
{
    GameSnapshot world;
    world.phase = NetworkMatchPhase::Running;
    world.safeZoneStage = NetworkSafeZoneStage::Stage1;
    world.safeZoneRadius = 128.0F;
    world.aliveContenders = 2U;
    world.actors = {
        Actor(0U, {}),
        Actor(9U, {otherDistance, 0.0F})};
    return world;
}

[[nodiscard]] bool ContainsActorValue(
    const SnapshotPayload& payload,
    const EntityId id)
{
    return std::any_of(
        payload.actorValues.begin(),
        payload.actorValues.end(),
        [id](const QuantizedActorValue& actor) { return actor.id == id; });
}

[[nodiscard]] const QuantizedActorDelta& OnlyActorDelta(
    const SnapshotPayload& payload)
{
    if (payload.actorDeltas.size() != 1U)
    {
        throw std::runtime_error{"test payload does not have one actor delta"};
    }
    return payload.actorDeltas.front();
}

template <typename Field>
[[nodiscard]] bool HasField(const Field fields, const Field field)
{
    return (static_cast<std::uint8_t>(fields)
            & static_cast<std::uint8_t>(field))
        != 0U;
}

[[nodiscard]] SnapshotPayload ApplyDeltaForTest(
    const SnapshotPayload& base,
    const SnapshotPayload& delta)
{
    SnapshotPayload applied = base;
    const QuantizedGlobalDelta& global = delta.globalDelta;
    if (HasField(global.fields, GlobalField::Phase))
    {
        applied.global.phase = global.phase;
    }
    if (HasField(global.fields, GlobalField::SafeZone))
    {
        applied.global.safeZoneStage = global.safeZoneStage;
        applied.global.safeZoneCenter = global.safeZoneCenter;
        applied.global.safeZoneRadius = global.safeZoneRadius;
    }
    if (HasField(global.fields, GlobalField::AliveContenders))
    {
        applied.global.aliveContenders = global.aliveContenders;
    }
    if (HasField(global.fields, GlobalField::Result))
    {
        applied.global.result = global.result;
        applied.global.hasResult = global.hasResult;
    }
    if (HasField(global.fields, GlobalField::EventChecksum))
    {
        applied.global.eventChecksum = global.eventChecksum;
    }

    std::erase_if(
        applied.actorValues,
        [&delta](const QuantizedActorValue& actor) {
            return std::binary_search(
                delta.removedActors.begin(),
                delta.removedActors.end(),
                actor.id);
        });
    applied.actorValues.insert(
        applied.actorValues.end(),
        delta.actorValues.begin(),
        delta.actorValues.end());
    std::sort(
        applied.actorValues.begin(),
        applied.actorValues.end(),
        [](const QuantizedActorValue& left,
           const QuantizedActorValue& right) {
            return left.id < right.id;
        });
    for (const QuantizedActorDelta& change : delta.actorDeltas)
    {
        const auto found = std::lower_bound(
            applied.actorValues.begin(),
            applied.actorValues.end(),
            change.id,
            [](const QuantizedActorValue& actor, const EntityId id) {
                return actor.id < id;
            });
        if (found == applied.actorValues.end() || found->id != change.id)
        {
            throw std::runtime_error{"actor delta target is missing"};
        }
        if (HasField(change.fields, ActorField::Position))
        {
            found->position = change.position;
        }
        if (HasField(change.fields, ActorField::HealthAlive))
        {
            found->health = change.health;
            found->alive = change.alive;
        }
        if (HasField(change.fields, ActorField::WeaponCooldown))
        {
            found->weapon = change.weapon;
            found->cooldownTicksRemaining = change.cooldownTicksRemaining;
        }
        if (HasField(change.fields, ActorField::Eliminations))
        {
            found->eliminations = change.eliminations;
        }
    }

    std::erase_if(
        applied.lootValues,
        [&delta](const QuantizedLootValue& loot) {
            return std::binary_search(
                delta.removedLoot.begin(),
                delta.removedLoot.end(),
                loot.id);
        });
    applied.lootValues.insert(
        applied.lootValues.end(),
        delta.lootValues.begin(),
        delta.lootValues.end());
    std::sort(
        applied.lootValues.begin(),
        applied.lootValues.end(),
        [](const QuantizedLootValue& left,
           const QuantizedLootValue& right) {
            return left.id < right.id;
        });
    for (const QuantizedLootDelta& change : delta.lootDeltas)
    {
        const auto found = std::lower_bound(
            applied.lootValues.begin(),
            applied.lootValues.end(),
            change.id,
            [](const QuantizedLootValue& loot, const std::uint32_t id) {
                return loot.id < id;
            });
        if (found == applied.lootValues.end() || found->id != change.id)
        {
            throw std::runtime_error{"loot delta target is missing"};
        }
        found->active = change.active;
    }
    return applied;
}

TEST(SnapshotReplicator, FullStatePreservesTheExistingWorld)
{
    SnapshotReplicator replicator{Arena(), Config(ReplicationMode::FullState)};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    const GameSnapshot world = World();

    const ReplicationBuild build = replicator.Build(
        PlayerId{1U},
        1U,
        world);

    EXPECT_EQ(SnapshotPayloadKind::FullState, build.payload.header.kind);
    EXPECT_EQ(SnapshotValueEncoding::FullPrecision,
              build.payload.header.valueEncoding);
    EXPECT_EQ(124U, build.visibleActorCount);
    EXPECT_EQ(60U, build.visibleLootCount);
    EXPECT_FALSE(build.keyframe);
    EXPECT_EQ(
        EncodeSnapshotPayload(build.payload),
        build.encodedPayload);
    EXPECT_EQ(world, RoundTrip(build.payload).fullPrecision);
}

TEST(SnapshotReplicator, InterestKeyframeAlwaysContainsLocalAndGlobalState)
{
    SnapshotReplicator replicator{
        Arena(),
        Config(ReplicationMode::InterestFullPrecision)};
    replicator.RegisterRecipient(PlayerId{7U}, EntityId{7U});
    const GameSnapshot world = FinishedSparseWorld();

    const ReplicationBuild build = replicator.Build(
        PlayerId{7U},
        1U,
        world);
    const SnapshotPayload decoded = RoundTrip(build.payload);

    EXPECT_EQ(SnapshotPayloadKind::Keyframe, decoded.header.kind);
    EXPECT_TRUE(ContainsActor(decoded, EntityId{7U}));
    EXPECT_TRUE(ContainsActor(decoded, EntityId{8U}));
    EXPECT_FALSE(ContainsActor(decoded, EntityId{9U}));
    EXPECT_EQ(world.phase, decoded.fullPrecision.phase);
    EXPECT_EQ(world.safeZoneStage, decoded.fullPrecision.safeZoneStage);
    EXPECT_EQ(world.aliveContenders, decoded.fullPrecision.aliveContenders);
    EXPECT_EQ(world.result, decoded.fullPrecision.result);
    EXPECT_EQ(world.eventChecksum, decoded.fullPrecision.eventChecksum);
    EXPECT_TRUE(build.keyframe);
}

TEST(SnapshotReplicator, QuantizedKeyframeRespectsHalfStepError)
{
    SnapshotReplicator replicator{
        Arena(),
        Config(ReplicationMode::InterestQuantized)};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    const GameSnapshot world = World();

    const SnapshotPayload payload = RoundTrip(
        replicator.Build(PlayerId{1U}, 1U, world).payload);
    ASSERT_EQ(SnapshotValueEncoding::Quantized,
              payload.header.valueEncoding);
    ASSERT_FALSE(payload.actorValues.empty());

    constexpr float halfStep = 256.0F / 131070.0F;
    for (const QuantizedActorValue& encoded : payload.actorValues)
    {
        const auto source = std::lower_bound(
            world.actors.begin(),
            world.actors.end(),
            encoded.id,
            [](const NetworkActorSnapshot& actor, const EntityId id) {
                return actor.id < id;
            });
        ASSERT_NE(world.actors.end(), source);
        EXPECT_LE(
            std::abs(
                DequantizeCoordinate(encoded.position.x, -128.0F, 128.0F)
                - source->position.x),
            halfStep);
        EXPECT_LE(
            std::abs(
                DequantizeCoordinate(encoded.position.z, -128.0F, 128.0F)
                - source->position.z),
            halfStep);
    }
    EXPECT_LE(
        std::abs(
            DequantizeSafeZoneRadius(payload.global.safeZoneRadius, 128.0F)
            - world.safeZoneRadius),
        128.0F / 131070.0F);
}

TEST(SnapshotReplicator, DeltaModeBeginsWithQuantizedKeyframe)
{
    SnapshotReplicator replicator{
        Arena(),
        Config(ReplicationMode::InterestDelta)};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});

    const ReplicationBuild build = replicator.Build(
        PlayerId{1U},
        1U,
        World());

    EXPECT_EQ(SnapshotPayloadKind::Keyframe, build.payload.header.kind);
    EXPECT_EQ(SnapshotValueEncoding::Quantized,
              build.payload.header.valueEncoding);
    EXPECT_TRUE(build.keyframe);
}

TEST(SnapshotReplicator, BuildsDeltaAgainstAcknowledgedRecipientView)
{
    SnapshotReplicator replicator{
        Arena(),
        Config(ReplicationMode::InterestDelta)};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    const GameSnapshot base = World();
    const ReplicationBuild keyframe = replicator.Build(
        PlayerId{1U},
        1U,
        base);
    ASSERT_EQ(SnapshotPayloadKind::Keyframe, keyframe.payload.header.kind);
    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 1U));

    GameSnapshot changed = base;
    ActorById(changed, EntityId{7U}).position = {10.0F, 11.0F};
    const ReplicationBuild delta = replicator.Build(
        PlayerId{1U},
        2U,
        changed);

    EXPECT_EQ(SnapshotPayloadKind::Delta, delta.payload.header.kind);
    EXPECT_EQ(1U, delta.payload.header.baseSnapshotId);
    EXPECT_FALSE(delta.keyframe);
    EXPECT_EQ(
        EncodeSnapshotPayload(delta.payload),
        delta.encodedPayload);
    EXPECT_EQ(EntityId{7U}, OnlyActorDelta(delta.payload).id);
    EXPECT_EQ(ActorField::Position, OnlyActorDelta(delta.payload).fields);
}

TEST(SnapshotReplicator, EnterAndLeaveUseFullRecordAndRemoveId)
{
    SnapshotReplicator replicator{
        Arena(),
        Config(ReplicationMode::InterestDelta)};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    (void)replicator.Build(PlayerId{1U}, 1U, EntryWorld(90.0F));
    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 1U));

    const ReplicationBuild entered = replicator.Build(
        PlayerId{1U},
        2U,
        EntryWorld(79.0F));
    EXPECT_TRUE(ContainsActorValue(entered.payload, EntityId{9U}));
    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 2U));

    const ReplicationBuild left = replicator.Build(
        PlayerId{1U},
        3U,
        EntryWorld(89.0F));
    EXPECT_EQ((std::vector<EntityId>{EntityId{9U}}),
              left.payload.removedActors);
}

TEST(SnapshotReplicator, QuantizedEquivalentMovementCreatesNoActorDelta)
{
    SnapshotReplicator replicator{
        Arena(),
        Config(ReplicationMode::InterestDelta)};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    const GameSnapshot base = World();
    (void)replicator.Build(PlayerId{1U}, 1U, base);
    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 1U));

    GameSnapshot changed = base;
    ActorById(changed, EntityId{7U}).position.x += 0.0001F;
    const ReplicationBuild delta = replicator.Build(
        PlayerId{1U},
        2U,
        changed);

    EXPECT_EQ(SnapshotPayloadKind::Delta, delta.payload.header.kind);
    EXPECT_TRUE(delta.payload.actorDeltas.empty());
}

TEST(SnapshotReplicator, ImmutableActorIdentityChangeIsAnInvariantFailure)
{
    SnapshotReplicator replicator{
        Arena(),
        Config(ReplicationMode::InterestDelta)};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    GameSnapshot changed = World();
    (void)replicator.Build(PlayerId{1U}, 1U, changed);
    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 1U));

    NetworkActorSnapshot& actor = ActorById(changed, EntityId{7U});
    actor.role = NetworkActorRole::Neutral;
    actor.neutralArchetype = NetworkNeutralArchetype::Melee;
    EXPECT_THROW(
        (void)replicator.Build(PlayerId{1U}, 2U, changed),
        std::logic_error);
}

TEST(SnapshotReplicator, PeriodicAndRequestedKeyframesResetDeltaBase)
{
    ReplicationConfig config = Config(ReplicationMode::InterestDelta);
    config.keyframeIntervalSnapshots = 30U;
    SnapshotReplicator replicator{Arena(), config};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    const GameSnapshot world = World();
    (void)replicator.Build(PlayerId{1U}, 1U, world);
    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 1U));

    for (std::uint32_t snapshotId = 2U; snapshotId <= 30U; ++snapshotId)
    {
        EXPECT_EQ(
            SnapshotPayloadKind::Delta,
            replicator.Build(PlayerId{1U}, snapshotId, world)
                .payload.header.kind);
    }
    EXPECT_EQ(
        SnapshotPayloadKind::Keyframe,
        replicator.Build(PlayerId{1U}, 31U, world).payload.header.kind);

    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 31U));
    replicator.RequestKeyframe(PlayerId{1U});
    const ReplicationBuild requested = replicator.Build(
        PlayerId{1U},
        32U,
        world);
    EXPECT_EQ(SnapshotPayloadKind::Keyframe, requested.payload.header.kind);
    EXPECT_EQ(0U, requested.payload.header.baseSnapshotId);
}

TEST(SnapshotReplicator, OlderAckDoesNotMoveAcknowledgedBaselineBackward)
{
    SnapshotReplicator replicator{
        Arena(),
        Config(ReplicationMode::InterestDelta)};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    const GameSnapshot world = World();
    (void)replicator.Build(PlayerId{1U}, 1U, world);
    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 1U));
    (void)replicator.Build(PlayerId{1U}, 2U, world);
    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 2U));
    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 1U));

    const ReplicationBuild delta = replicator.Build(
        PlayerId{1U},
        3U,
        world);
    EXPECT_EQ(SnapshotPayloadKind::Delta, delta.payload.header.kind);
    EXPECT_EQ(2U, delta.payload.header.baseSnapshotId);
}

TEST(SnapshotReplicator, EvictedAcknowledgementForcesFallbackKeyframe)
{
    ReplicationConfig config = Config(ReplicationMode::InterestDelta);
    config.maximumBaselinesPerRecipient = 2U;
    config.keyframeIntervalSnapshots = 1000U;
    SnapshotReplicator replicator{Arena(), config};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    const GameSnapshot world = World();
    (void)replicator.Build(PlayerId{1U}, 1U, world);
    (void)replicator.Build(PlayerId{1U}, 2U, world);
    (void)replicator.Build(PlayerId{1U}, 3U, world);

    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 1U));
    const ReplicationBuild fallback = replicator.Build(
        PlayerId{1U},
        4U,
        world);
    EXPECT_EQ(SnapshotPayloadKind::Keyframe, fallback.payload.header.kind);
    EXPECT_TRUE(fallback.keyframe);
    EXPECT_TRUE(fallback.fallbackKeyframe);
}

TEST(SnapshotReplicator, DeltaReconstructsTheCurrentQuantizedKeyframe)
{
    GameSnapshot base = EntryWorld(90.0F);
    base.actors.push_back(Actor(10U, {70.0F, 0.0F}));
    base.aliveContenders = 3U;
    base.loot = {
        NetworkLootSnapshot{1U, NetworkLootType::Rifle, {5.0F, 0.0F}, true},
        NetworkLootSnapshot{2U, NetworkLootType::MedKit, {90.0F, 0.0F}, true}};

    GameSnapshot current = base;
    NetworkActorSnapshot& local = ActorById(current, EntityId{0U});
    local.position = {1.0F, 1.0F};
    local.health = 75;
    local.weapon = NetworkWeaponType::ArcPulse;
    local.cooldownTicksRemaining = 13U;
    local.eliminations = 4U;
    ActorById(current, EntityId{9U}).position = {79.0F, 0.0F};
    ActorById(current, EntityId{10U}).position = {90.0F, 0.0F};
    current.loot[0].active = false;
    current.loot[1].position = {10.0F, 0.0F};
    current.safeZoneStage = NetworkSafeZoneStage::Stage2;
    current.safeZoneCenter = {2.0F, 3.0F};
    current.safeZoneRadius = 96.0F;
    current.eventChecksum = 1234U;

    SnapshotReplicator deltaReplicator{
        Arena(),
        Config(ReplicationMode::InterestDelta)};
    deltaReplicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    const SnapshotPayload basePayload = deltaReplicator.Build(
        PlayerId{1U},
        1U,
        base).payload;
    ASSERT_TRUE(deltaReplicator.AcceptAcknowledgement(PlayerId{1U}, 1U));
    const SnapshotPayload deltaPayload = deltaReplicator.Build(
        PlayerId{1U},
        2U,
        current).payload;

    SnapshotReplicator keyframeReplicator{
        Arena(),
        Config(ReplicationMode::InterestQuantized)};
    keyframeReplicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    const SnapshotPayload expected = keyframeReplicator.Build(
        PlayerId{1U},
        2U,
        current).payload;
    const SnapshotPayload reconstructed = ApplyDeltaForTest(
        basePayload,
        deltaPayload);

    EXPECT_EQ(expected.global, reconstructed.global);
    EXPECT_EQ(expected.actorValues, reconstructed.actorValues);
    EXPECT_EQ(expected.lootValues, reconstructed.lootValues);
}

TEST(SnapshotReplicator, EveryKeyframeModePreservesTheSameMatchResult)
{
    const GameSnapshot world = FinishedSparseWorld();
    for (const ReplicationMode mode : {
             ReplicationMode::FullState,
             ReplicationMode::InterestFullPrecision,
             ReplicationMode::InterestQuantized})
    {
        SnapshotReplicator replicator{Arena(), Config(mode)};
        replicator.RegisterRecipient(PlayerId{7U}, EntityId{7U});
        const SnapshotPayload payload = RoundTrip(
            replicator.Build(PlayerId{7U}, 1U, world).payload);

        EXPECT_EQ(world.result, ResultFrom(payload));
    }
}

TEST(SnapshotReplicator, BuildsSameWorldForMultipleRecipientsAtOneSnapshotId)
{
    SnapshotReplicator replicator{
        Arena(),
        Config(ReplicationMode::InterestQuantized)};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    replicator.RegisterRecipient(PlayerId{2U}, EntityId{1U});
    const GameSnapshot world = World();

    const ReplicationBuild first = replicator.Build(
        PlayerId{1U},
        1U,
        world);
    const ReplicationBuild second = replicator.Build(
        PlayerId{2U},
        1U,
        world);

    EXPECT_TRUE(ContainsActor(first.payload, EntityId{0U}));
    EXPECT_TRUE(ContainsActor(second.payload, EntityId{1U}));
    EXPECT_EQ(1U, first.payload.header.payloadSnapshotId);
    EXPECT_EQ(1U, second.payload.header.payloadSnapshotId);
}

TEST(SnapshotReplicator, ValidatesAcknowledgementAndRecipientLifecycle)
{
    SnapshotReplicator replicator{
        Arena(),
        Config(ReplicationMode::InterestQuantized)};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    EXPECT_FALSE(replicator.AcceptAcknowledgement(PlayerId{1U}, 1U));

    (void)replicator.Build(PlayerId{1U}, 1U, World());
    EXPECT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 1U));
    EXPECT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 1U));
    EXPECT_FALSE(replicator.AcceptAcknowledgement(PlayerId{1U}, 2U));
    EXPECT_THROW(
        (void)replicator.Build(PlayerId{1U}, 1U, World()),
        std::invalid_argument);

    replicator.RemoveRecipient(PlayerId{1U});
    EXPECT_THROW(
        (void)replicator.Build(PlayerId{1U}, 2U, World()),
        std::out_of_range);
}

TEST(SnapshotReplicator, RejectsMissingLocalActorAndInvalidConfiguration)
{
    SnapshotReplicator replicator{
        Arena(),
        Config(ReplicationMode::InterestQuantized)};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{999U});
    EXPECT_THROW(
        (void)replicator.Build(PlayerId{1U}, 1U, World()),
        std::logic_error);

    ReplicationConfig invalid = Config(ReplicationMode::InterestQuantized);
    invalid.leaveRadius = invalid.enterRadius - 1.0F;
    EXPECT_THROW(
        (void)SnapshotReplicator(Arena(), invalid),
        std::invalid_argument);

    SnapshotReplicator canonical{
        Arena(),
        Config(ReplicationMode::InterestQuantized)};
    canonical.RegisterRecipient(PlayerId{2U}, EntityId{0U});
    GameSnapshot unsorted = World();
    std::swap(unsorted.actors[0], unsorted.actors[1]);
    EXPECT_THROW(
        (void)canonical.Build(PlayerId{2U}, 1U, unsorted),
        std::invalid_argument);
}
} // namespace
