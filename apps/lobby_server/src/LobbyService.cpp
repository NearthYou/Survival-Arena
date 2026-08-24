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
using dxa::protocol::JoinRoomRequest;
using dxa::protocol::LeaveRoomRequest;
using dxa::protocol::ListRoomsRequest;
using dxa::protocol::LobbyError;
using dxa::protocol::PlayerId;
using dxa::protocol::RoomId;
using dxa::protocol::RoomListResponse;
using dxa::protocol::RoomState;
using dxa::protocol::SetReadyRequest;
using dxa::protocol::StartMatchRequest;

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
} // namespace

LobbyService::LobbyService(
    IGameWorkerAllocator& allocator,
    MatchTicketRegistry& tickets,
    const LobbyServiceConfig config)
    : allocator_{allocator},
      tickets_{tickets},
      nextConnection_{InitialCounter(config.nextConnection)},
      nextPlayer_{InitialCounter(config.nextPlayer)},
      nextRoom_{InitialCounter(config.nextRoom)},
      nextMatch_{InitialCounter(config.nextMatch)},
      nextJoinOrdinal_{InitialCounter(config.nextJoinOrdinal)},
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
    static_cast<void>(now);
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
        [this, connection, player](const auto& request) -> LobbyServiceResult {
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
                return Error(
                    connection,
                    request.requestId,
                    LobbyError::WorkerUnavailable);
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
        if (playerToRoom_.contains(player))
        {
            result = LeaveWaitingRoom(player, std::nullopt, 0U);
        }
        playerToConnection_.erase(player);
    }
    connections_.erase(connectionIt);
    return result;
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
