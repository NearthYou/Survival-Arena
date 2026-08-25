#include <dxa/lobby/LobbyService.hpp>

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>
#include <variant>

namespace dxa::lobby
{
namespace
{
using dxa::protocol::ClientHello;
using dxa::protocol::CreateRoomRequest;
using dxa::protocol::ErrorResponse;
using dxa::protocol::GameEndpoint;
using dxa::protocol::JoinRoomRequest;
using dxa::protocol::LeaveRoomRequest;
using dxa::protocol::ListRoomsRequest;
using dxa::protocol::LobbyError;
using dxa::protocol::MatchTicketValue;
using dxa::protocol::PlayerId;
using dxa::protocol::ReservationId;
using dxa::protocol::ReservedParticipant;
using dxa::protocol::RoomId;
using dxa::protocol::RoomListResponse;
using dxa::protocol::RoomState;
using dxa::protocol::SetReadyRequest;
using dxa::protocol::StartMatchRequest;
using dxa::protocol::WorkerId;

template <typename Value>
[[nodiscard]] std::optional<Value> InitialCounter(const Value value) noexcept
{
    return value == 0 ? std::nullopt : std::optional<Value>{value};
}

template <typename Value>
[[nodiscard]] std::optional<Value> TakeNext(
    std::optional<Value>& counter) noexcept
{
    if (!counter.has_value())
    {
        return std::nullopt;
    }

    const Value value = *counter;
    if (value == std::numeric_limits<Value>::max())
    {
        counter.reset();
    }
    else
    {
        *counter = static_cast<Value>(value + 1);
    }
    return value;
}

[[nodiscard]] LobbyAuditEvent Audit(
    const LobbyAuditEventType type,
    const std::optional<ConnectionId> connection = std::nullopt,
    const std::optional<PlayerId> player = std::nullopt,
    const std::optional<RoomId> room = std::nullopt)
{
    LobbyAuditEvent event;
    event.type = type;
    event.connection = connection;
    event.player = player;
    event.room = room;
    return event;
}

[[nodiscard]] bool IsValidEndpoint(const GameEndpoint& endpoint) noexcept
{
    if (endpoint.host.empty()
        || endpoint.host.size() > 255U
        || endpoint.tcpPort == 0U
        || endpoint.udpPort == 0U)
    {
        return false;
    }
    return std::all_of(
        endpoint.host.begin(),
        endpoint.host.end(),
        [](const char character) {
            const auto value = static_cast<unsigned char>(character);
            return value >= 0x21U && value <= 0x7EU;
        });
}

} // namespace

LobbyService::LobbyService(
    MatchTicketRegistry& tickets,
    const LobbyServiceConfig config)
    : tickets_{tickets},
      nextConnection_{InitialCounter(config.nextConnection)},
      nextPlayer_{InitialCounter(config.nextPlayer)},
      nextRoom_{InitialCounter(config.nextRoom)},
      nextMatch_{InitialCounter(config.nextMatch)},
      nextReservation_{InitialCounter(config.nextReservation)},
      nextJoinOrdinal_{InitialCounter(config.nextJoinOrdinal)},
      matchSeedBase_{config.matchSeedBase},
      maximumRooms_{std::min<std::size_t>(
          config.maximumRooms,
          dxa::protocol::MaximumRooms)}
{
}

std::optional<ConnectionId> LobbyService::OpenConnection()
{
    const auto value = TakeNext(nextConnection_);
    if (!value.has_value())
    {
        return std::nullopt;
    }

    const ConnectionId connection{*value};
    connections_.emplace(connection, ConnectionState{});
    return connection;
}

LobbyServiceResult LobbyService::Handle(
    const ConnectionId connection,
    const dxa::protocol::ClientMessage& message,
    const std::chrono::steady_clock::time_point now)
{
    const auto connectionIt = connections_.find(connection);
    if (connectionIt == connections_.end())
    {
        return {};
    }

    ConnectionState& state = connectionIt->second;
    const std::uint32_t requestId = dxa::protocol::RequestId(message);
    if (requestId == 0U || requestId <= state.lastRequestId)
    {
        return Error(connection, requestId, LobbyError::RequestOutOfOrder);
    }
    state.lastRequestId = requestId;

    if (const auto* hello = std::get_if<ClientHello>(&message))
    {
        return HandleHello(connection, state, *hello);
    }
    if (!state.player.has_value())
    {
        return Error(connection, requestId, LobbyError::NotWelcomed);
    }

    const PlayerId player = *state.player;
    return std::visit(
        [this, connection, player, now](const auto& request) -> LobbyServiceResult {
            using Request = std::decay_t<decltype(request)>;
            if constexpr (std::is_same_v<Request, ClientHello>)
            {
                return Error(
                    connection,
                    request.requestId,
                    LobbyError::AlreadyWelcomed);
            }
            else if constexpr (std::is_same_v<Request, ListRoomsRequest>)
            {
                return HandleListRooms(connection, request);
            }
            else if constexpr (std::is_same_v<Request, CreateRoomRequest>)
            {
                return HandleCreateRoom(connection, player, request);
            }
            else if constexpr (std::is_same_v<Request, JoinRoomRequest>)
            {
                return HandleJoinRoom(connection, player, request);
            }
            else if constexpr (std::is_same_v<Request, LeaveRoomRequest>)
            {
                return HandleLeaveRoom(connection, player, request);
            }
            else if constexpr (std::is_same_v<Request, SetReadyRequest>)
            {
                return HandleSetReady(connection, player, request);
            }
            else if constexpr (std::is_same_v<Request, StartMatchRequest>)
            {
                return HandleStartMatch(connection, player, request, now);
            }
        },
        message);
}

LobbyServiceResult LobbyService::Disconnect(const ConnectionId connection)
{
    const auto connectionIt = connections_.find(connection);
    if (connectionIt == connections_.end())
    {
        return {};
    }

    LobbyServiceResult result;
    if (connectionIt->second.player.has_value())
    {
        const PlayerId player = *connectionIt->second.player;
        const auto membership = playerToRoom_.find(player);
        if (membership != playerToRoom_.end())
        {
            const auto room = rooms_.find(membership->second);
            if (room != rooms_.end())
            {
                const RoomState state = room->second.Snapshot(0U).state;
                if (state == RoomState::Starting)
                {
                    result = DisconnectStartingPlayer(player, membership->second);
                }
                else if (state == RoomState::Waiting)
                {
                    result = LeaveWaitingRoom(player, std::nullopt, 0U);
                }
            }
        }
        playerToConnection_.erase(player);
    }
    connections_.erase(connectionIt);
    return result;
}

LobbyServiceResult LobbyService::HandleWorkerEvent(
    const WorkerEvent& event,
    const std::chrono::steady_clock::time_point now)
{
    return std::visit(
        [this, now](const auto& value) -> LobbyServiceResult {
            using Event = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Event, ReservationFailedEvent>)
            {
                const auto pending = pendingReservations_.find(value.reservation);
                if (pending == pendingReservations_.end()
                    || pending->second.action.match != value.match)
                {
                    return {};
                }
                return FailPendingReservation(
                    value.reservation,
                    LobbyError::WorkerUnavailable);
            }
            else if constexpr (std::is_same_v<Event, ReservationReadyEvent>)
            {
                const auto pending = pendingReservations_.find(value.reservation);
                if (pending == pendingReservations_.end()
                    || pending->second.action.match != value.match)
                {
                    return {};
                }
                const auto expiresAt = pending->second.action.issuedAt
                    + std::chrono::seconds{
                        static_cast<std::chrono::seconds::rep>(
                            dxa::protocol::MatchTicketLifetimeSeconds)};
                const auto remaining = std::chrono::duration_cast<
                    std::chrono::seconds>(expiresAt - now).count();
                if (value.worker.value == 0U
                    || !IsValidEndpoint(value.endpoint)
                    || remaining < 1)
                {
                    return FailPendingReservation(
                        value.reservation,
                        LobbyError::WorkerUnavailable);
                }

                ReserveMatchAction action = pending->second.action;
                pendingReservations_.erase(pending);
                roomToReservation_.erase(action.room);
                const auto room = rooms_.find(action.room);
                if (room == rooms_.end()
                    || room->second.Snapshot(0U).state != RoomState::Starting)
                {
                    std::vector<MatchTicketValue> issued;
                    issued.reserve(action.participants.size());
                    for (const ReservedParticipant& participant : action.participants)
                    {
                        issued.push_back(participant.ticket);
                    }
                    tickets_.Revoke(issued);
                    return {};
                }

                room->second.MarkInMatch();
                ActiveMatch active;
                active.room = action.room;
                active.worker = value.worker;
                active.tickets.reserve(action.participants.size());
                for (const ReservedParticipant& participant : action.participants)
                {
                    active.tickets.push_back(participant.ticket);
                }
                activeMatches_.emplace(action.match, std::move(active));

                LobbyServiceResult result;
                const auto requester = playerToConnection_.find(action.requester);
                const std::optional<ConnectionId> requesterConnection =
                    requester == playerToConnection_.end()
                    ? std::nullopt
                    : std::optional<ConnectionId>{requester->second};
                BroadcastSnapshot(
                    result,
                    room->second,
                    requesterConnection,
                    action.requestId);

                const auto expiresInSeconds = static_cast<std::uint16_t>(
                    std::min<std::int64_t>(
                        remaining,
                        static_cast<std::int64_t>(
                            dxa::protocol::MatchTicketLifetimeSeconds)));
                for (const ReservedParticipant& participant : action.participants)
                {
                    const auto connection = playerToConnection_.find(participant.player);
                    if (connection == playerToConnection_.end())
                    {
                        continue;
                    }
                    result.outbound.push_back({
                        connection->second,
                        dxa::protocol::MatchTicket{
                            requesterConnection.has_value()
                                    && connection->second == *requesterConnection
                                ? action.requestId
                                : 0U,
                            action.match,
                            participant.ticket,
                            value.endpoint.host,
                            value.endpoint.tcpPort,
                            value.endpoint.udpPort,
                            expiresInSeconds}});
                }

                LobbyAuditEvent audit = Audit(
                    LobbyAuditEventType::MatchStarted,
                    requesterConnection,
                    action.requester,
                    action.room);
                audit.match = action.match;
                audit.endpoint = value.endpoint;
                result.audit.push_back(std::move(audit));
                return result;
            }
            else if constexpr (std::is_same_v<Event, MatchFinishedEvent>)
            {
                return FinishActiveMatch(value.worker, value.match, false);
            }
            else
            {
                static_assert(std::is_same_v<Event, MatchUnavailableEvent>);
                return FinishActiveMatch(value.worker, value.match, true);
            }
        },
        event);
}

