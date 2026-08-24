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
#include <utility>
#include <variant>
#include <vector>

namespace
{
using dxa::lobby::ConnectionId;
using dxa::lobby::ITicketSource;
using dxa::lobby::LobbyAuditEventType;
using dxa::lobby::LobbyRuntimeAction;
using dxa::lobby::LobbyService;
using dxa::lobby::LobbyServiceConfig;
using dxa::lobby::LobbyServiceResult;
using dxa::lobby::MatchFinishedEvent;
using dxa::lobby::MatchUnavailableEvent;
using dxa::lobby::MatchTicketRegistry;
using dxa::lobby::ReservationFailedEvent;
using dxa::lobby::ReservationReadyEvent;
using dxa::lobby::ReserveMatchAction;
using dxa::lobby::CancelReservationAction;
using dxa::lobby::TicketConsumeResult;
using dxa::protocol::ClientHello;
using dxa::protocol::ClientMessage;
using dxa::protocol::CreateRoomRequest;
using dxa::protocol::ErrorResponse;
using dxa::protocol::GameEndpoint;
using dxa::protocol::JoinRoomRequest;
using dxa::protocol::LeaveRoomRequest;
using dxa::protocol::ListRoomsRequest;
using dxa::protocol::LobbyError;
using dxa::protocol::MatchId;
using dxa::protocol::MatchTicket;
using dxa::protocol::MatchTicketValue;
using dxa::protocol::PlayerId;
using dxa::protocol::ReservationId;
using dxa::protocol::RoomId;
using dxa::protocol::RoomListResponse;
using dxa::protocol::RoomSnapshot;
using dxa::protocol::RoomState;
using dxa::protocol::ServerWelcome;
using dxa::protocol::SetReadyRequest;
using dxa::protocol::StartMatchRequest;
using dxa::protocol::WorkerId;

[[nodiscard]] std::chrono::steady_clock::time_point Time(
    const std::int64_t seconds)
{
    return std::chrono::steady_clock::time_point{
        std::chrono::seconds{seconds}};
}

class SequenceTicketSource final : public ITicketSource
{
public:
    explicit SequenceTicketSource(
        const std::optional<std::size_t> failAtCall = std::nullopt)
        : failAtCall_{failAtCall}
    {
    }

    [[nodiscard]] bool Fill(
        const std::span<std::byte, dxa::protocol::MatchTicketBytes> output) noexcept override
    {
        const std::size_t call = calls_++;
        if (failAtCall_.has_value() && call == *failAtCall_)
        {
            return false;
        }

        MatchTicketValue ticket{};
        for (std::size_t index = 0; index < ticket.size(); ++index)
        {
            ticket[index] = static_cast<std::byte>(
                static_cast<std::uint8_t>(call + index + 1U));
        }
        std::copy(ticket.begin(), ticket.end(), output.begin());
        generated.push_back(ticket);
        return true;
    }

    std::vector<MatchTicketValue> generated;

private:
    std::optional<std::size_t> failAtCall_;
    std::size_t calls_ = 0;
};

struct ServiceFixture
{
    explicit ServiceFixture(
        const LobbyServiceConfig config = {},
        const std::optional<std::size_t> failAtTicketCall = std::nullopt)
        : ticketSource{failAtTicketCall},
          tickets{ticketSource},
          service{tickets, config}
    {
    }

