#include <dxa/protocol/ReplicationSnapshot.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace
{
using namespace dxa::protocol;

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
} // namespace