MatchTicketRegistry& LobbyService::Tickets() noexcept
{
    return tickets_;
}

std::size_t LobbyService::RoomCount() const noexcept
{
    return rooms_.size();
}

bool LobbyService::HasRoom(const RoomId room) const noexcept
{
    return rooms_.contains(room);
}

LobbyServiceResult LobbyService::HandleHello(
    const ConnectionId connection,
    ConnectionState& state,
    const ClientHello& request)
{
    if (state.player.has_value())
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::AlreadyWelcomed);
    }

    const auto value = TakeNext(nextPlayer_);
    if (!value.has_value())
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::IdSpaceExhausted);
    }

    const PlayerId player{*value};
    state.player = player;
    playerToConnection_.emplace(player, connection);

    LobbyServiceResult result;
    result.outbound.push_back({
        connection,
        dxa::protocol::ServerWelcome{request.requestId, player}});
    result.audit.push_back(Audit(
        LobbyAuditEventType::PlayerAssigned,
        connection,
        player));
    return result;
}

LobbyServiceResult LobbyService::HandleListRooms(
    const ConnectionId connection,
    const ListRoomsRequest& request) const
{
    LobbyServiceResult result;
    result.outbound.push_back({connection, RoomList(request.requestId)});
    return result;
}

LobbyServiceResult LobbyService::HandleCreateRoom(
    const ConnectionId connection,
    const PlayerId player,
    const CreateRoomRequest& request)
{
    if (playerToRoom_.contains(player))
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::AlreadyInRoom);
    }
    if (rooms_.size() >= maximumRooms_)
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::RoomLimitReached);
    }
    if (!nextRoom_.has_value() || !nextJoinOrdinal_.has_value())
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::IdSpaceExhausted);
    }

    const RoomId roomId{*TakeNext(nextRoom_)};
    const std::uint64_t joinOrdinal = *TakeNext(nextJoinOrdinal_);
    auto [roomIt, inserted] = rooms_.emplace(
        roomId,
        Room::Create(roomId, player, joinOrdinal));
    if (!inserted)
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::InternalError);
    }
    playerToRoom_.emplace(player, roomId);

    LobbyServiceResult result;
    BroadcastSnapshot(result, roomIt->second, connection, request.requestId);
    result.audit.push_back(Audit(
        LobbyAuditEventType::RoomCreated,
        connection,
        player,
        roomId));
    return result;
}

