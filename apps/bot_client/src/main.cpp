#include <dxa/bot_client/BotClientOptions.hpp>
#include <dxa/bot_client/BotCoordinator.hpp>

#include <boost/asio.hpp>

#include <algorithm>
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
        const auto parsed =
            dxa::bot_client::ParseBotClientOptions(arguments);
        if (!parsed.options.has_value())
        {
            std::cerr << parsed.error << '\n';
            return 2;
        }

        boost::asio::io_context io;
        dxa::bot_client::BotCoordinator coordinator{io, *parsed.options};
        coordinator.Start();
        io.run();
        const dxa::bot_client::BotCoordinatorReport report =
            coordinator.Report();
        for (const dxa::bot_client::BotSessionReport& session
             : report.sessions)
        {
            std::cout << "bot session player=" << session.player.value
                      << " match="
                      << (session.match.has_value()
                              ? session.match->value
                              : 0U)
                      << " snapshots_applied=" << session.snapshotsApplied
                      << " tcp_received_bytes=" << session.receivedTcpBytes
                      << " udp_received_bytes=" << session.receivedUdpBytes
                      << " discarded_snapshots="
                      << session.discardedSnapshots
                      << " keyframe_requests=" << session.keyframeRequests
                      << " exit=" << session.exitCode << '\n';
        }
        if (report.result.has_value())
        {
            std::cout << "bot result match=" << report.result->match.value
                      << " winner=";
            if (report.result->hasWinner)
            {
                std::cout << report.result->winner.value;
            }
            else
            {
                std::cout << "none";
            }
            std::cout << " reason="
                      << static_cast<std::uint32_t>(report.result->reason)
                      << " tick=" << report.result->finishedTick
                      << " sessions=" << report.sessions.size()
                      << " exit=" << report.exitCode << '\n';
        }
        return report.exitCode;
    }
    catch (const std::exception& error)
    {
        std::cerr << "bot client failed: " << error.what() << '\n';
        return 3;
    }
}
