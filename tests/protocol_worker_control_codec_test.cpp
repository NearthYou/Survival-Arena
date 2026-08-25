#include <dxa/protocol/ByteCodec.hpp>
#include <dxa/protocol/WorkerControlMessageCodec.hpp>
#include <dxa/protocol/WorkerControlMessages.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

void ExpectWorkerRoundTrip(const WorkerToLobbyMessage& source)
{
    const EncodedMessage encoded = EncodeWorkerToLobbyMessage(source);
    const auto decoded = DecodeWorkerToLobbyMessage(
        encoded.type,
        encoded.payload);
    ASSERT_TRUE(decoded.message.has_value());
    EXPECT_EQ(DecodeError::None, decoded.error);
    EXPECT_TRUE(source == *decoded.message);
}

void ExpectLobbyRoundTrip(const LobbyToWorkerMessage& source)
{
    const EncodedMessage encoded = EncodeLobbyToWorkerMessage(source);
    const auto decoded = DecodeLobbyToWorkerMessage(
        encoded.type,
        encoded.payload);
    ASSERT_TRUE(decoded.message.has_value());
    EXPECT_EQ(DecodeError::None, decoded.error);
    EXPECT_TRUE(source == *decoded.message);
}

[[nodiscard]] ReserveMatch Reservation(
    std::vector<ReservedParticipant> participants = {
        {PlayerId{2U}, Ticket(1U)},
        {PlayerId{5U}, Ticket(2U)}})
{
    return ReserveMatch{
        ReservationId{9U},
        MatchId{11U},
        20260824U,
        60000U,
        std::move(participants)};
}
} // namespace

TEST(WorkerControlCodec, RoundTripsEveryWorkerToLobbyMessage)
{
    ExpectWorkerRoundTrip(WorkerToLobbyMessage{WorkerRegister{
        WorkerId{7U}, "127.0.0.1", 7100U, 7101U, 1U}});
    ExpectWorkerRoundTrip(WorkerToLobbyMessage{ReserveMatchReady{
        ReservationId{9U}, MatchId{11U}}});
    ExpectWorkerRoundTrip(WorkerToLobbyMessage{ReserveMatchRejected{
        ReservationId{9U},
        MatchId{11U},
        WorkerReservationReject::SimulationInitializationFailed}});
    ExpectWorkerRoundTrip(WorkerToLobbyMessage{MatchReservationCancelled{
        ReservationId{9U}, MatchId{11U}}});
    ExpectWorkerRoundTrip(WorkerToLobbyMessage{MatchFinished{
        MatchId{11U},
        EntityId{0U},
        true,
        MatchCompletionReason::LastSurvivor,
        320U}});
    ExpectWorkerRoundTrip(WorkerToLobbyMessage{MatchFinished{
        MatchId{11U},
        EntityId{},
        false,
        MatchCompletionReason::NoConnectedPlayers,
        321U}});
}

TEST(WorkerControlCodec, RoundTripsEveryLobbyToWorkerMessage)
{
    ExpectLobbyRoundTrip(LobbyToWorkerMessage{WorkerRegistered{WorkerId{7U}}});
    ExpectLobbyRoundTrip(LobbyToWorkerMessage{Reservation()});
    ExpectLobbyRoundTrip(LobbyToWorkerMessage{CancelMatchReservation{
        ReservationId{9U}, MatchId{11U}}});
}

TEST(WorkerControlCodec, EncodesRegistrationInLockedWireOrder)
{
    const EncodedMessage encoded = EncodeWorkerToLobbyMessage(
        WorkerToLobbyMessage{WorkerRegister{
            WorkerId{0x01020304U}, "x", 0x1122U, 0x3344U, 1U}});

    const std::vector<std::byte> expected{
        std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01},
        std::byte{0x01}, std::byte{0x78},
        std::byte{0x22}, std::byte{0x11},
        std::byte{0x44}, std::byte{0x33},
        std::byte{0x01}};
    EXPECT_EQ(MessageType::WorkerRegister, encoded.type);
    EXPECT_EQ(expected, encoded.payload);
}

TEST(WorkerControlCodec, CanonicalizesReservationParticipantsByPlayerId)
{
    const EncodedMessage encoded = EncodeLobbyToWorkerMessage(
        LobbyToWorkerMessage{Reservation({
            {PlayerId{9U}, Ticket(9U)},
            {PlayerId{2U}, Ticket(2U)},
            {PlayerId{5U}, Ticket(5U)}})});

    const auto decoded = DecodeLobbyToWorkerMessage(
        encoded.type,
        encoded.payload);
    ASSERT_TRUE(decoded.message.has_value());
    const auto& reservation = std::get<ReserveMatch>(*decoded.message);
    ASSERT_EQ(3U, reservation.participants.size());
    EXPECT_EQ(PlayerId{2U}, reservation.participants[0].player);
    EXPECT_EQ(PlayerId{5U}, reservation.participants[1].player);
    EXPECT_EQ(PlayerId{9U}, reservation.participants[2].player);
}

