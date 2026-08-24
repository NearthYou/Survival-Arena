#include <dxa/lobby_cli/LobbyCliCommand.hpp>
#include <dxa/lobby_cli/LobbyCliOutput.hpp>
#include <dxa/lobby_client/LobbyClient.hpp>

#include <boost/asio.hpp>

#include <algorithm>
#include <charconv>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace
{
struct CliOptions
{
    std::string host = "127.0.0.1";
    std::uint16_t port = 7000U;
};

[[nodiscard]] std::optional<std::uint16_t> ParsePort(
    const std::string_view text) noexcept
{
    std::uint32_t value = 0U;
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value);
    if (error != std::errc{}
        || end != text.data() + text.size()
        || value == 0U
        || value > 65535U)
    {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(value);
}

[[nodiscard]] std::optional<CliOptions> ParseOptions(
    const std::vector<std::string_view>& arguments)
{
    CliOptions options;
    bool sawHost = false;
    bool sawPort = false;
    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        if (index + 1U >= arguments.size())
        {
            return std::nullopt;
        }
        const std::string_view option = arguments[index];
        const std::string_view value = arguments[++index];
        if (option == "--host" && !sawHost)
        {
            sawHost = true;
            options.host = value;
        }
        else if (option == "--port" && !sawPort)
        {
            sawPort = true;
            const auto port = ParsePort(value);
            if (!port.has_value())
            {
                return std::nullopt;
            }
            options.port = *port;
        }
        else
        {
            return std::nullopt;
        }
    }
    if (options.host.empty())
    {
        return std::nullopt;
    }
    return options;
}

void Dispatch(
    dxa::lobby_client::LobbyClient& client,
    const dxa::lobby_cli::LobbyCliCommand& command)
{
    using dxa::lobby_cli::LobbyCliCommandType;
    switch (command.type)
    {
    case LobbyCliCommandType::List:
        static_cast<void>(client.ListRooms());
        return;
    case LobbyCliCommandType::Create:
        static_cast<void>(client.CreateRoom());
        return;
    case LobbyCliCommandType::Join:
        static_cast<void>(client.JoinRoom(command.room));
        return;
    case LobbyCliCommandType::Leave:
        static_cast<void>(client.LeaveRoom());
        return;
    case LobbyCliCommandType::Ready:
        static_cast<void>(client.SetReady(command.ready));
        return;
    case LobbyCliCommandType::Start:
        static_cast<void>(client.StartMatch());
        return;
    case LobbyCliCommandType::Quit:
        client.Close();
        return;
    }
}
} // namespace

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
        const auto options = ParseOptions(arguments);
        if (!options.has_value())
        {
            std::cerr << "usage: dxa_lobby_cli [--host HOST] [--port PORT]\n";
            return 2;
        }

        boost::asio::io_context io;
        auto work = boost::asio::make_work_guard(io);
        const auto client = dxa::lobby_client::LobbyClient::Create(io);
        std::mutex stateMutex;
        std::condition_variable stateChanged;
        bool connected = false;
        bool stopped = false;
        std::mutex outputMutex;
        const auto print = [&outputMutex](const std::string& text) {
            const std::scoped_lock lock{outputMutex};
            std::cout << text << '\n';
        };

        client->AsyncConnect(
            options->host,
            options->port,
            dxa::lobby_client::LobbyClientCallbacks{
                [client, &stateMutex, &stateChanged, &connected, &print] {
                    static_cast<void>(client->Hello());
                    {
                        const std::scoped_lock lock{stateMutex};
                        connected = true;
                    }
                    stateChanged.notify_all();
                    print("connected; commands: list, create, join ID, leave, ready on, ready off, start, quit");
                },
                [&print](dxa::protocol::ServerMessage message) {
                    print(dxa::lobby_cli::FormatLobbyServerMessage(message));
                },
                [&stateMutex, &stateChanged, &stopped, &work, &print](
                    const boost::system::error_code error) {
                    {
                        const std::scoped_lock lock{stateMutex};
                        stopped = true;
                    }
                    work.reset();
                    stateChanged.notify_all();
                    print("connection closed error=" + std::to_string(error.value()));
                }});

        std::thread inputThread{
            [&io,
             &stateMutex,
             &stateChanged,
             &connected,
             &stopped,
             &print,
             client] {
                {
                    std::unique_lock lock{stateMutex};
                    stateChanged.wait(lock, [&connected, &stopped] {
                        return connected || stopped;
                    });
                    if (stopped)
                    {
                        return;
                    }
                }

                for (std::string line; std::getline(std::cin, line);)
                {
                    const auto parsed =
                        dxa::lobby_cli::ParseLobbyCliCommand(line);
                    if (!parsed.command.has_value())
                    {
                        print("command error: " + parsed.error);
                        continue;
                    }
                    const dxa::lobby_cli::LobbyCliCommand command =
                        *parsed.command;
                    boost::asio::post(io, [client, command, &print] {
                        try
                        {
                            Dispatch(*client, command);
                        }
                        catch (const std::exception& error)
                        {
                            print("command failed: " + std::string{error.what()});
                            client->Close();
                        }
                    });
                    if (command.type ==
                        dxa::lobby_cli::LobbyCliCommandType::Quit)
                    {
                        return;
                    }
                }
                boost::asio::post(io, [client] { client->Close(); });
            }};

        io.run();
        if (inputThread.joinable())
        {
            inputThread.join();
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "lobby CLI failed: " << error.what() << '\n';
        return 1;
    }
}