LobbyServiceResult LobbyService::HandleJoinRoom(
    const ConnectionId connection,
    const PlayerId player,
    const JoinRoomRequest& request)
{
    if (playerToRoom_.contains(player))
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::AlreadyInRoom);
    }

    const auto roomIt = rooms_.find(request.room);
    if (roomIt == rooms_.end())
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::RoomNotFound);
    }

    const auto before = roomIt->second.Snapshot(0U);
    if (before.state != RoomState::Waiting)
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::RoomNotJoinable);
    }
    if (before.members.size() >= dxa::protocol::RoomCapacity)
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::RoomFull);
    }

    const auto ordinal = TakeNext(nextJoinOrdinal_);
    if (!ordinal.has_value())
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::IdSpaceExhausted);
    }
    const auto joinError = roomIt->second.Join(player, *ordinal);
    if (joinError.has_value())
    {
        return Error(connection, request.requestId, *joinError);
    }
    playerToRoom_.emplace(player, request.room);

    LobbyServiceResult result;
    BroadcastSnapshot(result, roomIt->second, connection, request.requestId);
    return result;
}

LobbyServiceResult LobbyService::HandleLeaveRoom(
    const ConnectionId connection,
    const PlayerId player,
    const LeaveRoomRequest& request)
{
    if (!playerToRoom_.contains(player))
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::NotInRoom);
    }
    return LeaveWaitingRoom(player, connection, request.requestId);
}

