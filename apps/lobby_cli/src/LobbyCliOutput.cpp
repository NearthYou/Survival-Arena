#include <dxa/lobby_cli/LobbyCliOutput.hpp>

#include <cstdint>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <variant>

namespace dxa::lobby_cli
{
using namespace dxa::protocol;

namespace
{
[[nodiscard]] std::string_view RoomStateName(const RoomState state) noexcept
{
    switch (state)
    {
    case RoomState::Waiting:
        return "Waiting";
    case RoomState::Starting:
        return "Starting";
    case RoomState::InMatch:
        return "InMatch";
    }
    return "Unknown";
}

[[nodiscard]] std::string_view ErrorName(const LobbyError error) noexcept
{
    switch (error)
    {
    case LobbyError::MalformedPayload:
        return "MalformedPayload";
    case LobbyError::UnsupportedVersion:
        return "UnsupportedVersion";
    case LobbyError::UnknownMessageType:
        return "UnknownMessageType";
    case LobbyError::FrameTooLarge:
        return "FrameTooLarge";
    case LobbyError::RequestOutOfOrder:
        return "RequestOutOfOrder";
    case LobbyError::NotWelcomed:
        return "NotWelcomed";
    case LobbyError::AlreadyWelcomed:
        return "AlreadyWelcomed";
    case LobbyError::AlreadyInRoom:
        return "AlreadyInRoom";
    case LobbyError::NotInRoom:
        return "NotInRoom";
    case LobbyError::RoomNotFound:
        return "RoomNotFound";
    case LobbyError::RoomNotJoinable:
        return "RoomNotJoinable";
    case LobbyError::RoomFull:
        return "RoomFull";
    case LobbyError::RoomLimitReached:
        return "RoomLimitReached";
    case LobbyError::NotHost:
        return "NotHost";
    case LobbyError::MinimumPlayersRequired:
        return "MinimumPlayersRequired";
    case LobbyError::NotAllReady:
        return "NotAllReady";
    case LobbyError::WorkerUnavailable:
        return "WorkerUnavailable";
    case LobbyError::IdSpaceExhausted:
        return "IdSpaceExhausted";
    case LobbyError::InternalError:
        return "InternalError";
    }
    return "UnknownError";
}
} // namespace

std::string FormatLobbyServerMessage(const dxa::protocol::ServerMessage& message)
{
    return std::visit(
        [](const auto& value) {
            using Message = std::decay_t<decltype(value)>;
            std::ostringstream output;
            if constexpr (std::is_same_v<Message, ServerWelcome>)
            {
                output << "welcome request=" << value.requestId
                       << " player=" << value.player.value;
            }
            else if constexpr (std::is_same_v<Message, RoomListResponse>)
            {
                output << "room list request=" << value.requestId
                       << " count=" << value.rooms.size();
                for (const RoomSummary& room : value.rooms)
                {
                    output << " room=" << room.room.value
                           << " players=" << static_cast<std::uint32_t>(room.players)
                           << '/' << static_cast<std::uint32_t>(room.capacity);
                }
            }
            else if constexpr (std::is_same_v<Message, RoomSnapshot>)
            {
                output << "room snapshot request=" << value.requestId
                       << " room=" << value.room.value
                       << " state=" << RoomStateName(value.state)
                       << " host=" << value.host.value
                       << " members=" << value.members.size();
                for (const RoomMemberView& member : value.members)
                {
                    output << " player=" << member.player.value
                           << " ready=" << (member.ready ? "on" : "off");
                }
            }
            else if constexpr (std::is_same_v<Message, MatchTicket>)
            {
                output << "match ticket received request=" << value.requestId
                       << " match=" << value.match.value
                       << " endpoint=" << value.host
                       << ':' << value.tcpPort
                       << " udp=" << value.udpPort
                       << " expires=" << value.expiresInSeconds;
            }
            else if constexpr (std::is_same_v<Message, ErrorResponse>)
            {
                output << "error request=" << value.requestId
                       << " code=" << ErrorName(value.error);
            }
            return output.str();
        },
        message);
}
} // namespace dxa::lobby_cli