TEST(WorkerControlCodec, EnforcesRegistrationIdentityAndEndpointBounds)
{
    ExpectWorkerRoundTrip(WorkerToLobbyMessage{WorkerRegister{
        WorkerId{1U}, std::string(255U, 'a'), 1U, 65535U, 1U}});

    EXPECT_THROW(
        (void)EncodeWorkerToLobbyMessage(WorkerToLobbyMessage{WorkerRegister{
            WorkerId{}, "worker", 1U, 2U, 1U}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)EncodeWorkerToLobbyMessage(WorkerToLobbyMessage{WorkerRegister{
            WorkerId{1U}, "", 1U, 2U, 1U}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)EncodeWorkerToLobbyMessage(WorkerToLobbyMessage{WorkerRegister{
            WorkerId{1U}, std::string(256U, 'a'), 1U, 2U, 1U}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)EncodeWorkerToLobbyMessage(WorkerToLobbyMessage{WorkerRegister{
            WorkerId{1U}, std::string{"bad\nworker"}, 1U, 2U, 1U}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)EncodeWorkerToLobbyMessage(WorkerToLobbyMessage{WorkerRegister{
            WorkerId{1U}, "worker", 0U, 2U, 1U}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)EncodeWorkerToLobbyMessage(WorkerToLobbyMessage{WorkerRegister{
            WorkerId{1U}, "worker", 1U, 2U, 2U}}),
        std::invalid_argument);
}

TEST(WorkerControlCodec, EnforcesReservationParticipantAndLifetimeBounds)
{
    std::vector<ReservedParticipant> maximum;
    maximum.reserve(RoomCapacity);
    for (std::uint32_t value = 0U; value < RoomCapacity; ++value)
    {
        maximum.push_back({PlayerId{value}, Ticket(static_cast<std::uint8_t>(value))});
    }
    ExpectLobbyRoundTrip(LobbyToWorkerMessage{Reservation(maximum)});

    EXPECT_THROW(
        (void)EncodeLobbyToWorkerMessage(LobbyToWorkerMessage{Reservation({})}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)EncodeLobbyToWorkerMessage(LobbyToWorkerMessage{Reservation({
            {PlayerId{1U}, Ticket(1U)}})}),
        std::invalid_argument);

    maximum.push_back({PlayerId{24U}, Ticket(24U)});
    EXPECT_THROW(
        (void)EncodeLobbyToWorkerMessage(LobbyToWorkerMessage{Reservation(maximum)}),
        std::invalid_argument);

    EXPECT_THROW(
        (void)EncodeLobbyToWorkerMessage(LobbyToWorkerMessage{Reservation({
            {PlayerId{1U}, Ticket(1U)},
            {PlayerId{1U}, Ticket(2U)}})}),
        std::invalid_argument);

    ReserveMatch zeroLifetime = Reservation();
    zeroLifetime.ticketLifetimeMilliseconds = 0U;
    EXPECT_THROW(
        (void)EncodeLobbyToWorkerMessage(LobbyToWorkerMessage{zeroLifetime}),
        std::invalid_argument);

    ReserveMatch longLifetime = Reservation();
    longLifetime.ticketLifetimeMilliseconds = 60001U;
    EXPECT_THROW(
        (void)EncodeLobbyToWorkerMessage(LobbyToWorkerMessage{longLifetime}),
        std::invalid_argument);
}

TEST(WorkerControlCodec, RejectsWrongDirectionTrailingBytesAndInvalidEnum)
{
    const EncodedMessage registration = EncodeWorkerToLobbyMessage(
        WorkerToLobbyMessage{WorkerRegister{
            WorkerId{1U}, "worker", 1U, 2U, 1U}});
    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeLobbyToWorkerMessage(
            registration.type,
            registration.payload).error);

    EncodedMessage ready = EncodeWorkerToLobbyMessage(
        WorkerToLobbyMessage{ReserveMatchReady{
            ReservationId{1U}, MatchId{2U}}});
    ready.payload.push_back(std::byte{0x00});
    EXPECT_EQ(
        DecodeError::TrailingBytes,
        DecodeWorkerToLobbyMessage(ready.type, ready.payload).error);

    ByteWriter invalidReject;
    invalidReject.WriteU64(1U);
    invalidReject.WriteU64(2U);
    invalidReject.WriteU8(99U);
    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeWorkerToLobbyMessage(
            MessageType::ReserveMatchRejected,
            std::move(invalidReject).Finish()).error);

    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeWorkerToLobbyMessage(MessageType::ClientHello, {}).error);
}

TEST(WorkerControlCodec, EnforcesReservationIdsAndCompletionWinnerRules)
{
    EXPECT_THROW(
        (void)EncodeWorkerToLobbyMessage(WorkerToLobbyMessage{ReserveMatchReady{
            ReservationId{}, MatchId{1U}}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)EncodeLobbyToWorkerMessage(LobbyToWorkerMessage{WorkerRegistered{
            WorkerId{}}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)EncodeWorkerToLobbyMessage(WorkerToLobbyMessage{MatchFinished{
            MatchId{1U},
            EntityId{},
            false,
            MatchCompletionReason::LastSurvivor,
            1U}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)EncodeWorkerToLobbyMessage(WorkerToLobbyMessage{MatchFinished{
            MatchId{1U},
            EntityId{2U},
            true,
            MatchCompletionReason::NoAuthenticatedPlayers,
            1U}}),
        std::invalid_argument);
}
