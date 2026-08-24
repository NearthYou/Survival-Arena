#pragma once

#include <dxa/protocol/Ids.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace dxa::lobby_cli
{
enum class LobbyCliCommandType
{
    List,
    Create,
    Join,
    Leave,
    Ready,
    Start,
    Quit
};

struct LobbyCliCommand
{
    LobbyCliCommandType type = LobbyCliCommandType::List;
    dxa::protocol::RoomId room;
    bool ready = false;
};

struct LobbyCliCommandParseResult
{
    std::optional<LobbyCliCommand> command;
    std::string error;
};

[[nodiscard]] LobbyCliCommandParseResult ParseLobbyCliCommand(
    std::string_view line);
} // namespace dxa::lobby_cli
