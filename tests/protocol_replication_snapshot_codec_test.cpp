#include <dxa/protocol/ReplicationSnapshot.hpp>
#include <dxa/protocol/ReplicationSnapshotCodec.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
using namespace dxa::protocol;

[[nodiscard]] GameSnapshot MinimalFullPrecisionSnapshot()
{
    GameSnapshot snapshot;
    snapshot.phase = NetworkMatchPhase::Running;
    snapshot.safeZoneStage = NetworkSafeZoneStage::Stage2;
    snapshot.safeZoneCenter = {4.0F, -8.0F};
    snapshot.safeZoneRadius = 96.0F;
    snapshot.aliveContenders = 1U;
    snapshot.actors.push_back(NetworkActorSnapshot{
        EntityId{1U},
        NetworkActorRole::Contender,
        NetworkNeutralArchetype::None,
        {1.25F, -2.5F},
        100,
        true,
        NetworkWeaponType::Rifle,
        3U,
        2U});
    snapshot.loot.push_back(NetworkLootSnapshot{
        7U,
        NetworkLootType::MedKit,
        {3.5F, 4.5F},
        true});
    snapshot.eventChecksum = 0x0102030405060708ULL;
    return snapshot;
}

[[nodiscard]] QuantizedActorValue QuantizedActor(
    const std::uint32_t id)
{
    return QuantizedActorValue{
        EntityId{id},
        NetworkActorRole::Contender,
        NetworkNeutralArchetype::None,
        {static_cast<std::uint16_t>(1000U + id),
         static_cast<std::uint16_t>(2000U + id)},
        100U,
        true,
        NetworkWeaponType::Rifle,
        12U,
        3U};
}

[[nodiscard]] QuantizedGlobalValue QuantizedGlobal()
{
    return QuantizedGlobalValue{
        NetworkMatchPhase::Running,
        NetworkSafeZoneStage::Stage2,
        {32768U, 16384U},
        49151U,
        2U,
        {},
        false,
        0x0102030405060708ULL};
}

[[nodiscard]] SnapshotPayload RepresentativeFullStatePayload()
{
    SnapshotPayload payload;
    payload.header = {
        SnapshotPayloadKind::FullState,
        SnapshotValueEncoding::FullPrecision,
        0U,
        10U};
    payload.fullPrecision = MinimalFullPrecisionSnapshot();
    return payload;
}

[[nodiscard]] SnapshotPayload RepresentativeFullPrecisionKeyframePayload()
{
    SnapshotPayload payload = RepresentativeFullStatePayload();
    payload.header.kind = SnapshotPayloadKind::Keyframe;
    payload.header.payloadSnapshotId = 11U;
    payload.fullPrecision.aliveContenders = 24U;
    return payload;
}

[[nodiscard]] SnapshotPayload RepresentativeKeyframePayload()
{
    SnapshotPayload payload;
    payload.header = {
        SnapshotPayloadKind::Keyframe,
        SnapshotValueEncoding::Quantized,
        0U,
        0x01020304U};
    payload.global = QuantizedGlobal();
    payload.actorValues = {QuantizedActor(1U), QuantizedActor(2U)};
    payload.lootValues = {
        QuantizedLootValue{
            10U,
            NetworkLootType::MedKit,
            {3000U, 4000U},
            true}};
    return payload;
}

[[nodiscard]] SnapshotPayload RepresentativeDeltaPayload()
{
    SnapshotPayload payload;
    payload.header = {
        SnapshotPayloadKind::Delta,
        SnapshotValueEncoding::Quantized,
        7U,
        8U};
    payload.actorValues = {QuantizedActor(2U)};
    payload.actorDeltas = {
        QuantizedActorDelta{
            EntityId{1U},
            ActorField::Position | ActorField::WeaponCooldown,
            {1234U, 5678U},
            100U,
            true,
            NetworkWeaponType::ArcPulse,
            9U,
            0U}};
    payload.removedActors = {EntityId{3U}};
    payload.lootValues = {
        QuantizedLootValue{
            11U,
            NetworkLootType::Rifle,
            {2222U, 3333U},
            true}};
    payload.lootDeltas = {
        QuantizedLootDelta{10U, LootField::Active, false}};
    payload.removedLoot = {12U};
    payload.globalDelta = QuantizedGlobalDelta{
        GlobalField::SafeZone | GlobalField::EventChecksum,
        NetworkMatchPhase::Waiting,
        NetworkSafeZoneStage::Stage3,
        {30000U, 31000U},
        40000U,
        0U,
        {},
        false,
        0x1112131415161718ULL};
    return payload;
}

