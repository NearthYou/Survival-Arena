#include <dxa/game_client/ClientSnapshotStream.hpp>

#include <dxa/protocol/ReplicationSnapshotCodec.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace
{
using dxa::game_client::ClientSnapshotStream;
using dxa::game_client::SnapshotApplyResult;
using namespace dxa::protocol;

[[nodiscard]] QuantizedActorValue ActorValue(
    const std::uint32_t id,
    const std::uint16_t x,
    const std::uint16_t z)
{
    return QuantizedActorValue{
        EntityId{id},
        NetworkActorRole::Contender,
        NetworkNeutralArchetype::None,
        {x, z},
        100U,
        true,
        NetworkWeaponType::Blade,
        0U,
        0U};
}

[[nodiscard]] SnapshotPayload KeyframePayload(
    const std::uint32_t snapshotId,
    std::vector<QuantizedActorValue> actors)
{
    SnapshotPayload payload;
    payload.header = {
        SnapshotPayloadKind::Keyframe,
        SnapshotValueEncoding::Quantized,
        0U,
        snapshotId};
    payload.global = {
        NetworkMatchPhase::Running,
        NetworkSafeZoneStage::Stage1,
        {32768U, 32768U},
        std::numeric_limits<std::uint16_t>::max(),
        static_cast<std::uint8_t>(actors.size()),
        {},
        false,
        10U};
    payload.actorValues = std::move(actors);
    return payload;
}

[[nodiscard]] SnapshotPayload DeltaPayload(
    const std::uint32_t baseSnapshotId,
    const std::uint32_t snapshotId)
{
    SnapshotPayload payload;
    payload.header = {
        SnapshotPayloadKind::Delta,
        SnapshotValueEncoding::Quantized,
        baseSnapshotId,
        snapshotId};
    return payload;
}

[[nodiscard]] QuantizedLootValue LootValue(
    const std::uint32_t id,
    const bool active = true)
{
    return QuantizedLootValue{
        id,
        NetworkLootType::Rifle,
        {static_cast<std::uint16_t>(1000U + id),
         static_cast<std::uint16_t>(2000U + id)},
        active};
}

[[nodiscard]] const NetworkActorSnapshot& ActorById(
    const GameSnapshot& world,
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
        throw std::out_of_range{"actor is missing from applied world"};
    }
    return *found;
}

[[nodiscard]] bool ContainsActor(
    const GameSnapshot& world,
    const EntityId id)
{
    return std::any_of(
        world.actors.begin(),
        world.actors.end(),
        [id](const NetworkActorSnapshot& actor) { return actor.id == id; });
}

