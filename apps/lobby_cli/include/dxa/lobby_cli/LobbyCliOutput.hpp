#pragma once

#include <dxa/protocol/LobbyMessages.hpp>

#include <string>

namespace dxa::lobby_cli
{
[[nodiscard]] std::string FormatLobbyServerMessage(
    const dxa::protocol::ServerMessage& message);
} // namespace dxa::lobby_cli
