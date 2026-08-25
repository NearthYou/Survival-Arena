#pragma once

#include <dxa/protocol/GameTypes.hpp>
#include <dxa/protocol/DatagramShaper.hpp>
#include <dxa/protocol/Ids.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace dxa::game_server
{
struct GameServerOptions
{
    std::string lobbyControlHost = "127.0.0.1";
    std::uint16_t lobbyControlPort = 7001U;
    dxa::protocol::WorkerId worker{1U};
    std::string advertisedHost = "127.0.0.1";
    std::string gameBindAddress = "127.0.0.1";
    std::uint16_t gameTcpPort = 7100U;
    std::uint16_t gameUdpPort = 7101U;
    dxa::protocol::ReplicationMode replicationMode =
        dxa::protocol::ReplicationMode::FullState;
    std::string metricsOutputRoot;
    dxa::protocol::DatagramShaperConfig udpImpairment;
};

struct GameServerOptionsParseResult
{
    std::optional<GameServerOptions> options;
    std::string error;
};

[[nodiscard]] GameServerOptionsParseResult ParseGameServerOptions(
    std::span<const std::string_view> arguments);
} // namespace dxa::game_server
