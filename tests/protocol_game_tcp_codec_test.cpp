#include <dxa/protocol/ByteCodec.hpp>
#include <dxa/protocol/GameTcpMessageCodec.hpp>
#include <dxa/protocol/GameTcpMessages.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace
{
using namespace dxa::protocol;

[[nodiscard]] MatchTicketValue Ticket(const std::uint8_t seed)
{
    MatchTicketValue ticket;
    for (std::size_t index = 0; index < ticket.size(); ++index)
    {
        ticket[index] = static_cast<std::byte>(
            static_cast<std::uint8_t>(seed + index));
    }
    return ticket;
}

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

void ExpectClientRoundTrip(const GameClientMessage& source)
{
    const EncodedMessage encoded = EncodeGameClientMessage(source);
    const auto decoded = DecodeGameClientMessage(encoded.type, encoded.payload);
    ASSERT_TRUE(decoded.message.has_value());
    EXPECT_EQ(DecodeError::None, decoded.error);
    EXPECT_TRUE(source == *decoded.message);
}

void ExpectServerRoundTrip(const GameServerMessage& source)
{
    const EncodedMessage encoded = EncodeGameServerMessage(source);
    const auto decoded = DecodeGameServerMessage(encoded.type, encoded.payload);
    ASSERT_TRUE(decoded.message.has_value());
    EXPECT_EQ(DecodeError::None, decoded.error);
    EXPECT_TRUE(source == *decoded.message);
}
} // namespace

TEST(GameTcpCodec, EncodesHelloInLockedOrderWithoutPrintingTicketBytes)
{
    const MatchTicketValue ticket = Ticket(0x20U);
    const EncodedMessage encoded = EncodeGameClientMessage(GameClientMessage{
        GameClientHello{MatchId{4U}, PlayerId{9U}, ticket}});

    EXPECT_EQ(MessageType::GameClientHello, encoded.type);
    ASSERT_EQ(28U, encoded.payload.size());
    EXPECT_EQ(std::byte{0x04}, encoded.payload[0]);
    EXPECT_EQ(std::byte{0x00}, encoded.payload[7]);
    EXPECT_EQ(std::byte{0x09}, encoded.payload[8]);
    EXPECT_EQ(std::byte{0x00}, encoded.payload[11]);
    EXPECT_TRUE(std::equal(
        ticket.begin(),
        ticket.end(),
        encoded.payload.begin() + 12));
}

TEST(GameTcpCodec, RoundTripsEveryGameTcpMessage)
{
    ExpectClientRoundTrip(GameClientMessage{GameClientHello{
        MatchId{4U}, PlayerId{9U}, Ticket(0x20U)}});
    ExpectServerRoundTrip(GameServerMessage{GameServerWelcome{
        MatchId{4U},
        PlayerId{9U},
        EntityId{0U},
        GameTickRate,
        SnapshotRate,
        1U,
        0x12345678U,
        Token(0x40U)}});
    ExpectServerRoundTrip(GameServerMessage{GameServerErrorMessage{
        GameServerErrorCode::AuthenticationFailed}});
    ExpectServerRoundTrip(GameServerMessage{GameMatchResult{
        MatchId{4U},
        EntityId{0U},
        true,
        MatchCompletionReason::LastSurvivor,
        90U}});
    ExpectServerRoundTrip(GameServerMessage{GameMatchResult{
        MatchId{4U},
        EntityId{},
        false,
        MatchCompletionReason::NoAuthenticatedPlayers,
        0U}});
}

TEST(GameTcpCodec, PreservesZeroPublicIdsAsRepresentableValues)
{
    ExpectClientRoundTrip(GameClientMessage{GameClientHello{
        MatchId{}, PlayerId{}, Ticket(1U)}});
    ExpectServerRoundTrip(GameServerMessage{GameServerWelcome{
        MatchId{},
        PlayerId{},
        EntityId{},
        GameTickRate,
        SnapshotRate,
        1U,
        0U,
        Token(1U)}});
}

TEST(GameTcpCodec, EnforcesWelcomeRatesAndMapIdentity)
{
    GameServerWelcome welcome{
        MatchId{1U},
        PlayerId{2U},
        EntityId{},
        GameTickRate,
        SnapshotRate,
        1U,
        0x1234U,
        Token(1U)};

    welcome.tickRate = 29U;
    EXPECT_THROW(
        (void)EncodeGameServerMessage(GameServerMessage{welcome}),
        std::invalid_argument);
    welcome.tickRate = GameTickRate;
    welcome.snapshotRate = 14U;
    EXPECT_THROW(
        (void)EncodeGameServerMessage(GameServerMessage{welcome}),
        std::invalid_argument);
    welcome.snapshotRate = SnapshotRate;
    welcome.mapId = 0U;
    EXPECT_THROW(
        (void)EncodeGameServerMessage(GameServerMessage{welcome}),
        std::invalid_argument);
}

TEST(GameTcpCodec, EnforcesPublicErrorAndResultWinnerRules)
{
    EXPECT_THROW(
        (void)EncodeGameServerMessage(GameServerMessage{
            GameServerErrorMessage{static_cast<GameServerErrorCode>(99U)}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)EncodeGameServerMessage(GameServerMessage{GameMatchResult{
            MatchId{1U},
            EntityId{},
            false,
            MatchCompletionReason::LastSurvivor,
            1U}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)EncodeGameServerMessage(GameServerMessage{GameMatchResult{
            MatchId{1U},
            EntityId{2U},
            true,
            MatchCompletionReason::NoConnectedPlayers,
            1U}}),
        std::invalid_argument);
}

TEST(GameTcpCodec, RejectsWrongDirectionInvalidEnumAndTrailingBytes)
{
    const EncodedMessage hello = EncodeGameClientMessage(GameClientMessage{
        GameClientHello{MatchId{1U}, PlayerId{2U}, Ticket(3U)}});
    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeGameServerMessage(hello.type, hello.payload).error);

    ByteWriter invalidError;
    invalidError.WriteU8(99U);
    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeGameServerMessage(
            MessageType::GameServerError,
            std::move(invalidError).Finish()).error);

    EncodedMessage welcome = EncodeGameServerMessage(GameServerMessage{
        GameServerWelcome{
            MatchId{1U},
            PlayerId{2U},
            EntityId{},
            GameTickRate,
            SnapshotRate,
            1U,
            0x1234U,
            Token(1U)}});
    welcome.payload.push_back(std::byte{0x00});
    EXPECT_EQ(
        DecodeError::TrailingBytes,
        DecodeGameServerMessage(welcome.type, welcome.payload).error);

    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeGameClientMessage(MessageType::WorkerRegister, {}).error);
}
