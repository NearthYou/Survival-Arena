#pragma once

#include <cstddef>
#include <cstdint>

namespace dxa::protocol
{
inline constexpr std::uint16_t ProtocolVersion = 1U;
inline constexpr std::size_t TcpFrameHeaderBytes = 12U;
inline constexpr std::size_t MaxTcpFrameBytes = 65536U;
inline constexpr std::size_t MaxTcpPayloadBytes = MaxTcpFrameBytes - TcpFrameHeaderBytes;
inline constexpr std::size_t RoomCapacity = 24U;
inline constexpr std::size_t MaximumRooms = 1024U;
inline constexpr std::size_t MatchTicketBytes = 16U;
inline constexpr std::size_t MatchTicketLifetimeSeconds = 60U;
inline constexpr std::size_t MaxPendingWriteBytes = 262144U;

enum class RoomState : std::uint8_t
{
    Waiting = 1,
    Starting = 2,
    InMatch = 3
};

enum class MessageType : std::uint16_t
{
    ClientHello = 1,
    ServerWelcome = 2,
    ListRoomsRequest = 3,
    RoomListResponse = 4,
    CreateRoomRequest = 5,
    JoinRoomRequest = 6,
    LeaveRoomRequest = 7,
    SetReadyRequest = 8,
    StartMatchRequest = 9,
    RoomSnapshot = 10,
    MatchTicket = 11,
    ErrorResponse = 12
};

enum class LobbyError : std::uint16_t
{
    MalformedPayload = 1,
    UnsupportedVersion = 2,
    UnknownMessageType = 3,
    FrameTooLarge = 4,
    RequestOutOfOrder = 5,
    NotWelcomed = 6,
    AlreadyWelcomed = 7,
    AlreadyInRoom = 8,
    NotInRoom = 9,
    RoomNotFound = 10,
    RoomNotJoinable = 11,
    RoomFull = 12,
    RoomLimitReached = 13,
    NotHost = 14,
    MinimumPlayersRequired = 15,
    NotAllReady = 16,
    WorkerUnavailable = 17,
    IdSpaceExhausted = 18,
    InternalError = 19
};
} // namespace dxa::protocol