LobbyServiceResult LobbyService::HandleSetReady(
    const ConnectionId connection,
    const PlayerId player,
    const SetReadyRequest& request)
{
    const auto membership = playerToRoom_.find(player);
    if (membership == playerToRoom_.end())
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::NotInRoom);
    }
    const auto roomIt = rooms_.find(membership->second);
    if (roomIt == rooms_.end())
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::InternalError);
    }

    const auto readyError = roomIt->second.SetReady(player, request.ready);
    if (readyError.has_value())
    {
        return Error(connection, request.requestId, *readyError);
    }

    LobbyServiceResult result;
    BroadcastSnapshot(result, roomIt->second, connection, request.requestId);
    return result;
}

LobbyServiceResult LobbyService::HandleStartMatch(
    const ConnectionId connection,
    const PlayerId player,
    const StartMatchRequest& request,
    const std::chrono::steady_clock::time_point now)
{
    const auto membership = playerToRoom_.find(player);
    if (membership == playerToRoom_.end())
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::NotInRoom);
    }
    const RoomId roomId = membership->second;
    const auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end())
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::InternalError);
    }

    Room& room = roomIt->second;
    const auto validationError = room.ValidateStart(player);
    if (validationError.has_value())
    {
        return Error(connection, request.requestId, *validationError);
    }

    if (!nextMatch_.has_value() || !nextReservation_.has_value())
    {
        return Error(
            connection,
            request.requestId,
            LobbyError::IdSpaceExhausted);
    }
    const dxa::protocol::MatchId match{*TakeNext(nextMatch_)};
    const ReservationId reservation{*TakeNext(nextReservation_)};
    const std::vector<PlayerId> players = room.Players();
    room.BeginStarting();

    std::vector<MatchTicketValue> temporaryTickets;
    temporaryTickets.reserve(players.size());

    const auto rollback = [
        this,
        &room,
        &temporaryTickets,
        connection,
        player,
        roomId,
        match,
        requestId = request.requestId](const LobbyError error) {
        tickets_.Revoke(temporaryTickets);
        room.ReturnToWaiting();

        LobbyServiceResult result;
        BroadcastSnapshot(result, room, std::nullopt, 0U);
        result.outbound.push_back({
            connection,
            ErrorResponse{requestId, error}});

        LobbyAuditEvent event = Audit(
            LobbyAuditEventType::StartFailed,
            connection,
            player,
            roomId);
        event.match = match;
        event.error = error;
        result.audit.push_back(std::move(event));
        return result;
    };

    for (const PlayerId participant : players)
    {
        const auto ticket = tickets_.Issue(match, participant, now);
        if (!ticket.has_value())
        {
            return rollback(LobbyError::InternalError);
        }
        temporaryTickets.push_back(*ticket);
    }

    std::vector<ReservedParticipant> participants;
    participants.reserve(players.size());
    for (std::size_t index = 0U; index < players.size(); ++index)
    {
        participants.push_back({players[index], temporaryTickets[index]});
    }

    const std::uint32_t seed = matchSeedBase_
        ^ static_cast<std::uint32_t>(match.value)
        ^ static_cast<std::uint32_t>(match.value >> 32U);
    ReserveMatchAction action{
        reservation,
        roomId,
        match,
        player,
        request.requestId,
        seed,
        now,
        std::move(participants)};
    const auto [pending, inserted] = pendingReservations_.emplace(
        reservation,
        PendingReservation{action});
    static_cast<void>(pending);
    const auto [roomReservation, roomInserted] = roomToReservation_.emplace(
        roomId,
        reservation);
    static_cast<void>(roomReservation);
    if (!inserted || !roomInserted)
    {
        pendingReservations_.erase(reservation);
        roomToReservation_.erase(roomId);
        return rollback(LobbyError::InternalError);
    }

    LobbyServiceResult result;
    BroadcastSnapshot(result, room, connection, request.requestId);
    result.actions.push_back(std::move(action));
    return result;
}