TEST(ClientSnapshotStream, AppliesKeyframeThenDeltaAndAdvancesAck)
{
    ClientSnapshotStream stream{32U};
    SnapshotPayload initial = KeyframePayload(
        1U,
        {ActorValue(1U, 1000U, 2000U),
         ActorValue(9U, 3000U, 4000U)});
    initial.lootValues = {LootValue(1U), LootValue(3U)};
    const SnapshotApplyResult keyframe = stream.Apply(
        1U,
        initial);
    ASSERT_TRUE(keyframe.world.has_value());
    EXPECT_EQ(1U, keyframe.acknowledgedSnapshotId);

    SnapshotPayload delta = DeltaPayload(1U, 2U);
    delta.globalDelta = QuantizedGlobalDelta{
        GlobalField::SafeZone | GlobalField::EventChecksum,
        NetworkMatchPhase::Waiting,
        NetworkSafeZoneStage::Stage2,
        {30000U, 31000U},
        40000U,
        0U,
        {},
        false,
        99U};
    delta.actorDeltas = {
        QuantizedActorDelta{
            EntityId{1U},
            ActorField::Position
                | ActorField::HealthAlive
                | ActorField::WeaponCooldown
                | ActorField::Eliminations,
            {5000U, 6000U},
            75U,
            true,
            NetworkWeaponType::ArcPulse,
            13U,
            4U}};
    delta.removedActors = {EntityId{9U}};
    delta.actorValues = {ActorValue(10U, 7000U, 8000U)};
    delta.removedLoot = {1U};
    delta.lootValues = {LootValue(2U)};
    delta.lootDeltas = {
        QuantizedLootDelta{3U, LootField::Active, false}};

    const SnapshotApplyResult applied = stream.Apply(2U, delta);
    ASSERT_TRUE(applied.world.has_value());
    EXPECT_EQ(2U, applied.acknowledgedSnapshotId);
    EXPECT_FALSE(applied.requestKeyframe);
    EXPECT_EQ((std::vector<EntityId>{EntityId{9U}}),
              applied.removedActors);
    EXPECT_EQ((std::vector<EntityId>{EntityId{10U}}),
              applied.reenteredActors);
    EXPECT_FALSE(ContainsActor(*applied.world, EntityId{9U}));
    EXPECT_TRUE(ContainsActor(*applied.world, EntityId{10U}));

    const NetworkActorSnapshot& actor = ActorById(
        *applied.world,
        EntityId{1U});
    EXPECT_EQ(75, actor.health);
    EXPECT_EQ(NetworkWeaponType::ArcPulse, actor.weapon);
    EXPECT_EQ(13U, actor.cooldownTicksRemaining);
    EXPECT_EQ(4U, actor.eliminations);
    EXPECT_FLOAT_EQ(
        DequantizeCoordinate(5000U, -128.0F, 128.0F),
        actor.position.x);
    EXPECT_EQ(NetworkSafeZoneStage::Stage2,
              applied.world->safeZoneStage);
    EXPECT_EQ(99U, applied.world->eventChecksum);
    ASSERT_EQ(2U, applied.world->loot.size());
    EXPECT_EQ(2U, applied.world->loot[0].id);
    EXPECT_EQ(3U, applied.world->loot[1].id);
    EXPECT_FALSE(applied.world->loot[1].active);
}

TEST(ClientSnapshotStream, MissingBaseDoesNotMutateAndRequestsKeyframe)
{
    ClientSnapshotStream stream{32U};
    const SnapshotApplyResult result = stream.Apply(
        8U,
        DeltaPayload(7U, 8U));

    EXPECT_FALSE(result.world.has_value());
    EXPECT_EQ(0U, result.acknowledgedSnapshotId);
    EXPECT_TRUE(result.requestKeyframe);

    const SnapshotApplyResult recovered = stream.Apply(
        9U,
        KeyframePayload(9U, {ActorValue(1U, 1000U, 2000U)}));
    ASSERT_TRUE(recovered.world.has_value());
    EXPECT_EQ(9U, recovered.acknowledgedSnapshotId);
    EXPECT_FALSE(recovered.requestKeyframe);
}

TEST(ClientSnapshotStream, FullStatePreservesExactWorld)
{
    GameSnapshot world;
    world.phase = NetworkMatchPhase::Running;
    world.safeZoneStage = NetworkSafeZoneStage::Stage2;
    world.safeZoneCenter = {1.25F, -2.5F};
    world.safeZoneRadius = 96.0F;
    world.aliveContenders = 1U;
    world.actors = {
        NetworkActorSnapshot{
            EntityId{1U},
            NetworkActorRole::Contender,
            NetworkNeutralArchetype::None,
            {3.5F, 4.5F},
            100,
            true,
            NetworkWeaponType::Rifle,
            7U,
            2U}};
    world.eventChecksum = 42U;
    SnapshotPayload payload;
    payload.header = {
        SnapshotPayloadKind::FullState,
        SnapshotValueEncoding::FullPrecision,
        0U,
        1U};
    payload.fullPrecision = world;

    ClientSnapshotStream stream{32U};
    const SnapshotApplyResult applied = stream.Apply(1U, payload);

    ASSERT_TRUE(applied.world.has_value());
    EXPECT_EQ(world, *applied.world);
    EXPECT_EQ(1U, applied.acknowledgedSnapshotId);
}

