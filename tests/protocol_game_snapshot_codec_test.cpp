#include <dxa/protocol/ByteCodec.hpp>
#include <dxa/protocol/GameSnapshot.hpp>
#include <dxa/protocol/GameSnapshotCodec.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
using namespace dxa::protocol;

[[nodiscard]] NetworkActorSnapshot Contender(
    const std::uint32_t id,
    const NetworkVec2 position = {})
{
    return NetworkActorSnapshot{
        EntityId{id},
        NetworkActorRole::Contender,
        NetworkNeutralArchetype::None,
        position,
        100,
        true,
        NetworkWeaponType::Blade,
        0U,
        0U};
}

[[nodiscard]] NetworkActorSnapshot Neutral(
    const std::uint32_t id,
    const NetworkNeutralArchetype archetype,
    const NetworkVec2 position = {})
{
    return NetworkActorSnapshot{
        EntityId{id},
        NetworkActorRole::Neutral,
        archetype,
        position,
        100,
        true,
        NetworkWeaponType::Blade,
        0U,
        0U};
}

[[nodiscard]] GameSnapshot MinimalSnapshot()
{
    GameSnapshot snapshot;
    snapshot.phase = NetworkMatchPhase::Running;
    snapshot.safeZoneStage = NetworkSafeZoneStage::Stage1;
    snapshot.safeZoneCenter = {0.0F, 0.0F};
    snapshot.safeZoneRadius = 128.0F;
    snapshot.aliveContenders = 1U;
    snapshot.actors = {
        Contender(0U, {1.0F, 2.0F}),
        Neutral(1U, NetworkNeutralArchetype::Melee, {3.0F, 4.0F})};
    snapshot.loot = {
        NetworkLootSnapshot{
            0U,
            NetworkLootType::Rifle,
            {5.0F, 6.0F},
            true}};
    snapshot.eventChecksum = 0x0102030405060708ULL;
    return snapshot;
}

[[nodiscard]] GameSnapshot MaximumSnapshot()
{
    GameSnapshot snapshot;
    snapshot.phase = NetworkMatchPhase::Running;
    snapshot.safeZoneStage = NetworkSafeZoneStage::Stage2;
    snapshot.safeZoneCenter = {2.0F, -3.0F};
    snapshot.safeZoneRadius = 96.0F;
    snapshot.aliveContenders = 24U;
    snapshot.actors.reserve(MaxSnapshotActors);
    for (std::uint32_t id = 0U; id < 24U; ++id)
    {
        snapshot.actors.push_back(Contender(
            id,
            {static_cast<float>(id), 0.0F}));
    }
    for (std::uint32_t id = 24U; id < MaxSnapshotActors; ++id)
    {
        snapshot.actors.push_back(Neutral(
            id,
            id % 2U == 0U
                ? NetworkNeutralArchetype::Melee
                : NetworkNeutralArchetype::Ranged,
            {static_cast<float>(id), 1.0F}));
    }
    snapshot.loot.reserve(MaxSnapshotLoot);
    for (std::uint32_t id = 0U; id < MaxSnapshotLoot; ++id)
    {
        snapshot.loot.push_back(NetworkLootSnapshot{
            id,
            NetworkLootType::MedKit,
            {static_cast<float>(id), -1.0F},
            true});
    }
    return snapshot;
}
} // namespace

TEST(GameSnapshotCodec, RoundTripsFullStateAndResult)
{
    GameSnapshot source = MinimalSnapshot();
    source.phase = NetworkMatchPhase::Finished;
    source.hasResult = true;
    source.result = NetworkMatchResult{
        EntityId{0U},
        NetworkMatchEndReason::LastSurvivor,
        90U};

    const std::vector<std::byte> encoded = EncodeGameSnapshot(source);
    const auto decoded = DecodeGameSnapshot(encoded);

    ASSERT_TRUE(decoded.snapshot.has_value());
    EXPECT_EQ(DecodeError::None, decoded.error);
    EXPECT_TRUE(source == *decoded.snapshot);
}

TEST(GameSnapshotCodec, CanonicalizesActorsAndLootByNumericId)
{
    GameSnapshot source = MinimalSnapshot();
    std::swap(source.actors[0], source.actors[1]);
    source.loot.push_back(NetworkLootSnapshot{
        9U,
        NetworkLootType::ArcPulse,
        {7.0F, 8.0F},
        true});
    std::swap(source.loot[0], source.loot[1]);

    const auto decoded = DecodeGameSnapshot(EncodeGameSnapshot(source));

    ASSERT_TRUE(decoded.snapshot.has_value());
    ASSERT_EQ(2U, decoded.snapshot->actors.size());
    ASSERT_EQ(2U, decoded.snapshot->loot.size());
    EXPECT_EQ(EntityId{0U}, decoded.snapshot->actors[0].id);
    EXPECT_EQ(EntityId{1U}, decoded.snapshot->actors[1].id);
    EXPECT_EQ(0U, decoded.snapshot->loot[0].id);
    EXPECT_EQ(9U, decoded.snapshot->loot[1].id);
}

TEST(GameSnapshotCodec, AcceptsLockedMaximumPopulation)
{
    const GameSnapshot source = MaximumSnapshot();
    const std::vector<std::byte> encoded = EncodeGameSnapshot(source);
    const auto decoded = DecodeGameSnapshot(encoded);

    EXPECT_LE(encoded.size(), MaxSnapshotPayloadBytes);
    ASSERT_TRUE(decoded.snapshot.has_value());
    EXPECT_EQ(MaxSnapshotActors, decoded.snapshot->actors.size());
    EXPECT_EQ(MaxSnapshotLoot, decoded.snapshot->loot.size());
    EXPECT_EQ(24U, decoded.snapshot->aliveContenders);
}