    SequenceTicketSource ticketSource;
    MatchTicketRegistry tickets;
    LobbyService service;
};

struct ReadyRoom
{
    ConnectionId host;
    ConnectionId guest;
    RoomId room;
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
        if (candidate == nullptr)
        {
            continue;
        }
        if (found != nullptr)
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

template <typename Action>
[[nodiscard]] const Action& OnlyAction(const LobbyServiceResult& result)
{
    if (result.actions.size() != 1U)
    {
        throw std::logic_error{"expected exactly one runtime action"};
    }
    const auto* action = std::get_if<Action>(&result.actions.front());
    if (action == nullptr)
    {
        throw std::logic_error{"unexpected runtime action type"};
    }
    return *action;
}

template <typename Message>
[[nodiscard]] std::size_t CountMessages(const LobbyServiceResult& result)
{
    return static_cast<std::size_t>(std::count_if(
        result.outbound.begin(),
        result.outbound.end(),
        [](const auto& outbound) {
            return std::holds_alternative<Message>(outbound.message);
        }));
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

[[nodiscard]] ReadyRoom ReadyTwoPlayerRoom(ServiceFixture& fixture)
{
    const ConnectionId host = WelcomedConnection(fixture);
    const ConnectionId guest = WelcomedConnection(fixture);
    const RoomId room = CreateRoom(fixture, host, 2U);
    JoinRoom(fixture, guest, 2U, room);

    static_cast<void>(fixture.service.Handle(
        host,
        ClientMessage{ListRoomsRequest{3U}},
        Time(0)));
    static_cast<void>(fixture.service.Handle(
        host,
        ClientMessage{SetReadyRequest{4U, true}},
        Time(0)));
    static_cast<void>(fixture.service.Handle(
        guest,
        ClientMessage{SetReadyRequest{3U, true}},
        Time(0)));
    return {host, guest, room};
}

[[nodiscard]] const RoomSnapshot& SnapshotFor(
    const LobbyServiceResult& result,
    const ConnectionId recipient)
{
    return MessageFor<RoomSnapshot>(result, recipient);
}

[[nodiscard]] const ErrorResponse& ErrorFor(
    const LobbyServiceResult& result,
    const ConnectionId recipient)
{
    return MessageFor<ErrorResponse>(result, recipient);
}

[[nodiscard]] const MatchTicket& TicketFor(
    const LobbyServiceResult& result,
    const ConnectionId recipient)
{
    return MessageFor<MatchTicket>(result, recipient);
}

[[nodiscard]] const dxa::protocol::RoomMemberView& Member(
    const RoomSnapshot& snapshot,
    const PlayerId player)
{
    const auto member = std::find_if(
        snapshot.members.begin(),
        snapshot.members.end(),
        [player](const auto& candidate) { return candidate.player == player; });
    if (member == snapshot.members.end())
    {
        throw std::logic_error{"room member not found"};
    }
    return *member;
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

TEST(LobbyService, ReportsMissingRoomMembership)
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
    EXPECT_EQ(LobbyError::NotInRoom, OnlyError(output).error);
}

TEST(LobbyService, ValidatesHostPlayerCountAndReadyStateBeforeStarting)
{
    ServiceFixture fixture;
    const ConnectionId host = WelcomedConnection(fixture);
    const RoomId room = CreateRoom(fixture, host, 2U);

    auto output = fixture.service.Handle(
        host,
        ClientMessage{StartMatchRequest{3U}},
        Time(0));
    EXPECT_EQ(LobbyError::MinimumPlayersRequired, OnlyError(output).error);

    const ConnectionId guest = WelcomedConnection(fixture);
    JoinRoom(fixture, guest, 2U, room);
    output = fixture.service.Handle(
        guest,
        ClientMessage{StartMatchRequest{3U}},
        Time(0));
    EXPECT_EQ(LobbyError::NotHost, OnlyError(output).error);

    output = fixture.service.Handle(
        host,
        ClientMessage{StartMatchRequest{4U}},
        Time(0));
    EXPECT_EQ(LobbyError::NotAllReady, OnlyError(output).error);
    EXPECT_TRUE(fixture.ticketSource.generated.empty());
}

TEST(LobbyService, StartsReservationWithoutPublishingTickets)
{
    ServiceFixture fixture;
    const ReadyRoom ready = ReadyTwoPlayerRoom(fixture);

    const auto output = fixture.service.Handle(
        ready.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));

    EXPECT_EQ(RoomState::Starting, SnapshotFor(output, ready.host).state);
    EXPECT_EQ(RoomState::Starting, SnapshotFor(output, ready.guest).state);
    EXPECT_EQ(5U, SnapshotFor(output, ready.host).requestId);
    EXPECT_EQ(0U, SnapshotFor(output, ready.guest).requestId);
    EXPECT_EQ(0U, CountMessages<MatchTicket>(output));
    EXPECT_TRUE(output.audit.empty());

    const auto& reservation = OnlyAction<ReserveMatchAction>(output);
    EXPECT_EQ(ReservationId{1U}, reservation.reservation);
    EXPECT_EQ(ready.room, reservation.room);
    EXPECT_EQ(MatchId{1U}, reservation.match);
    EXPECT_EQ(PlayerId{1U}, reservation.requester);
    EXPECT_EQ(5U, reservation.requestId);
    EXPECT_EQ(20260825U, reservation.seed);
    EXPECT_EQ(Time(0), reservation.issuedAt);
    ASSERT_EQ(2U, reservation.participants.size());
    EXPECT_EQ(PlayerId{1U}, reservation.participants[0].player);
    EXPECT_EQ(PlayerId{2U}, reservation.participants[1].player);
    EXPECT_TRUE(
        reservation.participants[0].ticket
        != reservation.participants[1].ticket);
}

TEST(LobbyService, PublishesTicketsOnlyAfterMatchingReadyEvent)
{
    ServiceFixture fixture;
    const ReadyRoom ready = ReadyTwoPlayerRoom(fixture);
    const auto start = fixture.service.Handle(
        ready.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));
    const ReserveMatchAction reservation =
        OnlyAction<ReserveMatchAction>(start);

