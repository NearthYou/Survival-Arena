#include <dxa/lobby/LobbyService.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

namespace
{
using dxa::lobby::ConnectionId;
using dxa::lobby::GameEndpoint;
using dxa::lobby::ITicketSource;
using dxa::lobby::LobbyAuditEventType;
using dxa::lobby::LobbyService;
using dxa::lobby::LobbyServiceConfig;
using dxa::lobby::LobbyServiceResult;
using dxa::lobby::MatchTicketRegistry;
using dxa::lobby::StaticGameWorkerAllocator;
using dxa::lobby::UnavailableGameWorkerAllocator;
using dxa::protocol::ClientHello;
using dxa::protocol::ClientMessage;
using dxa::protocol::CreateRoomRequest;
using dxa::protocol::ErrorResponse;
using dxa::protocol::JoinRoomRequest;
using dxa::protocol::LeaveRoomRequest;
using dxa::protocol::ListRoomsRequest;
using dxa::protocol::LobbyError;
using dxa::protocol::MatchId;
using dxa::protocol::PlayerId;
using dxa::protocol::RoomId;
using dxa::protocol::RoomListResponse;
using dxa::protocol::RoomSnapshot;
using dxa::protocol::ServerWelcome;
using dxa::protocol::SetReadyRequest;
using dxa::protocol::StartMatchRequest;

[[nodiscard]] std::chrono::steady_clock::time_point Time(
    const std::int64_t seconds)
{
    return std::chrono::steady_clock::time_point{
        std::chrono::seconds{seconds}};
}

class NullTicketSource final : public ITicketSource
{
public:
    [[nodiscard]] bool Fill(
        std::span<std::byte, dxa::protocol::MatchTicketBytes>) noexcept override
    {
        return false;
    }
};

struct ServiceFixture
{
    explicit ServiceFixture(const LobbyServiceConfig config = {})
        : tickets{ticketSource},
          service{allocator, tickets, config}
    {
    }

    NullTicketSource ticketSource;
    MatchTicketRegistry tickets;
    UnavailableGameWorkerAllocator allocator;
    LobbyService service;
};

template <typename Message>
[[nodiscard]] const Message& OnlyMessage(const LobbyServiceResult& result)
{
    if (result.outbound.size() != 1U)
    {
        throw std::logic_error{"expected exactly one outbound message"};
    }
    const auto* message = std::get_if<Message>(&result.outbound.front().message);
    if (message == nullptr)
    {
        throw std::logic_error{"unexpected outbound message type"};
    }
    return *message;
}

template <typename Message>
[[nodiscard]] const Message& MessageFor(
    const LobbyServiceResult& result,
    const ConnectionId recipient)
{
    const Message* found = nullptr;
    for (const auto& outbound : result.outbound)
    {
        if (outbound.recipient != recipient)
        {
            continue;
        }
        const auto* candidate = std::get_if<Message>(&outbound.message);
        if (candidate == nullptr || found != nullptr)
        {
            throw std::logic_error{"unexpected outbound message collection"};
        }
        found = candidate;
    }
    if (found == nullptr)
    {
        throw std::logic_error{"recipient message not found"};
    }
    return *found;
}

[[nodiscard]] const ErrorResponse& OnlyError(const LobbyServiceResult& result)
{
    return OnlyMessage<ErrorResponse>(result);
}

[[nodiscard]] const ServerWelcome& OnlyWelcome(const LobbyServiceResult& result)
{
    return OnlyMessage<ServerWelcome>(result);
}

[[nodiscard]] const RoomSnapshot& OnlySnapshot(const LobbyServiceResult& result)
{
    return OnlyMessage<RoomSnapshot>(result);
}

[[nodiscard]] ConnectionId WelcomedConnection(
    ServiceFixture& fixture,
    const std::uint32_t helloRequestId = 1U)
{
    const auto connection = fixture.service.OpenConnection();
    if (!connection.has_value())
    {
        throw std::logic_error{"connection allocation failed"};
    }
    const auto result = fixture.service.Handle(
        *connection,
        ClientMessage{ClientHello{helloRequestId}},
        Time(0));
    static_cast<void>(OnlyWelcome(result));
    return *connection;
}

[[nodiscard]] RoomId CreateRoom(
    ServiceFixture& fixture,
    const ConnectionId host,
    const std::uint32_t requestId)
{
    const auto result = fixture.service.Handle(
        host,
        ClientMessage{CreateRoomRequest{requestId}},
        Time(0));
    return OnlySnapshot(result).room;
}

void JoinRoom(
    ServiceFixture& fixture,
    const ConnectionId guest,
    const std::uint32_t requestId,
    const RoomId room)
{
    const auto result = fixture.service.Handle(
        guest,
        ClientMessage{JoinRoomRequest{requestId, room}},
        Time(0));
    static_cast<void>(MessageFor<RoomSnapshot>(result, guest));
}
} // namespace

