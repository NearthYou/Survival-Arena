#pragma once

#include <dxa/protocol/GameTypes.hpp>
#include <dxa/protocol/Ids.hpp>
#include <dxa/protocol/LobbyTypes.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace dxa::protocol
{
struct ClientHello
{
    std::uint32_t requestId = 0;
    [[nodiscard]] bool operator==(const ClientHello&) const = default;
};

struct ListRoomsRequest
{
    std::uint32_t requestId = 0;
    [[nodiscard]] bool operator==(const ListRoomsRequest&) const = default;
};

struct CreateRoomRequest
{
    std::uint32_t requestId = 0;
    [[nodiscard]] bool operator==(const CreateRoomRequest&) const = default;
};

struct JoinRoomRequest
{
    std::uint32_t requestId = 0;
    RoomId room;
    [[nodiscard]] bool operator==(const JoinRoomRequest&) const = default;
};

struct LeaveRoomRequest
{
    std::uint32_t requestId = 0;
    [[nodiscard]] bool operator==(const LeaveRoomRequest&) const = default;
};

struct SetReadyRequest
{
    std::uint32_t requestId = 0;
    bool ready = false;
    [[nodiscard]] bool operator==(const SetReadyRequest&) const = default;
};

struct StartMatchRequest
{
    std::uint32_t requestId = 0;
    [[nodiscard]] bool operator==(const StartMatchRequest&) const = default;
};

struct RoomSummary
{
    RoomId room;
    std::uint8_t players = 0;
    std::uint8_t capacity = static_cast<std::uint8_t>(RoomCapacity);
    [[nodiscard]] bool operator==(const RoomSummary&) const = default;
};

struct RoomMemberView
{
    PlayerId player;
    bool ready = false;
    [[nodiscard]] bool operator==(const RoomMemberView&) const = default;
};

struct ServerWelcome
{
    std::uint32_t requestId = 0;
    PlayerId player;
    [[nodiscard]] bool operator==(const ServerWelcome&) const = default;
};

struct RoomListResponse
{
    std::uint32_t requestId = 0;
    std::vector<RoomSummary> rooms;
    [[nodiscard]] bool operator==(const RoomListResponse&) const = default;
};

struct RoomSnapshot
{
    std::uint32_t requestId = 0;
    RoomId room;
    RoomState state = RoomState::Waiting;
    PlayerId host;
    std::vector<RoomMemberView> members;
    [[nodiscard]] bool operator==(const RoomSnapshot&) const = default;
};

struct MatchTicket
{
    std::uint32_t requestId = 0;
    MatchId match;
    MatchTicketValue ticket;
    std::string host;
    std::uint16_t tcpPort = 0;
    std::uint16_t udpPort = 0;
    std::uint16_t expiresInSeconds = static_cast<std::uint16_t>(
        MatchTicketLifetimeSeconds);
    [[nodiscard]] bool operator==(const MatchTicket&) const = default;
};

struct ErrorResponse
{
    std::uint32_t requestId = 0;
    LobbyError error = LobbyError::InternalError;
    [[nodiscard]] bool operator==(const ErrorResponse&) const = default;
};

using ClientMessage = std::variant<
    ClientHello,
    ListRoomsRequest,
    CreateRoomRequest,
    JoinRoomRequest,
    LeaveRoomRequest,
    SetReadyRequest,
    StartMatchRequest>;

using ServerMessage = std::variant<
    ServerWelcome,
    RoomListResponse,
    RoomSnapshot,
    MatchTicket,
    ErrorResponse>;

[[nodiscard]] std::uint32_t RequestId(const ClientMessage& message) noexcept;
[[nodiscard]] std::uint32_t RequestId(const ServerMessage& message) noexcept;
} // namespace dxa::protocol