    const auto output = fixture.service.HandleWorkerEvent(
        ReservationReadyEvent{
            reservation.reservation,
            reservation.match,
            WorkerId{3U},
            GameEndpoint{"127.0.0.1", 41000U, 41001U}},
        Time(1));

    EXPECT_EQ(RoomState::InMatch, SnapshotFor(output, ready.host).state);
    EXPECT_EQ(RoomState::InMatch, SnapshotFor(output, ready.guest).state);
    EXPECT_EQ(5U, SnapshotFor(output, ready.host).requestId);
    EXPECT_EQ(0U, SnapshotFor(output, ready.guest).requestId);
    EXPECT_EQ(2U, CountMessages<MatchTicket>(output));

    const auto& hostTicket = TicketFor(output, ready.host);
    const auto& guestTicket = TicketFor(output, ready.guest);
    EXPECT_TRUE(hostTicket.ticket != guestTicket.ticket);
    EXPECT_EQ(5U, hostTicket.requestId);
    EXPECT_EQ(0U, guestTicket.requestId);
    EXPECT_EQ(MatchId{1U}, hostTicket.match);
    EXPECT_EQ("127.0.0.1", hostTicket.host);
    EXPECT_EQ(41000U, hostTicket.tcpPort);
    EXPECT_EQ(41001U, hostTicket.udpPort);
    EXPECT_EQ(59U, hostTicket.expiresInSeconds);

    ASSERT_EQ(1U, output.audit.size());
    EXPECT_EQ(LobbyAuditEventType::MatchStarted, output.audit.front().type);
    EXPECT_EQ(MatchId{1U}, output.audit.front().match);
    EXPECT_EQ(GameEndpoint({"127.0.0.1", 41000U, 41001U}),
              output.audit.front().endpoint);

    const auto roomList = fixture.service.Handle(
        ready.guest,
        ClientMessage{ListRoomsRequest{4U}},
        Time(1));
    EXPECT_TRUE(OnlyMessage<RoomListResponse>(roomList).rooms.empty());
}

TEST(LobbyService, ReservationFailureReturnsWaitingAndPreservesReadyState)
{
    ServiceFixture fixture;
    const ReadyRoom ready = ReadyTwoPlayerRoom(fixture);
    const auto start = fixture.service.Handle(
        ready.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));
    const ReserveMatchAction reservation =
        OnlyAction<ReserveMatchAction>(start);

    const auto output = fixture.service.HandleWorkerEvent(
        ReservationFailedEvent{
            reservation.reservation,
            reservation.match},
        Time(1));

