#include <dxa/protocol/Crc32.hpp>
#include <dxa/protocol/GameUdpCodec.hpp>
#include <dxa/protocol/GameUdpMessages.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
using namespace dxa::protocol;

[[nodiscard]] UdpSessionToken Token(const std::uint8_t seed)
{
    UdpSessionToken token;
    for (std::size_t index = 0; index < token.size(); ++index)
    {
        token[index] = static_cast<std::byte>(
            static_cast<std::uint8_t>(seed + index));
    }
    return token;
}

[[nodiscard]] std::vector<std::byte> Payload(const std::size_t size)
{
    std::vector<std::byte> payload(size);
    for (std::size_t index = 0; index < payload.size(); ++index)
    {
        payload[index] = static_cast<std::byte>(
            static_cast<std::uint8_t>(index & 0xFFU));
    }
    return payload;
}

void ExpectClientRoundTrip(const ClientDatagram& source)
{
    const EncodedDatagram encoded = EncodeClientDatagram(source);
    const auto decoded = DecodeClientDatagram(encoded.bytes);
    ASSERT_TRUE(decoded.datagram.has_value());
    EXPECT_EQ(DecodeError::None, decoded.error);
    EXPECT_TRUE(source == *decoded.datagram);
}

void ExpectServerRoundTrip(const ServerDatagram& source)
{
    const EncodedDatagram encoded = EncodeServerDatagram(source);
    const auto decoded = DecodeServerDatagram(encoded.bytes);
    ASSERT_TRUE(decoded.datagram.has_value());
    EXPECT_EQ(DecodeError::None, decoded.error);
    EXPECT_TRUE(source == *decoded.datagram);
}

[[nodiscard]] SnapshotFragment SingleFragment(std::vector<std::byte> bytes)
{
    const std::uint32_t size = static_cast<std::uint32_t>(bytes.size());
    const std::uint32_t checksum = Crc32(bytes);
    return SnapshotFragment{
        MatchId{1U},
        2U,
        4U,
        6U,
        0U,
        1U,
        size,
        checksum,
        std::move(bytes)};
}
} // namespace

TEST(Crc32, MatchesTheStandardCheckValue)
{
    constexpr std::array input{
        std::byte{0x31}, std::byte{0x32}, std::byte{0x33},
        std::byte{0x34}, std::byte{0x35}, std::byte{0x36},
        std::byte{0x37}, std::byte{0x38}, std::byte{0x39}};

    EXPECT_EQ(0xCBF43926U, Crc32(input));
    EXPECT_EQ(0U, Crc32({}));
}

TEST(GameUdpCodec, RoundTripsEveryDatagramDirection)
{
    ExpectClientRoundTrip(ClientDatagram{UdpBind{
        MatchId{1U}, PlayerId{2U}, Token(0x10U)}});
    ExpectClientRoundTrip(ClientDatagram{ClientInput{
        MatchId{1U},
        PlayerId{2U},
        Token(0x10U),
        3U,
        0U,
        false,
        NetworkVec2{1.0F, -2.0F},
        true,
        EntityId{4U},
        true}});
    ExpectServerRoundTrip(ServerDatagram{UdpBindAccepted{
        MatchId{1U}, PlayerId{2U}, 30U}});
    ExpectServerRoundTrip(ServerDatagram{SingleFragment(Payload(8U))});
}