void WriteU32At(
    std::vector<std::byte>& bytes,
    const std::size_t offset,
    const std::uint32_t value)
{
    ASSERT_GE(bytes.size(), offset + sizeof(value));
    for (std::size_t index = 0U; index < sizeof(value); ++index)
    {
        bytes[offset + index] = static_cast<std::byte>(
            (value >> (index * 8U)) & 0xFFU);
    }
}

TEST(ReplicationSnapshot, LocksWireEnumAndFieldBitValues)
{
    EXPECT_EQ(1U, static_cast<unsigned>(SnapshotPayloadKind::FullState));
    EXPECT_EQ(2U, static_cast<unsigned>(SnapshotPayloadKind::Keyframe));
    EXPECT_EQ(3U, static_cast<unsigned>(SnapshotPayloadKind::Delta));
    EXPECT_EQ(
        1U,
        static_cast<unsigned>(SnapshotValueEncoding::FullPrecision));
    EXPECT_EQ(2U, static_cast<unsigned>(SnapshotValueEncoding::Quantized));

    EXPECT_EQ(0x01U, static_cast<unsigned>(ActorField::Position));
    EXPECT_EQ(0x02U, static_cast<unsigned>(ActorField::HealthAlive));
    EXPECT_EQ(0x04U, static_cast<unsigned>(ActorField::WeaponCooldown));
    EXPECT_EQ(0x08U, static_cast<unsigned>(ActorField::Eliminations));
    EXPECT_EQ(0x01U, static_cast<unsigned>(LootField::Active));
    EXPECT_EQ(0x01U, static_cast<unsigned>(GlobalField::Phase));
    EXPECT_EQ(0x02U, static_cast<unsigned>(GlobalField::SafeZone));
    EXPECT_EQ(0x04U, static_cast<unsigned>(GlobalField::AliveContenders));
    EXPECT_EQ(0x08U, static_cast<unsigned>(GlobalField::Result));
    EXPECT_EQ(0x10U, static_cast<unsigned>(GlobalField::EventChecksum));
}

TEST(ReplicationSnapshot, QuantizedCoordinateRoundTripStaysWithinHalfStep)
{
    constexpr float minimum = -128.0F;
    constexpr float maximum = 128.0F;
    constexpr std::array values{
        -128.0F,
        -31.25F,
        0.0F,
        79.5F,
        128.0F};

    for (const float value : values)
    {
        const std::uint16_t encoded =
            QuantizeCoordinate(value, minimum, maximum);
        const float decoded =
            DequantizeCoordinate(encoded, minimum, maximum);
        EXPECT_LE(
            std::abs(decoded - value),
            (maximum - minimum) / 131070.0F);
    }
}

TEST(ReplicationSnapshot, QuantizedCoordinateMapsBothEndpointsExactly)
{
    EXPECT_EQ(0U, QuantizeCoordinate(-128.0F, -128.0F, 128.0F));
    EXPECT_EQ(
        std::numeric_limits<std::uint16_t>::max(),
        QuantizeCoordinate(128.0F, -128.0F, 128.0F));
    EXPECT_FLOAT_EQ(-128.0F, DequantizeCoordinate(0U, -128.0F, 128.0F));
    EXPECT_FLOAT_EQ(
        128.0F,
        DequantizeCoordinate(
            std::numeric_limits<std::uint16_t>::max(),
            -128.0F,
            128.0F));
}

