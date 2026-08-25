#include <dxa/game_server/GameServer.hpp>
#include <dxa/game_server/GameServerOptions.hpp>

#include <boost/asio.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <csignal>
#include <cstddef>
#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

int main(const int argc, const char* const* const argv)
{
    try
    {
        std::vector<std::string_view> arguments;
        arguments.reserve(static_cast<std::size_t>(std::max(0, argc - 1)));
        for (int index = 1; index < argc; ++index)
        {
            arguments.emplace_back(argv[index]);
        }
        const auto parsed = dxa::game_server::ParseGameServerOptions(arguments);
        if (!parsed.options.has_value())
        {
            std::cerr << parsed.error << '\n';
            return 2;
        }

        boost::asio::io_context io;
        dxa::game_server::GameServerConfig config;
        config.options = *parsed.options;
        dxa::game_server::GameServer server{io, config};
        boost::asio::signal_set signals{io, SIGINT, SIGTERM};
        signals.async_wait(
            [&server](const boost::system::error_code error, const int) {
                if (!error)
                {
                    server.Stop();
                }
            });

        server.Start();
        spdlog::info(
            "game_server_listening worker={} tcp_port={} udp_port={}",
            parsed.options->worker.value,
            server.GameTcpPort(),
            server.GameUdpPort());
        io.run();
        return 0;
    }
    catch (const std::exception& error)
    {
        spdlog::error("game server failed: {}", error.what());
        return 1;
    }
}