TEST(LobbyService, RequiresHelloAndStrictlyIncreasingRequestIds)
{
    ServiceFixture fixture;
    const auto connection = fixture.service.OpenConnection();
    ASSERT_TRUE(connection.has_value());

    auto output = fixture.service.Handle(
        *connection,
        ClientMessage{ListRoomsRequest{1U}},
        Time(0));
    EXPECT_EQ(LobbyError::NotWelcomed, OnlyError(output).error);
    EXPECT_EQ(1U, OnlyError(output).requestId);

    output = fixture.service.Handle(
        *connection,
        ClientMessage{ClientHello{2U}},
        Time(0));
    EXPECT_EQ(PlayerId{1U}, OnlyWelcome(output).player);
    EXPECT_EQ(2U, OnlyWelcome(output).requestId);

    output = fixture.service.Handle(
        *connection,
        ClientMessage{CreateRoomRequest{2U}},
        Time(0));
    EXPECT_EQ(LobbyError::RequestOutOfOrder, OnlyError(output).error);
    EXPECT_EQ(0U, fixture.service.RoomCount());

    output = fixture.service.Handle(
        *connection,
        ClientMessage{ClientHello{3U}},
        Time(0));
    EXPECT_EQ(LobbyError::AlreadyWelcomed, OnlyError(output).error);

    output = fixture.service.Handle(
        *connection,
        ClientMessage{ListRoomsRequest{3U}},
        Time(0));
    EXPECT_EQ(LobbyError::RequestOutOfOrder, OnlyError(output).error);

    output = fixture.service.Handle(
        *connection,
        ClientMessage{ListRoomsRequest{0U}},
        Time(0));
    EXPECT_EQ(LobbyError::RequestOutOfOrder, OnlyError(output).error);
}

TEST(LobbyService, CreatesListsJoinsAndBroadcastsSortedSnapshots)
{
    ServiceFixture fixture;
    const ConnectionId first = WelcomedConnection(fixture);
    const ConnectionId second = WelcomedConnection(fixture);
    const ConnectionId observer = WelcomedConnection(fixture);

    const RoomId firstRoom = CreateRoom(fixture, first, 2U);
    const RoomId secondRoom = CreateRoom(fixture, second, 2U);
    EXPECT_LT(firstRoom, secondRoom);

    auto output = fixture.service.Handle(
        observer,
        ClientMessage{ListRoomsRequest{2U}},
        Time(0));
    const auto& roomList = OnlyMessage<RoomListResponse>(output);
    ASSERT_EQ(2U, roomList.rooms.size());
    EXPECT_EQ(firstRoom, roomList.rooms[0].room);
    EXPECT_EQ(secondRoom, roomList.rooms[1].room);
    EXPECT_EQ(1U, roomList.rooms[0].players);
    EXPECT_EQ(dxa::protocol::RoomCapacity, roomList.rooms[0].capacity);

    output = fixture.service.Handle(
        observer,
        ClientMessage{JoinRoomRequest{3U, firstRoom}},
        Time(0));
    ASSERT_EQ(2U, output.outbound.size());
    const auto& requesterSnapshot = MessageFor<RoomSnapshot>(output, observer);
    const auto& hostSnapshot = MessageFor<RoomSnapshot>(output, first);
    EXPECT_EQ(3U, requesterSnapshot.requestId);
    EXPECT_EQ(0U, hostSnapshot.requestId);
    ASSERT_EQ(2U, requesterSnapshot.members.size());
    EXPECT_LT(
        requesterSnapshot.members[0].player,
        requesterSnapshot.members[1].player);
}