LobbyServiceResult LobbyService::FailPendingReservation(
    const ReservationId reservation,
    const LobbyError error)
{
    const auto pending = pendingReservations_.find(reservation);
    if (pending == pendingReservations_.end())
    {
        return {};
    }
    ReserveMatchAction action = std::move(pending->second.action);
    pendingReservations_.erase(pending);
    roomToReservation_.erase(action.room);

    std::vector<MatchTicketValue> issued;
    issued.reserve(action.participants.size());
    for (const ReservedParticipant& participant : action.participants)
    {
        issued.push_back(participant.ticket);
    }
    tickets_.Revoke(issued);

    LobbyServiceResult result;
    const auto room = rooms_.find(action.room);
    if (room != rooms_.end()
        && room->second.Snapshot(0U).state == RoomState::Starting)
    {
        room->second.ReturnToWaiting();
        BroadcastSnapshot(result, room->second, std::nullopt, 0U);
    }

    const auto requester = playerToConnection_.find(action.requester);
    const std::optional<ConnectionId> requesterConnection =
        requester == playerToConnection_.end()
        ? std::nullopt
        : std::optional<ConnectionId>{requester->second};
    if (requesterConnection.has_value())
    {
        result.outbound.push_back({
            *requesterConnection,
            ErrorResponse{action.requestId, error}});
    }

    LobbyAuditEvent audit = Audit(
        LobbyAuditEventType::StartFailed,
        requesterConnection,
        action.requester,
        action.room);
    audit.match = action.match;
    audit.error = error;
    result.audit.push_back(std::move(audit));
    return result;
}

LobbyServiceResult LobbyService::FinishActiveMatch(
    const WorkerId worker,
    const dxa::protocol::MatchId match,
    const bool unavailable)
{
    const auto active = activeMatches_.find(match);
    if (active == activeMatches_.end() || active->second.worker != worker)
    {
        return {};
    }
    ActiveMatch record = std::move(active->second);
    activeMatches_.erase(active);
    tickets_.Revoke(record.tickets);

    const auto room = rooms_.find(record.room);
    if (room == rooms_.end())
    {
        return {};
    }
    const std::vector<PlayerId> players = room->second.Players();
    std::vector<ConnectionId> recipients;
    recipients.reserve(players.size());
    for (const PlayerId player : players)
    {
        playerToRoom_.erase(player);
        const auto connection = playerToConnection_.find(player);
        if (connection != playerToConnection_.end())
        {
            recipients.push_back(connection->second);
        }
    }
    rooms_.erase(room);

    LobbyServiceResult result;
    for (const ConnectionId recipient : recipients)
    {
        if (unavailable)
        {
            result.outbound.push_back({
                recipient,
                ErrorResponse{0U, LobbyError::MatchUnavailable}});
        }
        result.outbound.push_back({recipient, RoomList(0U)});
    }
    result.audit.push_back(Audit(
        LobbyAuditEventType::RoomDeleted,
        std::nullopt,
        std::nullopt,
        record.room));
    return result;
}