TEST(ReplicationSnapshot, RejectsOutOfArenaAndNonFiniteCoordinates)
{
    EXPECT_THROW(
        (void)QuantizeCoordinate(129.0F, -128.0F, 128.0F),
        std::out_of_range);
    EXPECT_THROW(
        (void)QuantizeCoordinate(
            std::numeric_limits<float>::quiet_NaN(),
            -128.0F,
            128.0F),
        std::invalid_argument);
    EXPECT_THROW(
        (void)QuantizeCoordinate(
            std::numeric_limits<float>::infinity(),
            -128.0F,
            128.0F),
        std::invalid_argument);
}

TEST(ReplicationSnapshot, RejectsInvalidQuantizationBounds)
{
    EXPECT_THROW(
        (void)QuantizeCoordinate(0.0F, 1.0F, 1.0F),
        std::invalid_argument);
    EXPECT_THROW(
        (void)DequantizeCoordinate(0U, 1.0F, -1.0F),
        std::invalid_argument);
    EXPECT_THROW(
        (void)QuantizeSafeZoneRadius(1.0F, 0.0F),
        std::invalid_argument);
}

TEST(ReplicationSnapshot, BoundedGameplayValuesRejectOverflow)
{
    EXPECT_EQ(0U, QuantizeHealth(0));
    EXPECT_EQ(100U, QuantizeHealth(100));
    EXPECT_THROW((void)QuantizeHealth(-1), std::out_of_range);
    EXPECT_THROW((void)QuantizeHealth(101), std::out_of_range);

    EXPECT_EQ(0U, QuantizeCooldownTicks(0U));
    EXPECT_EQ(
        std::numeric_limits<std::uint16_t>::max(),
        QuantizeCooldownTicks(
            std::numeric_limits<std::uint16_t>::max()));
    EXPECT_THROW((void)QuantizeCooldownTicks(65536U), std::out_of_range);

    EXPECT_EQ(255U, QuantizeEliminations(255U));
    EXPECT_THROW((void)QuantizeEliminations(256U), std::out_of_range);

    EXPECT_EQ(24U, QuantizeAliveContenders(24U));
    EXPECT_THROW((void)QuantizeAliveContenders(25U), std::out_of_range);
}

TEST(ReplicationSnapshot, SafeZoneRadiusUsesUnsignedSixteenBitRange)
{
    EXPECT_EQ(0U, QuantizeSafeZoneRadius(0.0F, 128.0F));
    EXPECT_EQ(
        std::numeric_limits<std::uint16_t>::max(),
        QuantizeSafeZoneRadius(128.0F, 128.0F));
    EXPECT_NEAR(
        64.0F,
        DequantizeSafeZoneRadius(
            QuantizeSafeZoneRadius(64.0F, 128.0F),
            128.0F),
        128.0F / 131070.0F);
    EXPECT_THROW(
        (void)QuantizeSafeZoneRadius(128.01F, 128.0F),
        std::out_of_range);
}

TEST(ReplicationSnapshot, FieldMasksRejectUnknownBits)
{
    const ActorField actorFields =
        ActorField::Position | ActorField::WeaponCooldown;
    const GlobalField globalFields =
        GlobalField::SafeZone | GlobalField::EventChecksum;

    EXPECT_TRUE(IsValidActorFieldMask(actorFields));
    EXPECT_TRUE(IsValidGlobalFieldMask(globalFields));
    EXPECT_TRUE(IsValidLootFieldMask(LootField::Active));
    EXPECT_FALSE(IsValidActorFieldMask(static_cast<ActorField>(0x80U)));
    EXPECT_FALSE(IsValidGlobalFieldMask(static_cast<GlobalField>(0x80U)));
    EXPECT_FALSE(IsValidLootFieldMask(static_cast<LootField>(0x02U)));
}