TEST(GameUdpCodec, EncodesTenByteHeaderAndInputFlags)
{
    ClientInput input;
    input.match = MatchId{1U};
    input.player = PlayerId{2U};
    input.token = Token(0x10U);
    input.inputSequence = 3U;
    input.acknowledgedSnapshotId = 0xA1B2C3D4U;
    input.requestKeyframe = true;
    input.moveDestination = NetworkVec2{1.0F, -2.0F};
    input.hasMoveDestination = true;
    input.attackTarget = EntityId{4U};
    input.hasAttackTarget = true;

    const EncodedDatagram encoded = EncodeClientDatagram(
        ClientDatagram{input});

    ASSERT_EQ(59U, encoded.bytes.size());
    EXPECT_EQ(UdpDatagramType::ClientInput, encoded.type);
    EXPECT_EQ(std::byte{0x44}, encoded.bytes[0]);
    EXPECT_EQ(std::byte{0x58}, encoded.bytes[1]);
    EXPECT_EQ(std::byte{0x55}, encoded.bytes[2]);
    EXPECT_EQ(std::byte{0x31}, encoded.bytes[3]);
    EXPECT_EQ(std::byte{0x02}, encoded.bytes[4]);
    EXPECT_EQ(std::byte{0x00}, encoded.bytes[5]);
    EXPECT_EQ(std::byte{0x03}, encoded.bytes[6]);
    EXPECT_EQ(std::byte{0x00}, encoded.bytes[7]);
    EXPECT_EQ(std::byte{0x31}, encoded.bytes[8]);
    EXPECT_EQ(std::byte{0x00}, encoded.bytes[9]);
    EXPECT_EQ(std::byte{0xD4}, encoded.bytes[42]);
    EXPECT_EQ(std::byte{0xC3}, encoded.bytes[43]);
    EXPECT_EQ(std::byte{0xB2}, encoded.bytes[44]);
    EXPECT_EQ(std::byte{0xA1}, encoded.bytes[45]);
    EXPECT_EQ(std::byte{0x07}, encoded.bytes[46]);
}

TEST(GameUdpCodec, RoundTripsSnapshotAckAndKeyframeRequest)
{
    ClientInput input;
    input.match = MatchId{7U};
    input.player = PlayerId{3U};
    input.token = Token(0x20U);
    input.inputSequence = 11U;
    input.acknowledgedSnapshotId = 9U;
    input.requestKeyframe = true;

    ExpectClientRoundTrip(ClientDatagram{input});
}

TEST(GameUdpCodec, AcceptsExactlyTwelveHundredBytesAndRejectsOneMore)
{
    SnapshotFragment maximum = SingleFragment(
        Payload(MaxSnapshotFragmentPayloadBytes));
    const EncodedDatagram encoded = EncodeServerDatagram(
        ServerDatagram{maximum});
    EXPECT_EQ(MaxUdpDatagramBytes, encoded.bytes.size());

    maximum.bytes.push_back(std::byte{0x00});
    maximum.fullPayloadBytes = static_cast<std::uint32_t>(maximum.bytes.size());
    EXPECT_THROW(
        (void)EncodeServerDatagram(ServerDatagram{maximum}),
        std::invalid_argument);
}

TEST(GameUdpCodec, FragmentsPayloadAtTheLockedBoundary)
{
    const std::vector<std::byte> payload = Payload(
        MaxSnapshotFragmentPayloadBytes + 1U);
    const auto fragments = FragmentSnapshot(
        MatchId{1U},
        2U,
        4U,
        6U,
        payload);

    ASSERT_EQ(2U, fragments.size());
    EXPECT_EQ(MaxSnapshotFragmentPayloadBytes, fragments[0].bytes.size());
    EXPECT_EQ(1U, fragments[1].bytes.size());
    EXPECT_EQ(0U, fragments[0].fragmentIndex);
    EXPECT_EQ(1U, fragments[1].fragmentIndex);
    EXPECT_EQ(2U, fragments[0].fragmentCount);
    EXPECT_EQ(payload.size(), fragments[0].fullPayloadBytes);
    EXPECT_EQ(0x060A9727U, fragments[0].fullPayloadCrc32);
    EXPECT_EQ(fragments[0].fullPayloadCrc32,
              fragments[1].fullPayloadCrc32);
}

TEST(GameUdpCodec, SupportsThirtyTwoFragmentsAndRejectsPayloadAboveLimit)
{
    const auto maximum = FragmentSnapshot(
        MatchId{1U},
        2U,
        4U,
        0U,
        Payload(MaxSnapshotPayloadBytes));
    ASSERT_EQ(MaxSnapshotFragments, maximum.size());
    EXPECT_EQ(MaxSnapshotFragmentPayloadBytes, maximum.back().bytes.size());

    EXPECT_THROW(
        (void)FragmentSnapshot(
            MatchId{1U},
            2U,
            4U,
            0U,
            Payload(MaxSnapshotPayloadBytes + 1U)),
        std::length_error);
    EXPECT_THROW(
        (void)FragmentSnapshot(MatchId{1U}, 2U, 4U, 0U, {}),
        std::invalid_argument);
}