LobbyServiceResult LobbyService::DisconnectStartingPlayer(
    const PlayerId player,
    const RoomId roomId)
{
    const auto roomReservation = roomToReservation_.find(roomId);
    if (roomReservation == roomToReservation_.end())
    {
        return {};
    }
    const auto pending = pendingReservations_.find(roomReservation->second);
    if (pending == pendingReservations_.end())
    {
        roomToReservation_.erase(roomReservation);
        return {};
    }

    ReserveMatchAction action = std::move(pending->second.action);
    pendingReservations_.erase(pending);
    roomToReservation_.erase(roomReservation);
    std::vector<MatchTicketValue> issued;
    issued.reserve(action.participants.size());
    for (const ReservedParticipant& participant : action.participants)
    {
        issued.push_back(participant.ticket);
    }
    tickets_.Revoke(issued);

    const auto room = rooms_.find(roomId);
    if (room == rooms_.end())
    {
        return {};
    }
    room->second.ReturnToWaiting();

    LobbyServiceResult result = LeaveWaitingRoom(
        player,
        std::nullopt,
        0U);
    result.actions.push_back(CancelReservationAction{
        action.reservation,
        action.match});

    std::optional<ConnectionId> requesterConnection;
    if (action.requester != player)
    {
        const auto requester = playerToConnection_.find(action.requester);
        if (requester != playerToConnection_.end())
        {
            requesterConnection = requester->second;
            result.outbound.push_back({
                *requesterConnection,
                ErrorResponse{
                    action.requestId,
                    LobbyError::WorkerUnavailable}});
        }
    }

    LobbyAuditEvent audit = Audit(
        LobbyAuditEventType::StartFailed,
        requesterConnection,
        action.requester,
        action.room);
    audit.match = action.match;
    audit.error = LobbyError::WorkerUnavailable;
    result.audit.push_back(std::move(audit));
    return result;
}

LobbyServiceResult LobbyService::LeaveWaitingRoom(
    const PlayerId player,
    const std::optional<ConnectionId> requester,
    const std::uint32_t requestId)
{
    const auto membership = playerToRoom_.find(player);
    if (membership == playerToRoom_.end())
    {
        return {};
    }
    const RoomId roomId = membership->second;
    const auto roomIt = rooms_.find(roomId);
    if (roomIt == rooms_.end())
    {
        playerToRoom_.erase(membership);
        return {};
    }

    const PlayerId oldHost = roomIt->second.Host();
    const auto leaveError = roomIt->second.Leave(player);
    if (leaveError.has_value())
    {
        if (requester.has_value())
        {
            return Error(*requester, requestId, *leaveError);
        }
        return {};
    }
    playerToRoom_.erase(membership);

    LobbyServiceResult result;
    if (roomIt->second.Empty())
    {
        rooms_.erase(roomIt);
        result.audit.push_back(Audit(
            LobbyAuditEventType::RoomDeleted,
            requester,
            player,
            roomId));
    }
    else
    {
        const PlayerId newHost = roomIt->second.Host();
        BroadcastSnapshot(result, roomIt->second, std::nullopt, 0U);
        if (newHost != oldHost)
        {
            result.audit.push_back(Audit(
                LobbyAuditEventType::HostTransferred,
                playerToConnection_.contains(newHost)
                    ? std::optional<ConnectionId>{playerToConnection_.at(newHost)}
                    : std::nullopt,
                newHost,
                roomId));
        }
    }

    if (requester.has_value())
    {
        result.outbound.push_back({*requester, RoomList(requestId)});
    }
    return result;
}

LobbyServiceResult LobbyService::Error(
    const ConnectionId connection,
    const std::uint32_t requestId,
    const LobbyError error) const
{
    LobbyServiceResult result;
    result.outbound.push_back({
        connection,
        ErrorResponse{requestId, error}});
    return result;
}

RoomListResponse LobbyService::RoomList(const std::uint32_t requestId) const
{
    RoomListResponse response;
    response.requestId = requestId;
    response.rooms.reserve(rooms_.size());
    for (const auto& [roomId, room] : rooms_)
    {
        const auto snapshot = room.Snapshot(0U);
        if (snapshot.state != RoomState::Waiting)
        {
            continue;
        }
        response.rooms.push_back({
            roomId,
            static_cast<std::uint8_t>(snapshot.members.size()),
            static_cast<std::uint8_t>(dxa::protocol::RoomCapacity)});
    }
    return response;
}

void LobbyService::BroadcastSnapshot(
    LobbyServiceResult& result,
    const Room& room,
    const std::optional<ConnectionId> requester,
    const std::uint32_t requestId) const
{
    for (const PlayerId player : room.Players())
    {
        const auto connection = playerToConnection_.find(player);
        if (connection == playerToConnection_.end())
        {
            continue;
        }
        const std::uint32_t outboundRequestId =
            requester.has_value() && connection->second == *requester
            ? requestId
            : 0U;
        result.outbound.push_back({
            connection->second,
            room.Snapshot(outboundRequestId)});
    }
}
} // namespace dxa::lobby