TEST(GameSnapshotCodec, RejectsPopulationAboveLockedMaximum)
{
    GameSnapshot actors = MaximumSnapshot();
    actors.actors.push_back(Neutral(
        static_cast<std::uint32_t>(MaxSnapshotActors),
        NetworkNeutralArchetype::Melee));
    EXPECT_THROW(
        (void)EncodeGameSnapshot(actors),
        std::invalid_argument);

    GameSnapshot loot = MaximumSnapshot();
    loot.loot.push_back(NetworkLootSnapshot{
        static_cast<std::uint32_t>(MaxSnapshotLoot),
        NetworkLootType::Rifle,
        {},
        true});
    EXPECT_THROW(
        (void)EncodeGameSnapshot(loot),
        std::invalid_argument);
}

TEST(GameSnapshotCodec, RejectsDuplicateIdsAndAliveCountMismatch)
{
    GameSnapshot duplicateActor = MinimalSnapshot();
    duplicateActor.actors.push_back(Contender(0U));
    ++duplicateActor.aliveContenders;
    EXPECT_THROW(
        (void)EncodeGameSnapshot(duplicateActor),
        std::invalid_argument);

    GameSnapshot duplicateLoot = MinimalSnapshot();
    duplicateLoot.loot.push_back(duplicateLoot.loot.front());
    EXPECT_THROW(
        (void)EncodeGameSnapshot(duplicateLoot),
        std::invalid_argument);

    GameSnapshot wrongAliveCount = MinimalSnapshot();
    wrongAliveCount.aliveContenders = 0U;
    EXPECT_THROW(
        (void)EncodeGameSnapshot(wrongAliveCount),
        std::invalid_argument);
}

TEST(GameSnapshotCodec, RejectsNonFiniteAndInconsistentActorState)
{
    GameSnapshot actorPosition = MinimalSnapshot();
    actorPosition.actors[0].position.x =
        std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(
        (void)EncodeGameSnapshot(actorPosition),
        std::invalid_argument);

    GameSnapshot lootPosition = MinimalSnapshot();
    lootPosition.loot[0].position.z =
        std::numeric_limits<float>::infinity();
    EXPECT_THROW(
        (void)EncodeGameSnapshot(lootPosition),
        std::invalid_argument);

    GameSnapshot zone = MinimalSnapshot();
    zone.safeZoneRadius = -1.0F;
    EXPECT_THROW(
        (void)EncodeGameSnapshot(zone),
        std::invalid_argument);

    GameSnapshot health = MinimalSnapshot();
    health.actors[0].health = 0;
    EXPECT_THROW(
        (void)EncodeGameSnapshot(health),
        std::invalid_argument);

    GameSnapshot archetype = MinimalSnapshot();
    archetype.actors[0].neutralArchetype =
        NetworkNeutralArchetype::Melee;
    EXPECT_THROW(
        (void)EncodeGameSnapshot(archetype),
        std::invalid_argument);
}

TEST(GameSnapshotCodec, EnforcesResultPresenceAndWinnerMembership)
{
    GameSnapshot missingResult = MinimalSnapshot();
    missingResult.phase = NetworkMatchPhase::Finished;
    EXPECT_THROW(
        (void)EncodeGameSnapshot(missingResult),
        std::invalid_argument);

    GameSnapshot earlyResult = MinimalSnapshot();
    earlyResult.hasResult = true;
    earlyResult.result = {
        EntityId{0U},
        NetworkMatchEndReason::LastSurvivor,
        1U};
    EXPECT_THROW(
        (void)EncodeGameSnapshot(earlyResult),
        std::invalid_argument);

    GameSnapshot missingWinner = MinimalSnapshot();
    missingWinner.phase = NetworkMatchPhase::Finished;
    missingWinner.hasResult = true;
    missingWinner.result = {
        EntityId{99U},
        NetworkMatchEndReason::LastSurvivor,
        1U};
    EXPECT_THROW(
        (void)EncodeGameSnapshot(missingWinner),
        std::invalid_argument);
}

TEST(GameSnapshotCodec, RejectsMalformedCountsBoolEnumAndTrailingBytes)
{
    const std::vector<std::byte> valid = EncodeGameSnapshot(MinimalSnapshot());

    std::vector<std::byte> invalidPhase = valid;
    invalidPhase[0] = std::byte{0x63};
    EXPECT_EQ(DecodeError::InvalidValue,
              DecodeGameSnapshot(invalidPhase).error);

    std::vector<std::byte> tooManyActors = valid;
    tooManyActors[18] = std::byte{0x7D};
    tooManyActors[19] = std::byte{0x00};
    EXPECT_EQ(DecodeError::CountLimit,
              DecodeGameSnapshot(tooManyActors).error);

    std::vector<std::byte> invalidAlive = valid;
    invalidAlive[38] = std::byte{0x02};
    EXPECT_EQ(DecodeError::InvalidValue,
              DecodeGameSnapshot(invalidAlive).error);

    std::vector<std::byte> trailing = valid;
    trailing.push_back(std::byte{0x00});
    EXPECT_EQ(DecodeError::TrailingBytes,
              DecodeGameSnapshot(trailing).error);
}
