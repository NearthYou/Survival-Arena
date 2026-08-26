#pragma once

#include <dxa/lobby/ConnectionId.hpp>
#include <dxa/lobby/LobbyRuntimeTypes.hpp>
#include <dxa/lobby/MatchTicketRegistry.hpp>
#include <dxa/lobby/Room.hpp>

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace dxa::lobby
{
struct OutboundMessage
{
    template <typename Message>
        requires std::constructible_from<
            dxa::protocol::ServerMessage,
            Message&&>
    OutboundMessage(ConnectionId target, Message&& value)
        : recipient{target},
          message{std::forward<Message>(value)}
    {
    }

    ConnectionId recipient;
    dxa::protocol::ServerMessage message;
};

enum class LobbyAuditEventType
{
    PlayerAssigned,
    RoomCreated,
    RoomDeleted,
    HostTransferred,
    MatchStarted,
    StartFailed
};

struct LobbyAuditEvent
{
    LobbyAuditEventType type = LobbyAuditEventType::PlayerAssigned;
    std::optional<ConnectionId> connection;
    std::optional<dxa::protocol::PlayerId> player;
    std::optional<dxa::protocol::RoomId> room;
    std::optional<dxa::protocol::MatchId> match;
    std::optional<dxa::protocol::LobbyError> error;
    std::optional<dxa::protocol::GameEndpoint> endpoint;
};

struct LobbyServiceResult
{
    std::vector<OutboundMessage> outbound;
    std::vector<LobbyAuditEvent> audit;
    std::vector<LobbyRuntimeAction> actions;
};

struct LobbyServiceConfig
{
    std::uint64_t nextConnection = 1U;
    std::uint32_t nextPlayer = 1U;
    std::uint32_t nextRoom = 1U;
    std::uint64_t nextMatch = 1U;
    std::uint64_t nextReservation = 1U;
    std::uint64_t nextJoinOrdinal = 1U;
    std::uint32_t matchSeedBase = 20260824U;
    std::uint32_t maximumRooms = static_cast<std::uint32_t>(
        dxa::protocol::MaximumRooms);
};

class LobbyService
{
public:
    explicit LobbyService(
        MatchTicketRegistry& tickets,
        LobbyServiceConfig config = {});

    [[nodiscard]] std::optional<ConnectionId> OpenConnection();
    [[nodiscard]] LobbyServiceResult Handle(
        ConnectionId connection,
        const dxa::protocol::ClientMessage& message,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] LobbyServiceResult Disconnect(ConnectionId connection);
    [[nodiscard]] LobbyServiceResult HandleWorkerEvent(
        const WorkerEvent& event,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] MatchTicketRegistry& Tickets() noexcept;
    [[nodiscard]] std::size_t RoomCount() const noexcept;
    [[nodiscard]] bool HasRoom(dxa::protocol::RoomId room) const noexcept;

private:
    struct ConnectionState
    {
        std::optional<dxa::protocol::PlayerId> player;
        std::uint32_t lastRequestId = 0;
    };

    [[nodiscard]] LobbyServiceResult HandleHello(
        ConnectionId connection,
        ConnectionState& state,
        const dxa::protocol::ClientHello& request);
    [[nodiscard]] LobbyServiceResult HandleListRooms(
        ConnectionId connection,
        const dxa::protocol::ListRoomsRequest& request) const;
    [[nodiscard]] LobbyServiceResult HandleCreateRoom(
        ConnectionId connection,
        dxa::protocol::PlayerId player,
        const dxa::protocol::CreateRoomRequest& request);
    [[nodiscard]] LobbyServiceResult HandleJoinRoom(
        ConnectionId connection,
        dxa::protocol::PlayerId player,
        const dxa::protocol::JoinRoomRequest& request);
    [[nodiscard]] LobbyServiceResult HandleLeaveRoom(
        ConnectionId connection,
        dxa::protocol::PlayerId player,
        const dxa::protocol::LeaveRoomRequest& request);
    [[nodiscard]] LobbyServiceResult HandleSetReady(
        ConnectionId connection,
        dxa::protocol::PlayerId player,
        const dxa::protocol::SetReadyRequest& request);
    [[nodiscard]] LobbyServiceResult HandleStartMatch(
        ConnectionId connection,
        dxa::protocol::PlayerId player,
        const dxa::protocol::StartMatchRequest& request,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] LobbyServiceResult LeaveWaitingRoom(
        dxa::protocol::PlayerId player,
        std::optional<ConnectionId> requester,
        std::uint32_t requestId);
    [[nodiscard]] LobbyServiceResult Error(
        ConnectionId connection,
        std::uint32_t requestId,
        dxa::protocol::LobbyError error) const;
    [[nodiscard]] dxa::protocol::RoomListResponse RoomList(
        std::uint32_t requestId) const;
    void BroadcastSnapshot(
        LobbyServiceResult& result,
        const Room& room,
        std::optional<ConnectionId> requester,
        std::uint32_t requestId) const;

    struct PendingReservation
    {
        ReserveMatchAction action;
    };

    struct ActiveMatch
    {
        dxa::protocol::RoomId room;
        dxa::protocol::WorkerId worker;
        std::vector<dxa::protocol::MatchTicketValue> tickets;
    };

    [[nodiscard]] LobbyServiceResult FailPendingReservation(
        dxa::protocol::ReservationId reservation,
        dxa::protocol::LobbyError error);
    [[nodiscard]] LobbyServiceResult FinishActiveMatch(
        dxa::protocol::WorkerId worker,
        dxa::protocol::MatchId match,
        bool unavailable);
    [[nodiscard]] LobbyServiceResult DisconnectStartingPlayer(
        dxa::protocol::PlayerId player,
        dxa::protocol::RoomId room);

    MatchTicketRegistry& tickets_;
    std::optional<std::uint64_t> nextConnection_;
    std::optional<std::uint32_t> nextPlayer_;
    std::optional<std::uint32_t> nextRoom_;
    std::optional<std::uint64_t> nextMatch_;
    std::optional<std::uint64_t> nextReservation_;
    std::optional<std::uint64_t> nextJoinOrdinal_;
    std::uint32_t matchSeedBase_ = 20260824U;
    std::size_t maximumRooms_ = dxa::protocol::MaximumRooms;
    std::map<ConnectionId, ConnectionState> connections_;
    std::map<dxa::protocol::PlayerId, ConnectionId> playerToConnection_;
    std::map<dxa::protocol::PlayerId, dxa::protocol::RoomId> playerToRoom_;
    std::map<dxa::protocol::RoomId, Room> rooms_;
    std::map<dxa::protocol::ReservationId, PendingReservation>
        pendingReservations_;
    std::map<dxa::protocol::RoomId, dxa::protocol::ReservationId>
        roomToReservation_;
    std::map<dxa::protocol::MatchId, ActiveMatch> activeMatches_;
};
} // namespace dxa::lobby
