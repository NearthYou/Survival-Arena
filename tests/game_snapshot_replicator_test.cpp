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
