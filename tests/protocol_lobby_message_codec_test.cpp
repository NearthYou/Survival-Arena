#include <dxa/protocol/ByteCodec.hpp>
#include <dxa/protocol/LobbyMessageCodec.hpp>
#include <dxa/protocol/LobbyMessages.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace dxa::protocol;

void ExpectClientRoundTrip(const ClientMessage& source)
{
    const EncodedMessage encoded = EncodeClientMessage(source);
    const auto decoded = DecodeClientMessage(encoded.type, encoded.payload);
    ASSERT_TRUE(decoded.message.has_value());
    EXPECT_EQ(DecodeError::None, decoded.error);
    EXPECT_EQ(source, *decoded.message);
}

void ExpectServerRoundTrip(const ServerMessage& source)
{
    const EncodedMessage encoded = EncodeServerMessage(source);
    const auto decoded = DecodeServerMessage(encoded.type, encoded.payload);
    ASSERT_TRUE(decoded.message.has_value());
    EXPECT_EQ(DecodeError::None, decoded.error);
    EXPECT_EQ(source, *decoded.message);
}

[[nodiscard]] MatchTicketValue TicketBytes()
{
    MatchTicketValue ticket;
    for (std::size_t index = 0; index < ticket.size(); ++index)
    {
        ticket[index] = static_cast<std::byte>(index + 1U);
    }
    return ticket;
}
} // namespace

TEST(LobbyMessageCodec, RoundTripsEveryClientMessage)
{
    ExpectClientRoundTrip(ClientMessage{ClientHello{1U}});
    ExpectClientRoundTrip(ClientMessage{ListRoomsRequest{2U}});
    ExpectClientRoundTrip(ClientMessage{CreateRoomRequest{3U}});
    ExpectClientRoundTrip(ClientMessage{JoinRoomRequest{4U, RoomId{7U}}});
    ExpectClientRoundTrip(ClientMessage{LeaveRoomRequest{5U}});
    ExpectClientRoundTrip(ClientMessage{SetReadyRequest{6U, true}});
    ExpectClientRoundTrip(ClientMessage{StartMatchRequest{7U}});
}

TEST(LobbyMessageCodec, RoundTripsEveryServerMessage)
{
    ExpectServerRoundTrip(ServerMessage{ServerWelcome{1U, PlayerId{9U}}});
    ExpectServerRoundTrip(ServerMessage{RoomListResponse{
        2U,
        {{RoomId{1U}, 1U, 24U}, {RoomId{2U}, 3U, 24U}}}});
    ExpectServerRoundTrip(ServerMessage{RoomSnapshot{
        3U,
        RoomId{8U},
        RoomState::Waiting,
        PlayerId{2U},
        {{PlayerId{2U}, true}, {PlayerId{3U}, false}}}});
    ExpectServerRoundTrip(ServerMessage{MatchTicket{
        4U,
        MatchId{99U},
        TicketBytes(),
        "127.0.0.1",
        7100U,
        7101U,
        60U}});
    ExpectServerRoundTrip(ServerMessage{ErrorResponse{
        5U,
        LobbyError::WorkerUnavailable}});
}

TEST(LobbyMessageCodec, EncodesJoinRoomInLockedWireOrder)
{
    const EncodedMessage encoded = EncodeClientMessage(
        ClientMessage{JoinRoomRequest{7U, RoomId{42U}}});

    EXPECT_EQ(MessageType::JoinRoomRequest, encoded.type);
    const std::vector<std::byte> expected{
        std::byte{0x07}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x2A}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_EQ(expected, encoded.payload);
}

TEST(LobbyMessageCodec, SortsRoomAndMemberViewsBeforeEncoding)
{
    const ServerMessage rooms = RoomListResponse{
        1U,
        {{RoomId{9U}, 1U, 24U}, {RoomId{2U}, 2U, 24U}}};
    const auto decodedRooms = DecodeServerMessage(
        MessageType::RoomListResponse,
        EncodeServerMessage(rooms).payload);
    ASSERT_TRUE(decodedRooms.message.has_value());
    const auto& roomList = std::get<RoomListResponse>(*decodedRooms.message);
    ASSERT_EQ(2U, roomList.rooms.size());
    EXPECT_EQ(RoomId{2U}, roomList.rooms[0].room);
    EXPECT_EQ(RoomId{9U}, roomList.rooms[1].room);

    const ServerMessage snapshot = RoomSnapshot{
        2U,
        RoomId{1U},
        RoomState::Waiting,
        PlayerId{1U},
        {{PlayerId{8U}, false}, {PlayerId{3U}, true}}};
    const auto decodedSnapshot = DecodeServerMessage(
        MessageType::RoomSnapshot,
        EncodeServerMessage(snapshot).payload);
    ASSERT_TRUE(decodedSnapshot.message.has_value());
    const auto& room = std::get<RoomSnapshot>(*decodedSnapshot.message);
    ASSERT_EQ(2U, room.members.size());
    EXPECT_EQ(PlayerId{3U}, room.members[0].player);
    EXPECT_EQ(PlayerId{8U}, room.members[1].player);
}