TEST(LobbyService, RejectsAlreadyInRoomAndAdvancesSemanticFailures)
{
    ServiceFixture fixture;
    const ConnectionId first = WelcomedConnection(fixture);
    const ConnectionId second = WelcomedConnection(fixture);
    const RoomId firstRoom = CreateRoom(fixture, first, 2U);
    const RoomId secondRoom = CreateRoom(fixture, second, 2U);

    auto output = fixture.service.Handle(
        first,
        ClientMessage{CreateRoomRequest{3U}},
        Time(0));
    EXPECT_EQ(LobbyError::AlreadyInRoom, OnlyError(output).error);

    output = fixture.service.Handle(
        first,
        ClientMessage{JoinRoomRequest{4U, secondRoom}},
        Time(0));
    EXPECT_EQ(LobbyError::AlreadyInRoom, OnlyError(output).error);

    output = fixture.service.Handle(
        first,
        ClientMessage{JoinRoomRequest{5U, RoomId{9999U}}},
        Time(0));
    EXPECT_EQ(LobbyError::AlreadyInRoom, OnlyError(output).error);

    output = fixture.service.Handle(
        first,
        ClientMessage{ListRoomsRequest{5U}},
        Time(0));
    EXPECT_EQ(LobbyError::RequestOutOfOrder, OnlyError(output).error);
    EXPECT_TRUE(fixture.service.HasRoom(firstRoom));
}

TEST(LobbyService, ReadyBroadcastUsesRequesterAndPushRequestIds)
{
    ServiceFixture fixture;
    const ConnectionId host = WelcomedConnection(fixture);
    const ConnectionId guest = WelcomedConnection(fixture);
    const RoomId room = CreateRoom(fixture, host, 2U);
    JoinRoom(fixture, guest, 2U, room);

    const auto output = fixture.service.Handle(
        guest,
        ClientMessage{SetReadyRequest{3U, true}},
        Time(0));

    ASSERT_EQ(2U, output.outbound.size());
    const auto& guestSnapshot = MessageFor<RoomSnapshot>(output, guest);
    const auto& hostSnapshot = MessageFor<RoomSnapshot>(output, host);
    EXPECT_EQ(3U, guestSnapshot.requestId);
    EXPECT_EQ(0U, hostSnapshot.requestId);
    const auto readyMember = std::find_if(
        guestSnapshot.members.begin(),
        guestSnapshot.members.end(),
        [](const auto& member) { return member.ready; });
    ASSERT_NE(guestSnapshot.members.end(), readyMember);
    EXPECT_EQ(PlayerId{2U}, readyMember->player);
}

TEST(LobbyService, LeaveReturnsRoomListAndPushesOnlyRemainingMembers)
{
    ServiceFixture fixture;
    const ConnectionId host = WelcomedConnection(fixture);
    const ConnectionId guest = WelcomedConnection(fixture);
    const RoomId room = CreateRoom(fixture, host, 2U);
    JoinRoom(fixture, guest, 2U, room);

    const auto output = fixture.service.Handle(
        guest,
        ClientMessage{LeaveRoomRequest{3U}},
        Time(0));

    ASSERT_EQ(2U, output.outbound.size());
    const auto& roomList = MessageFor<RoomListResponse>(output, guest);
    EXPECT_EQ(3U, roomList.requestId);
    ASSERT_EQ(1U, roomList.rooms.size());
    EXPECT_EQ(1U, roomList.rooms.front().players);
    const auto& hostSnapshot = MessageFor<RoomSnapshot>(output, host);
    EXPECT_EQ(0U, hostSnapshot.requestId);
    ASSERT_EQ(1U, hostSnapshot.members.size());
    EXPECT_EQ(PlayerId{1U}, hostSnapshot.members.front().player);
}