TEST(ClientSnapshotStream, RemoveAndReenterResetInterpolationIdentity)
{
    ClientSnapshotStream stream{32U};
    (void)stream.Apply(
        1U,
        KeyframePayload(1U, {ActorValue(9U, 1000U, 2000U)}));

    SnapshotPayload remove = DeltaPayload(1U, 2U);
    remove.removedActors = {EntityId{9U}};
    const SnapshotApplyResult removed = stream.Apply(2U, remove);
    ASSERT_TRUE(removed.world.has_value());
    EXPECT_FALSE(ContainsActor(*removed.world, EntityId{9U}));

    SnapshotPayload enter = DeltaPayload(2U, 3U);
    enter.actorValues = {ActorValue(9U, 3000U, 4000U)};
    const SnapshotApplyResult reentered = stream.Apply(3U, enter);
    ASSERT_TRUE(reentered.world.has_value());
    EXPECT_TRUE(ContainsActor(*reentered.world, EntityId{9U}));
    EXPECT_EQ((std::vector<EntityId>{EntityId{9U}}),
              reentered.reenteredActors);
}

TEST(ClientSnapshotStream, RejectsStaleAndDuplicateWithoutChangingAck)
{
    ClientSnapshotStream stream{32U};
    (void)stream.Apply(
        2U,
        KeyframePayload(2U, {ActorValue(1U, 1000U, 2000U)}));

    const SnapshotApplyResult duplicate = stream.Apply(
        2U,
        KeyframePayload(2U, {ActorValue(1U, 1000U, 2000U)}));
    const SnapshotApplyResult stale = stream.Apply(
        1U,
        KeyframePayload(1U, {ActorValue(1U, 1000U, 2000U)}));

    EXPECT_FALSE(duplicate.world.has_value());
    EXPECT_FALSE(stale.world.has_value());
    EXPECT_EQ(2U, duplicate.acknowledgedSnapshotId);
    EXPECT_EQ(2U, stale.acknowledgedSnapshotId);
}

TEST(ClientSnapshotStream, EvictedBaseRequestsKeyframeAndKeepsLatestAck)
{
    ClientSnapshotStream stream{2U};
    for (std::uint32_t snapshotId = 1U; snapshotId <= 3U; ++snapshotId)
    {
        const SnapshotApplyResult result = stream.Apply(
            snapshotId,
            KeyframePayload(
                snapshotId,
                {ActorValue(1U, 1000U, 2000U)}));
        ASSERT_TRUE(result.world.has_value());
    }

    const SnapshotApplyResult missing = stream.Apply(
        4U,
        DeltaPayload(1U, 4U));
    EXPECT_FALSE(missing.world.has_value());
    EXPECT_EQ(3U, missing.acknowledgedSnapshotId);
    EXPECT_TRUE(missing.requestKeyframe);
}

TEST(ClientSnapshotStream, ResetAllowsLowerSnapshotIdentity)
{
    ClientSnapshotStream stream{32U};
    (void)stream.Apply(
        10U,
        KeyframePayload(10U, {ActorValue(1U, 1000U, 2000U)}));
    stream.Reset();

    const SnapshotApplyResult result = stream.Apply(
        1U,
        KeyframePayload(1U, {ActorValue(1U, 1000U, 2000U)}));
    ASSERT_TRUE(result.world.has_value());
    EXPECT_EQ(1U, result.acknowledgedSnapshotId);
}

TEST(ClientSnapshotStream, RejectsPayloadIdentityMismatchAndCapacityBounds)
{
    EXPECT_THROW((void)ClientSnapshotStream(0U), std::invalid_argument);
    EXPECT_THROW((void)ClientSnapshotStream(33U), std::invalid_argument);

    ClientSnapshotStream stream{32U};
    EXPECT_THROW(
        (void)stream.Apply(
            2U,
            KeyframePayload(1U, {ActorValue(1U, 1000U, 2000U)})),
        std::invalid_argument);
}
} // namespace
