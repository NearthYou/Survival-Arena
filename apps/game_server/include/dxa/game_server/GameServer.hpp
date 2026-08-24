#pragma once

#include <dxa/game_server/GameServerOptions.hpp>

#include <dxa/simulation/MatchConfig.hpp>

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <cstdint>
#include <memory>

namespace dxa::game_server
{
struct GameServerConfig
{
    GameServerOptions options;
    dxa::simulation::MatchConfig matchConfig =
        dxa::simulation::DefaultMatchConfig();
    std::chrono::milliseconds authenticationTimeout{5000};
    std::chrono::milliseconds controlReconnectDelay{1000};
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

private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace dxa::game_server