TEST(LobbyService, LeaveDeletesEmptyRoomAndReturnsUpdatedList)
{
    ServiceFixture fixture;
    const ConnectionId host = WelcomedConnection(fixture);
    const RoomId room = CreateRoom(fixture, host, 2U);

    const auto output = fixture.service.Handle(
        host,
        ClientMessage{LeaveRoomRequest{3U}},
        Time(0));

    const auto& roomList = OnlyMessage<RoomListResponse>(output);
    EXPECT_EQ(3U, roomList.requestId);
    EXPECT_TRUE(roomList.rooms.empty());
    EXPECT_FALSE(fixture.service.HasRoom(room));
    ASSERT_EQ(1U, output.audit.size());
    EXPECT_EQ(LobbyAuditEventType::RoomDeleted, output.audit.front().type);
}

TEST(LobbyService, DisconnectTransfersHostAndDeletesEmptyRoom)
{
    ServiceFixture fixture;
    const ConnectionId host = WelcomedConnection(fixture);
    const ConnectionId next = WelcomedConnection(fixture);
    const RoomId room = CreateRoom(fixture, host, 2U);
    JoinRoom(fixture, next, 2U, room);

    auto output = fixture.service.Disconnect(host);
    EXPECT_EQ(PlayerId{2U}, OnlySnapshot(output).host);
    EXPECT_EQ(0U, OnlySnapshot(output).requestId);
    EXPECT_TRUE(fixture.service.HasRoom(room));
    ASSERT_EQ(1U, output.audit.size());
    EXPECT_EQ(LobbyAuditEventType::HostTransferred, output.audit.front().type);
    EXPECT_EQ(PlayerId{2U}, output.audit.front().player);

    output = fixture.service.Disconnect(next);
    EXPECT_TRUE(output.outbound.empty());
    EXPECT_FALSE(fixture.service.HasRoom(room));
    ASSERT_EQ(1U, output.audit.size());
    EXPECT_EQ(LobbyAuditEventType::RoomDeleted, output.audit.front().type);
}

TEST(LobbyService, EnforcesConfiguredRoomLimit)
{
    LobbyServiceConfig config;
    config.maximumRooms = 2U;
    ServiceFixture fixture{config};
    const ConnectionId first = WelcomedConnection(fixture);
    const ConnectionId second = WelcomedConnection(fixture);
    const ConnectionId third = WelcomedConnection(fixture);
    static_cast<void>(CreateRoom(fixture, first, 2U));
    static_cast<void>(CreateRoom(fixture, second, 2U));

    const auto output = fixture.service.Handle(
        third,
        ClientMessage{CreateRoomRequest{2U}},
        Time(0));

    EXPECT_EQ(LobbyError::RoomLimitReached, OnlyError(output).error);
    EXPECT_EQ(2U, fixture.service.RoomCount());
    EXPECT_EQ(dxa::protocol::MaximumRooms, LobbyServiceConfig{}.maximumRooms);
}