TEST(GameUdpCodec, RejectsBadHeaderLengthAndWrongDirection)
{
    EncodedDatagram bind = EncodeClientDatagram(ClientDatagram{UdpBind{
        MatchId{1U}, PlayerId{2U}, Token(0x10U)}});

    std::vector<std::byte> badMagic = bind.bytes;
    badMagic[0] = std::byte{0x00};
    EXPECT_EQ(DecodeError::InvalidValue,
              DecodeClientDatagram(badMagic).error);

    std::vector<std::byte> badVersion = bind.bytes;
    badVersion[4] = std::byte{0x01};
    EXPECT_EQ(DecodeError::InvalidValue,
              DecodeClientDatagram(badVersion).error);

    std::vector<std::byte> badReserved = bind.bytes;
    badReserved[7] = std::byte{0x01};
    EXPECT_EQ(DecodeError::InvalidValue,
              DecodeClientDatagram(badReserved).error);

    std::vector<std::byte> truncated = bind.bytes;
    truncated[8] = static_cast<std::byte>(
        static_cast<std::uint8_t>(std::to_integer<std::uint8_t>(truncated[8]) + 1U));
    EXPECT_EQ(DecodeError::Truncated,
              DecodeClientDatagram(truncated).error);

    std::vector<std::byte> trailing = bind.bytes;
    trailing[8] = static_cast<std::byte>(
        static_cast<std::uint8_t>(std::to_integer<std::uint8_t>(trailing[8]) - 1U));
    EXPECT_EQ(DecodeError::TrailingBytes,
              DecodeClientDatagram(trailing).error);

    std::vector<std::byte> oversized = bind.bytes;
    oversized.resize(MaxUdpDatagramBytes + 1U);
    EXPECT_EQ(DecodeError::CountLimit,
              DecodeClientDatagram(oversized).error);

    EXPECT_EQ(DecodeError::InvalidValue,
              DecodeServerDatagram(bind.bytes).error);
}

TEST(GameUdpCodec, RejectsInvalidInputAndFragmentMetadata)
{
    ClientInput invalidInput{
        MatchId{1U},
        PlayerId{2U},
        Token(1U),
        0U,
        0U,
        false,
        NetworkVec2{},
        false,
        EntityId{},
        false};
    EXPECT_THROW(
        (void)EncodeClientDatagram(ClientDatagram{invalidInput}),
        std::invalid_argument);

    ClientInput validInput;
    validInput.match = MatchId{1U};
    validInput.player = PlayerId{2U};
    validInput.token = Token(1U);
    validInput.inputSequence = 1U;
    std::vector<std::byte> unknownInputFlag =
        EncodeClientDatagram(ClientDatagram{validInput}).bytes;
    unknownInputFlag[46] = std::byte{0x80};
    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeClientDatagram(unknownInputFlag).error);

    invalidInput.inputSequence = 1U;
    invalidInput.hasMoveDestination = true;
    invalidInput.moveDestination.x =
        std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(
        (void)EncodeClientDatagram(ClientDatagram{invalidInput}),
        std::invalid_argument);

    SnapshotFragment invalidFragment = SingleFragment(Payload(1U));
    invalidFragment.snapshotId = 0U;
    EXPECT_THROW(
        (void)EncodeServerDatagram(ServerDatagram{invalidFragment}),
        std::invalid_argument);

    invalidFragment = SingleFragment(Payload(1U));
    invalidFragment.fragmentIndex = 1U;
    EXPECT_THROW(
        (void)EncodeServerDatagram(ServerDatagram{invalidFragment}),
        std::invalid_argument);

    invalidFragment = SingleFragment(Payload(1U));
    invalidFragment.fullPayloadBytes = 2U;
    EXPECT_THROW(
        (void)EncodeServerDatagram(ServerDatagram{invalidFragment}),
        std::invalid_argument);
}