TEST(ReplicationSnapshotCodec, EncodesHeaderInLockedLittleEndianOrder)
{
    const std::vector<std::byte> bytes =
        EncodeSnapshotPayload(RepresentativeKeyframePayload());
    const std::vector<std::byte> expectedHeader{
        std::byte{0x02},
        std::byte{0x02},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0x04},
        std::byte{0x03},
        std::byte{0x02},
        std::byte{0x01}};

    ASSERT_GE(bytes.size(), expectedHeader.size());
    EXPECT_TRUE(std::equal(
        expectedHeader.begin(),
        expectedHeader.end(),
        bytes.begin()));
}

TEST(ReplicationSnapshotCodec, RoundTripsEveryPayloadRepresentation)
{
    const std::array payloads{
        RepresentativeFullStatePayload(),
        RepresentativeFullPrecisionKeyframePayload(),
        RepresentativeKeyframePayload(),
        RepresentativeDeltaPayload()};

    for (const SnapshotPayload& source : payloads)
    {
        const std::vector<std::byte> bytes = EncodeSnapshotPayload(source);
        const SnapshotPayloadDecodeResult decoded =
            DecodeSnapshotPayload(bytes);
        ASSERT_TRUE(decoded.payload.has_value());
        EXPECT_EQ(SnapshotPayloadDecodeError::None, decoded.error);
        EXPECT_EQ(source, *decoded.payload);
    }
}

TEST(ReplicationSnapshotCodec, RoundTripsDeltaEnterUpdateAndRemove)
{
    const SnapshotPayload source = RepresentativeDeltaPayload();
    const std::vector<std::byte> bytes = EncodeSnapshotPayload(source);
    const SnapshotPayloadDecodeResult decoded = DecodeSnapshotPayload(bytes);

    ASSERT_TRUE(decoded.payload.has_value());
    EXPECT_EQ(source.actorValues, decoded.payload->actorValues);
    EXPECT_EQ(source.actorDeltas, decoded.payload->actorDeltas);
    EXPECT_EQ(source.removedActors, decoded.payload->removedActors);
    EXPECT_EQ(source.lootValues, decoded.payload->lootValues);
    EXPECT_EQ(source.lootDeltas, decoded.payload->lootDeltas);
    EXPECT_EQ(source.removedLoot, decoded.payload->removedLoot);
    EXPECT_EQ(source.globalDelta, decoded.payload->globalDelta);
}

TEST(ReplicationSnapshotCodec, RejectsInvalidHeaderAndFieldMasks)
{
    SnapshotPayload missingBase = RepresentativeDeltaPayload();
    missingBase.header.baseSnapshotId = 0U;
    EXPECT_THROW(
        (void)EncodeSnapshotPayload(missingBase),
        std::invalid_argument);

    SnapshotPayload zeroPayloadId = RepresentativeKeyframePayload();
    zeroPayloadId.header.payloadSnapshotId = 0U;
    EXPECT_THROW(
        (void)EncodeSnapshotPayload(zeroPayloadId),
        std::invalid_argument);

    SnapshotPayload zeroFieldMask = RepresentativeDeltaPayload();
    zeroFieldMask.actorDeltas.front().fields = ActorField::None;
    EXPECT_THROW(
        (void)EncodeSnapshotPayload(zeroFieldMask),
        std::invalid_argument);

    SnapshotPayload unknownFieldMask = RepresentativeDeltaPayload();
    unknownFieldMask.globalDelta.fields =
        static_cast<GlobalField>(0x80U);
    EXPECT_THROW(
        (void)EncodeSnapshotPayload(unknownFieldMask),
        std::invalid_argument);

    SnapshotPayload hiddenActorValue = RepresentativeDeltaPayload();
    hiddenActorValue.actorDeltas.front().health = 99U;
    EXPECT_THROW(
        (void)EncodeSnapshotPayload(hiddenActorValue),
        std::invalid_argument);

    SnapshotPayload hiddenGlobalValue = RepresentativeDeltaPayload();
    hiddenGlobalValue.globalDelta.phase = NetworkMatchPhase::Finished;
    EXPECT_THROW(
        (void)EncodeSnapshotPayload(hiddenGlobalValue),
        std::invalid_argument);

    SnapshotPayload inconsistentResult = RepresentativeDeltaPayload();
    inconsistentResult.globalDelta.fields =
        GlobalField::Phase | GlobalField::Result;
    inconsistentResult.globalDelta.phase = NetworkMatchPhase::Finished;
    inconsistentResult.globalDelta.hasResult = false;
    inconsistentResult.globalDelta.eventChecksum = 0U;
    inconsistentResult.globalDelta.safeZoneStage =
        NetworkSafeZoneStage::Stage1;
    inconsistentResult.globalDelta.safeZoneCenter = {};
    inconsistentResult.globalDelta.safeZoneRadius = 0U;
    EXPECT_THROW(
        (void)EncodeSnapshotPayload(inconsistentResult),
        std::invalid_argument);

    std::vector<std::byte> unknownKind =
        EncodeSnapshotPayload(RepresentativeKeyframePayload());
    unknownKind[0] = std::byte{0x7F};
    EXPECT_EQ(
        SnapshotPayloadDecodeError::InvalidValue,
        DecodeSnapshotPayload(unknownKind).error);

    std::vector<std::byte> unknownDeltaMask =
        EncodeSnapshotPayload(RepresentativeDeltaPayload());
    constexpr std::size_t actorDeltaMaskOffset = 50U;
    unknownDeltaMask[actorDeltaMaskOffset] = std::byte{0x80};
    EXPECT_EQ(
        SnapshotPayloadDecodeError::InvalidValue,
        DecodeSnapshotPayload(unknownDeltaMask).error);
}