TEST(LobbyService, IssuesMaximumIdsOnceAndNeverWraps)
{
    LobbyServiceConfig connectionConfig;
    connectionConfig.nextConnection = std::numeric_limits<std::uint64_t>::max();
    ServiceFixture connectionFixture{connectionConfig};
    EXPECT_EQ(
        ConnectionId{std::numeric_limits<std::uint64_t>::max()},
        connectionFixture.service.OpenConnection());
    EXPECT_FALSE(connectionFixture.service.OpenConnection().has_value());

    LobbyServiceConfig playerConfig;
    playerConfig.nextPlayer = std::numeric_limits<std::uint32_t>::max();
    ServiceFixture playerFixture{playerConfig};
    const auto first = playerFixture.service.OpenConnection();
    const auto second = playerFixture.service.OpenConnection();
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    auto output = playerFixture.service.Handle(
        *first,
        ClientMessage{ClientHello{1U}},
        Time(0));
    EXPECT_EQ(
        PlayerId{std::numeric_limits<std::uint32_t>::max()},
        OnlyWelcome(output).player);
    output = playerFixture.service.Handle(
        *second,
        ClientMessage{ClientHello{1U}},
        Time(0));
    EXPECT_EQ(LobbyError::IdSpaceExhausted, OnlyError(output).error);

    LobbyServiceConfig roomConfig;
    roomConfig.nextRoom = std::numeric_limits<std::uint32_t>::max();
    ServiceFixture roomFixture{roomConfig};
    const ConnectionId roomHost = WelcomedConnection(roomFixture);
    const ConnectionId otherHost = WelcomedConnection(roomFixture);
    EXPECT_EQ(
        RoomId{std::numeric_limits<std::uint32_t>::max()},
        CreateRoom(roomFixture, roomHost, 2U));
    output = roomFixture.service.Handle(
        otherHost,
        ClientMessage{CreateRoomRequest{2U}},
        Time(0));
    EXPECT_EQ(LobbyError::IdSpaceExhausted, OnlyError(output).error);

    LobbyServiceConfig ordinalConfig;
    ordinalConfig.nextJoinOrdinal = std::numeric_limits<std::uint64_t>::max();
    ServiceFixture ordinalFixture{ordinalConfig};
    const ConnectionId ordinalHost = WelcomedConnection(ordinalFixture);
    const ConnectionId ordinalGuest = WelcomedConnection(ordinalFixture);
    const RoomId ordinalRoom = CreateRoom(ordinalFixture, ordinalHost, 2U);
    output = ordinalFixture.service.Handle(
        ordinalGuest,
        ClientMessage{JoinRoomRequest{2U, ordinalRoom}},
        Time(0));
    EXPECT_EQ(LobbyError::IdSpaceExhausted, OnlyError(output).error);
}

TEST(LobbyService, ReportsMissingRoomMembershipAndDeferredStart)
{
    ServiceFixture fixture;
    const ConnectionId player = WelcomedConnection(fixture);

    auto output = fixture.service.Handle(
        player,
        ClientMessage{JoinRoomRequest{2U, RoomId{404U}}},
        Time(0));
    EXPECT_EQ(LobbyError::RoomNotFound, OnlyError(output).error);

    output = fixture.service.Handle(
        player,
        ClientMessage{LeaveRoomRequest{3U}},
        Time(0));
    EXPECT_EQ(LobbyError::NotInRoom, OnlyError(output).error);

    output = fixture.service.Handle(
        player,
        ClientMessage{SetReadyRequest{4U, true}},
        Time(0));
    EXPECT_EQ(LobbyError::NotInRoom, OnlyError(output).error);

    output = fixture.service.Handle(
        player,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));
    EXPECT_EQ(LobbyError::WorkerUnavailable, OnlyError(output).error);
}

TEST(GameWorkerAllocator, ValidatesStaticEndpoint)
{
    StaticGameWorkerAllocator valid{GameEndpoint{"127.0.0.1", 41000U, 41001U}};
    const auto success = valid.Allocate(MatchId{1U}, {});
    ASSERT_TRUE(success.endpoint.has_value());
    EXPECT_EQ("127.0.0.1", success.endpoint->host);
    EXPECT_EQ(41000U, success.endpoint->tcpPort);
    EXPECT_EQ(41001U, success.endpoint->udpPort);

    StaticGameWorkerAllocator invalid{GameEndpoint{"", 41000U, 41001U}};
    const auto failure = invalid.Allocate(MatchId{1U}, {});
    EXPECT_FALSE(failure.endpoint.has_value());
    EXPECT_EQ(LobbyError::WorkerUnavailable, failure.error);
}