    EXPECT_EQ(RoomState::Waiting, SnapshotFor(output, ready.host).state);
    EXPECT_EQ(RoomState::Waiting, SnapshotFor(output, ready.guest).state);
    EXPECT_EQ(0U, SnapshotFor(output, ready.host).requestId);
    EXPECT_EQ(0U, SnapshotFor(output, ready.guest).requestId);
    EXPECT_TRUE(Member(SnapshotFor(output, ready.host), PlayerId{1U}).ready);
    EXPECT_TRUE(Member(SnapshotFor(output, ready.guest), PlayerId{2U}).ready);
    EXPECT_EQ(LobbyError::WorkerUnavailable, ErrorFor(output, ready.host).error);
    ASSERT_EQ(2U, fixture.ticketSource.generated.size());
    for (std::size_t index = 0U;
         index < fixture.ticketSource.generated.size();
         ++index)
    {
        EXPECT_EQ(
            TicketConsumeResult::NotFound,
            fixture.tickets.Consume(
                fixture.ticketSource.generated[index],
                reservation.match,
                PlayerId{static_cast<std::uint32_t>(index + 1U)},
                Time(1)));
    }
    ASSERT_EQ(1U, output.audit.size());
    EXPECT_EQ(LobbyAuditEventType::StartFailed, output.audit.front().type);
    EXPECT_EQ(reservation.match, output.audit.front().match);
    EXPECT_EQ(LobbyError::WorkerUnavailable, output.audit.front().error);
}

TEST(LobbyService, IgnoresMismatchedAndLateWorkerEvents)
{
    ServiceFixture fixture;
    const ReadyRoom ready = ReadyTwoPlayerRoom(fixture);
    const auto start = fixture.service.Handle(
        ready.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));
    const ReserveMatchAction reservation =
        OnlyAction<ReserveMatchAction>(start);

    const auto wrong = fixture.service.HandleWorkerEvent(
        ReservationReadyEvent{
            ReservationId{reservation.reservation.value + 1U},
            reservation.match,
            WorkerId{3U},
            GameEndpoint{"127.0.0.1", 41000U, 41001U}},
        Time(1));
    EXPECT_TRUE(wrong.outbound.empty());
    EXPECT_TRUE(wrong.actions.empty());

    const auto failed = fixture.service.HandleWorkerEvent(
        ReservationFailedEvent{
            reservation.reservation,
            reservation.match},
        Time(1));
    EXPECT_EQ(RoomState::Waiting, SnapshotFor(failed, ready.host).state);

    const auto late = fixture.service.HandleWorkerEvent(
        ReservationReadyEvent{
            reservation.reservation,
            reservation.match,
            WorkerId{3U},
            GameEndpoint{"127.0.0.1", 41000U, 41001U}},
        Time(2));
    EXPECT_TRUE(late.outbound.empty());
    EXPECT_TRUE(late.actions.empty());
}

TEST(LobbyService, TicketSourceFailureRevokesPartialIssueAndRollsBack)
{
    ServiceFixture fixture{LobbyServiceConfig{}, 1U};
    const ReadyRoom ready = ReadyTwoPlayerRoom(fixture);

    const auto output = fixture.service.Handle(
        ready.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));

    EXPECT_EQ(RoomState::Waiting, SnapshotFor(output, ready.host).state);
    EXPECT_EQ(LobbyError::InternalError, ErrorFor(output, ready.host).error);
    EXPECT_TRUE(output.actions.empty());
    ASSERT_EQ(1U, fixture.ticketSource.generated.size());
    EXPECT_EQ(
        TicketConsumeResult::NotFound,
        fixture.tickets.Consume(
            fixture.ticketSource.generated.front(),
            MatchId{1U},
            PlayerId{1U},
            Time(0)));
}

TEST(LobbyService, InvalidOrExpiredReadyRollsBackReservation)
{
    ServiceFixture invalidFixture;
    const ReadyRoom invalidReady = ReadyTwoPlayerRoom(invalidFixture);
    const auto invalidStart = invalidFixture.service.Handle(
        invalidReady.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));
    const ReserveMatchAction invalidReservation =
        OnlyAction<ReserveMatchAction>(invalidStart);
    auto output = invalidFixture.service.HandleWorkerEvent(
        ReservationReadyEvent{
            invalidReservation.reservation,
            invalidReservation.match,
            WorkerId{3U},
            GameEndpoint{"", 41000U, 41001U}},
        Time(1));
    EXPECT_EQ(RoomState::Waiting,
              SnapshotFor(output, invalidReady.host).state);
    EXPECT_EQ(LobbyError::WorkerUnavailable,
              ErrorFor(output, invalidReady.host).error);

    ServiceFixture expiredFixture;
    const ReadyRoom expiredReady = ReadyTwoPlayerRoom(expiredFixture);
    const auto expiredStart = expiredFixture.service.Handle(
        expiredReady.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));
    const ReserveMatchAction expiredReservation =
        OnlyAction<ReserveMatchAction>(expiredStart);
    output = expiredFixture.service.HandleWorkerEvent(
        ReservationReadyEvent{
            expiredReservation.reservation,
            expiredReservation.match,
            WorkerId{3U},
            GameEndpoint{"127.0.0.1", 41000U, 41001U}},
        Time(60));
    EXPECT_EQ(RoomState::Waiting,
              SnapshotFor(output, expiredReady.host).state);
    EXPECT_EQ(LobbyError::WorkerUnavailable,
              ErrorFor(output, expiredReady.host).error);
}