TEST(ReplicationSnapshotCodec, RejectsNonCanonicalAndDuplicateIds)
{
    SnapshotPayload unsorted = RepresentativeKeyframePayload();
    std::swap(unsorted.actorValues[0], unsorted.actorValues[1]);
    EXPECT_THROW(
        (void)EncodeSnapshotPayload(unsorted),
        std::invalid_argument);

    std::vector<std::byte> duplicate =
        EncodeSnapshotPayload(RepresentativeKeyframePayload());
    constexpr std::size_t firstActorIdOffset = 30U;
    constexpr std::size_t secondActorIdOffset = 46U;
    WriteU32At(duplicate, secondActorIdOffset, 1U);
    EXPECT_EQ(
        SnapshotPayloadDecodeError::DuplicateEntity,
        DecodeSnapshotPayload(duplicate).error);

    std::vector<std::byte> nonCanonical =
        EncodeSnapshotPayload(RepresentativeKeyframePayload());
    WriteU32At(nonCanonical, secondActorIdOffset, 0U);
    EXPECT_EQ(
        SnapshotPayloadDecodeError::NonCanonicalOrder,
        DecodeSnapshotPayload(nonCanonical).error);

    EXPECT_NE(firstActorIdOffset, secondActorIdOffset);
}

TEST(ReplicationSnapshotCodec, RejectsCountLimitTrailingBytesAndOversize)
{
    std::vector<std::byte> excessiveCount =
        EncodeSnapshotPayload(RepresentativeKeyframePayload());
    constexpr std::size_t actorCountOffset = 28U;
    excessiveCount[actorCountOffset] = std::byte{0x7D};
    excessiveCount[actorCountOffset + 1U] = std::byte{0x00};
    EXPECT_EQ(
        SnapshotPayloadDecodeError::CountLimit,
        DecodeSnapshotPayload(excessiveCount).error);

    std::vector<std::byte> trailing =
        EncodeSnapshotPayload(RepresentativeDeltaPayload());
    trailing.push_back(std::byte{0xFF});
    EXPECT_EQ(
        SnapshotPayloadDecodeError::TrailingBytes,
        DecodeSnapshotPayload(trailing).error);

    const std::vector<std::byte> oversized(MaxSnapshotPayloadBytes + 1U);
    EXPECT_EQ(
        SnapshotPayloadDecodeError::CountLimit,
        DecodeSnapshotPayload(oversized).error);
}
} // namespace
