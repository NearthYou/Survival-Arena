#pragma once

#include <dxa/game_common/NetworkMetrics.hpp>
#include <dxa/game_server/GameServerOptions.hpp>
#include <dxa/game_server/ServerMatchMetrics.hpp>
#include <dxa/game_server/UdpTokenSource.hpp>

#include <dxa/simulation/MatchConfig.hpp>

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace dxa::game_server
{
struct GameServerConfig
{
    GameServerOptions options;
    dxa::simulation::MatchConfig matchConfig =
        dxa::simulation::DefaultMatchConfig();
    std::chrono::milliseconds authenticationTimeout{5000};
    std::chrono::milliseconds controlReconnectDelay{1000};
    std::shared_ptr<IUdpTokenSource> udpTokenSource;
};

class GameServer
{
public:
    GameServer(boost::asio::io_context& io, GameServerConfig config);
    ~GameServer();

    GameServer(const GameServer&) = delete;
    GameServer& operator=(const GameServer&) = delete;

    void Start();
    void Stop();
    [[nodiscard]] std::uint16_t GameTcpPort() const;
    [[nodiscard]] std::uint16_t GameUdpPort() const;
    [[nodiscard]] dxa::game_common::GameTrafficTotals Traffic() const;
    [[nodiscard]] std::vector<ServerMatchMetricsSnapshot>
    CompletedMetrics() const;
    [[nodiscard]] std::size_t CompletedMetricCount() const;

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace dxa::game_server
