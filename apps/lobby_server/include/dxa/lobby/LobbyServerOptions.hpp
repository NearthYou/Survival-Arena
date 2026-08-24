#pragma once

#include <dxa/lobby/GameWorkerAllocator.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace dxa::lobby
{
struct LobbyServerOptions
{
    std::string bindAddress = "127.0.0.1";
    std::uint16_t port = 7000U;
    std::optional<GameEndpoint> worker;
};

struct LobbyServerOptionsParseResult
{
    std::optional<LobbyServerOptions> options;
    std::string error;
};

[[nodiscard]] LobbyServerOptionsParseResult ParseLobbyServerOptions(
    std::span<const std::string_view> arguments);
} // namespace dxa::lobby