TEST(LobbyMessageCodec, RejectsZeroRequestInvalidBoolAndTrailingBytes)
{
    ByteWriter zeroRequest;
    zeroRequest.WriteU32(0U);
    const auto zeroPayload = std::move(zeroRequest).Finish();
    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeClientMessage(MessageType::ClientHello, zeroPayload).error);

    ByteWriter invalidBool;
    invalidBool.WriteU32(1U);
    invalidBool.WriteU8(2U);
    const auto boolPayload = std::move(invalidBool).Finish();
    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeClientMessage(MessageType::SetReadyRequest, boolPayload).error);

    EncodedMessage hello = EncodeClientMessage(ClientMessage{ClientHello{1U}});
    hello.payload.push_back(std::byte{0x00});
    EXPECT_EQ(
        DecodeError::TrailingBytes,
        DecodeClientMessage(hello.type, hello.payload).error);
}

TEST(LobbyMessageCodec, RejectsRoomAndMemberCountsAboveLimits)
{
    ByteWriter roomList;
    roomList.WriteU32(1U);
    roomList.WriteU16(static_cast<std::uint16_t>(MaximumRooms + 1U));
    const auto roomListPayload = std::move(roomList).Finish();
    EXPECT_EQ(
        DecodeError::CountLimit,
        DecodeServerMessage(MessageType::RoomListResponse, roomListPayload).error);

    ByteWriter snapshot;
    snapshot.WriteU32(1U);
    snapshot.WriteU32(2U);
    snapshot.WriteU8(static_cast<std::uint8_t>(RoomState::Waiting));
    snapshot.WriteU32(3U);
    snapshot.WriteU8(static_cast<std::uint8_t>(RoomCapacity + 1U));
    const auto snapshotPayload = std::move(snapshot).Finish();
    EXPECT_EQ(
        DecodeError::CountLimit,
        DecodeServerMessage(MessageType::RoomSnapshot, snapshotPayload).error);
}

TEST(LobbyMessageCodec, EnforcesTicketEndpointAndHostLimits)
{
    MatchTicket maximum{
        1U,
        MatchId{1U},
        TicketBytes(),
        std::string(255U, 'a'),
        1U,
        2U,
        60U};
    ExpectServerRoundTrip(ServerMessage{maximum});

    maximum.host.push_back('a');
    EXPECT_THROW(
        (void)EncodeServerMessage(ServerMessage{maximum}),
        std::invalid_argument);

    maximum.host = "server";
    maximum.tcpPort = 0U;
    EXPECT_THROW(
        (void)EncodeServerMessage(ServerMessage{maximum}),
        std::invalid_argument);

    maximum.tcpPort = 1U;
    maximum.expiresInSeconds = 1U;
    ExpectServerRoundTrip(ServerMessage{maximum});

    maximum.expiresInSeconds = 0U;
    EXPECT_THROW(
        (void)EncodeServerMessage(ServerMessage{maximum}),
        std::invalid_argument);

    maximum.expiresInSeconds = 61U;
    EXPECT_THROW(
        (void)EncodeServerMessage(ServerMessage{maximum}),
        std::invalid_argument);
}

TEST(LobbyMessageCodec, RejectsDirectionAndInvalidEnumValues)
{
    const EncodedMessage welcome = EncodeServerMessage(
        ServerMessage{ServerWelcome{1U, PlayerId{1U}}});
    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeClientMessage(welcome.type, welcome.payload).error);

    ByteWriter snapshot;
    snapshot.WriteU32(1U);
    snapshot.WriteU32(1U);
    snapshot.WriteU8(99U);
    snapshot.WriteU32(1U);
    snapshot.WriteU8(0U);
    const auto payload = std::move(snapshot).Finish();
    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeServerMessage(MessageType::RoomSnapshot, payload).error);

    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeClientMessage(MessageType::GameClientHello, {}).error);
    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeServerMessage(MessageType::GameServerWelcome, {}).error);
}
