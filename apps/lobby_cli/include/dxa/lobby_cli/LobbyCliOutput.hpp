#pragma once

#include <dxa/protocol/LobbyMessages.hpp>

#include <iosfwd>
#include <string>
#include <string_view>

namespace dxa::lobby_cli
{
[[nodiscard]] std::string FormatLobbyServerMessage(
    const dxa::protocol::ServerMessage& message);
void WriteLobbyCliLine(std::ostream& output, std::string_view text);
} // namespace dxa::lobby_cli
