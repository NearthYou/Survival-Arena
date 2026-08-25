#include <dxa/lobby/LobbyServerOptions.hpp>
#include <dxa/lobby/LobbyTcpServer.hpp>
#include <dxa/lobby/MatchTicketRegistry.hpp>

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

        const auto parsed = dxa::lobby::ParseLobbyServerOptions(arguments);
        if (!parsed.options.has_value())
        {
            std::cerr << parsed.error << '\n';
            return 2;
        }

        boost::asio::io_context io;
        dxa::lobby::SecureTicketSource ticketSource;
        dxa::lobby::MatchTicketRegistry tickets{ticketSource};
        dxa::lobby::LobbyService service{tickets};
        dxa::lobby::LobbyTcpServer server{
            io,
            service,
            boost::asio::ip::tcp::endpoint{
                boost::asio::ip::make_address(parsed.options->bindAddress),
                parsed.options->port},
            boost::asio::ip::tcp::endpoint{
                boost::asio::ip::make_address(
                    parsed.options->workerBindAddress),
                parsed.options->workerPort}};
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
            "lobby_server_listening address={} port={} worker_address={} worker_port={}",
            parsed.options->bindAddress,
            server.LocalPort(),
            parsed.options->workerBindAddress,
            server.WorkerControlPort());
        io.run();
        return 0;
    }
    catch (const std::exception& error)
    {
        spdlog::error("lobby server failed: {}", error.what());
        return 1;
    }
}