TEST(LobbyService, StartingDisconnectCancelsThenRemovesParticipant)
{
    ServiceFixture fixture;
    const ReadyRoom ready = ReadyTwoPlayerRoom(fixture);
    const auto start = fixture.service.Handle(
        ready.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));
    const ReserveMatchAction reservation =
        OnlyAction<ReserveMatchAction>(start);

    auto rejected = fixture.service.Handle(
        ready.guest,
        ClientMessage{LeaveRoomRequest{4U}},
        Time(0));
    EXPECT_EQ(LobbyError::RoomNotJoinable, OnlyError(rejected).error);
    rejected = fixture.service.Handle(
        ready.host,
        ClientMessage{SetReadyRequest{6U, false}},
        Time(0));
    EXPECT_EQ(LobbyError::RoomNotJoinable, OnlyError(rejected).error);

    const auto output = fixture.service.Disconnect(ready.guest);

    const auto& cancel = OnlyAction<CancelReservationAction>(output);
    EXPECT_EQ(reservation.reservation, cancel.reservation);
    EXPECT_EQ(reservation.match, cancel.match);
    const auto& snapshot = SnapshotFor(output, ready.host);
    EXPECT_EQ(RoomState::Waiting, snapshot.state);
    ASSERT_EQ(1U, snapshot.members.size());
    EXPECT_EQ(PlayerId{1U}, snapshot.members.front().player);
    EXPECT_TRUE(snapshot.members.front().ready);
    EXPECT_EQ(LobbyError::WorkerUnavailable,
              ErrorFor(output, ready.host).error);
}

TEST(LobbyService, StartingHostDisconnectTransfersHostWithoutStaleError)
{
    ServiceFixture fixture;
    const ReadyRoom ready = ReadyTwoPlayerRoom(fixture);
    const auto start = fixture.service.Handle(
        ready.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));
    const ReserveMatchAction reservation =
        OnlyAction<ReserveMatchAction>(start);

    const auto output = fixture.service.Disconnect(ready.host);

    const auto& cancel = OnlyAction<CancelReservationAction>(output);
    EXPECT_EQ(reservation.reservation, cancel.reservation);
    const auto& snapshot = SnapshotFor(output, ready.guest);
    EXPECT_EQ(RoomState::Waiting, snapshot.state);
    EXPECT_EQ(PlayerId{2U}, snapshot.host);
    ASSERT_EQ(1U, snapshot.members.size());
    EXPECT_EQ(0U, CountMessages<ErrorResponse>(output));
    EXPECT_TRUE(std::any_of(
        output.audit.begin(),
        output.audit.end(),
        [](const auto& event) {
            return event.type == LobbyAuditEventType::HostTransferred;
        }));
}

TEST(LobbyService, InMatchLobbyDisconnectWaitsForWorkerCleanup)
{
    ServiceFixture fixture;
    const ReadyRoom ready = ReadyTwoPlayerRoom(fixture);
    const auto start = fixture.service.Handle(
        ready.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));
    const ReserveMatchAction reservation =
        OnlyAction<ReserveMatchAction>(start);
    static_cast<void>(fixture.service.HandleWorkerEvent(
        ReservationReadyEvent{
            reservation.reservation,
            reservation.match,
            WorkerId{3U},
            GameEndpoint{"127.0.0.1", 41000U, 41001U}},
        Time(1)));

    const auto disconnected = fixture.service.Disconnect(ready.guest);
    EXPECT_TRUE(disconnected.outbound.empty());
    EXPECT_TRUE(fixture.service.HasRoom(ready.room));

    const auto finished = fixture.service.HandleWorkerEvent(
        MatchFinishedEvent{WorkerId{3U}, reservation.match},
        Time(2));
    EXPECT_FALSE(fixture.service.HasRoom(ready.room));
    const auto& rooms = OnlyMessage<RoomListResponse>(finished);
    EXPECT_EQ(0U, rooms.requestId);
    EXPECT_TRUE(rooms.rooms.empty());
    for (std::size_t index = 0U;
         index < fixture.ticketSource.generated.size();
         ++index)
    {
        EXPECT_EQ(
            TicketConsumeResult::NotFound,
            fixture.tickets.Consume(
                fixture.ticketSource.generated[index],
                reservation.match,
                PlayerId{static_cast<std::uint32_t>(index + 1U)},
                Time(2)));
    }
}

TEST(LobbyService, ActiveWorkerLossPublishesErrorAndRoomList)
{
    ServiceFixture fixture;
    const ReadyRoom ready = ReadyTwoPlayerRoom(fixture);
    const auto start = fixture.service.Handle(
        ready.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));
    const ReserveMatchAction reservation =
        OnlyAction<ReserveMatchAction>(start);
    static_cast<void>(fixture.service.HandleWorkerEvent(
        ReservationReadyEvent{
            reservation.reservation,
            reservation.match,
            WorkerId{3U},
            GameEndpoint{"127.0.0.1", 41000U, 41001U}},
        Time(1)));

    const auto wrongWorker = fixture.service.HandleWorkerEvent(
        MatchUnavailableEvent{WorkerId{4U}, reservation.match},
        Time(2));
    EXPECT_TRUE(wrongWorker.outbound.empty());

    const auto output = fixture.service.HandleWorkerEvent(
        MatchUnavailableEvent{WorkerId{3U}, reservation.match},
        Time(2));

    EXPECT_FALSE(fixture.service.HasRoom(ready.room));
    EXPECT_EQ(LobbyError::MatchUnavailable,
              ErrorFor(output, ready.host).error);
    EXPECT_EQ(LobbyError::MatchUnavailable,
              ErrorFor(output, ready.guest).error);
    EXPECT_TRUE(MessageFor<RoomListResponse>(output, ready.host).rooms.empty());
    EXPECT_TRUE(MessageFor<RoomListResponse>(output, ready.guest).rooms.empty());
}

TEST(LobbyService, ConsumesMaximumMatchAndReservationIdsWithoutWrapping)
{
    LobbyServiceConfig config;
    config.nextMatch = std::numeric_limits<std::uint64_t>::max();
    config.nextReservation = std::numeric_limits<std::uint64_t>::max();
    ServiceFixture fixture{config};
    const ReadyRoom ready = ReadyTwoPlayerRoom(fixture);

    auto output = fixture.service.Handle(
        ready.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));
    const ReserveMatchAction reservation =
        OnlyAction<ReserveMatchAction>(output);
    EXPECT_EQ(MatchId{std::numeric_limits<std::uint64_t>::max()},
              reservation.match);
    EXPECT_EQ(ReservationId{std::numeric_limits<std::uint64_t>::max()},
              reservation.reservation);

    static_cast<void>(fixture.service.HandleWorkerEvent(
        ReservationFailedEvent{reservation.reservation, reservation.match},
        Time(1)));
    output = fixture.service.Handle(
        ready.host,
        ClientMessage{StartMatchRequest{6U}},
        Time(1));
    EXPECT_EQ(LobbyError::IdSpaceExhausted, OnlyError(output).error);

    LobbyServiceConfig noReservation;
    noReservation.nextReservation = 0U;
    ServiceFixture exhausted{noReservation};
    const ReadyRoom exhaustedReady = ReadyTwoPlayerRoom(exhausted);
    output = exhausted.service.Handle(
        exhaustedReady.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));
    EXPECT_EQ(LobbyError::IdSpaceExhausted, OnlyError(output).error);
    EXPECT_TRUE(exhausted.ticketSource.generated.empty());
}
