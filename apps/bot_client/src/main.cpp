#include <dxa/bot_client/BotClientOptions.hpp>

#include <dxa/lobby_client/LobbyClient.hpp>

#include <boost/asio.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace
{
struct BotState
{
    std::shared_ptr<dxa::lobby_client::LobbyClient> client;
    std::optional<dxa::protocol::PlayerId> player;
    bool joinSent = false;
    bool readySent = false;
    bool ticketReceived = false;
};

class BotCoordinator final
    : public std::enable_shared_from_this<BotCoordinator>
{
public:
    [[nodiscard]] static std::shared_ptr<BotCoordinator> Create(
        boost::asio::io_context& io,
        dxa::bot_client::BotClientOptions options)
    {
        return std::shared_ptr<BotCoordinator>{
            new BotCoordinator{io, std::move(options)}};
    }

    void Start()
    {
        bots_.resize(options_.count);
        for (std::size_t index = 0; index < bots_.size(); ++index)
        {
            bots_[index].client =
                dxa::lobby_client::LobbyClient::Create(io_);
            const std::weak_ptr<BotCoordinator> weak = shared_from_this();
            bots_[index].client->AsyncConnect(
                options_.host,
                options_.port,
                dxa::lobby_client::LobbyClientCallbacks{
                    [weak, index] {
                        if (const auto self = weak.lock())
                        {
                            self->Connected(index);
                        }
                    },
                    [weak, index](dxa::protocol::ServerMessage message) {
                        if (const auto self = weak.lock())
                        {
                            self->Message(index, std::move(message));
                        }
                    },
                    [weak, index](const boost::system::error_code error) {
                        if (const auto self = weak.lock())
                        {
                            self->Closed(index, error);
                        }
                    }});
        }

        const std::weak_ptr<BotCoordinator> weak = shared_from_this();
        timer_.expires_after(std::chrono::seconds{30});
        timer_.async_wait([weak](const boost::system::error_code error) {
            if (!error)
            {
                if (const auto self = weak.lock())
                {
                    self->Timeout();
                }
            }
        });
    }

    [[nodiscard]] int ExitCode() const noexcept
    {
        return exitCode_;
    }

private:
    BotCoordinator(
        boost::asio::io_context& io,
        dxa::bot_client::BotClientOptions options)
        : io_{io},
          options_{std::move(options)},
          timer_{io}
    {
    }

    void Connected(const std::size_t index)
    {
        if (shuttingDown_)
        {
            return;
        }
        try
        {
            static_cast<void>(bots_.at(index).client->Hello());
        }
        catch (const std::exception& error)
        {
            Fail(error.what());
        }
    }

    void Message(
        const std::size_t index,
        dxa::protocol::ServerMessage message)
    {
        if (shuttingDown_)
        {
            return;
        }
        try
        {
            BotState& bot = bots_.at(index);
            if (const auto* welcome =
                    std::get_if<dxa::protocol::ServerWelcome>(&message))
            {
                if (bot.player.has_value())
                {
                    Fail("duplicate welcome");
                    return;
                }
                bot.player = welcome->player;
                bot.joinSent = true;
                static_cast<void>(bot.client->JoinRoom(options_.room));
                return;
            }
            if (const auto* room =
                    std::get_if<dxa::protocol::RoomSnapshot>(&message))
            {
                if (!bot.player.has_value())
                {
                    Fail("room snapshot before welcome");
                    return;
                }
                const auto member = std::find_if(
                    room->members.begin(),
                    room->members.end(),
                    [&bot](const auto& candidate) {
                        return candidate.player == *bot.player;
                    });
                if (member != room->members.end() && !bot.readySent)
                {
                    bot.readySent = true;
                    static_cast<void>(bot.client->SetReady(true));
                }
                return;
            }
            if (std::holds_alternative<dxa::protocol::MatchTicket>(message))
            {
                if (!bot.ticketReceived)
                {
                    bot.ticketReceived = true;
                    ++ticketsReceived_;
                    if (ticketsReceived_ == bots_.size())
                    {
                        Finish();
                    }
                }
                return;
            }
            if (const auto* error =
                    std::get_if<dxa::protocol::ErrorResponse>(&message))
            {
                Fail("server error " + std::to_string(
                    static_cast<std::uint16_t>(error->error)));
            }
        }
        catch (const std::exception& error)
        {
            Fail(error.what());
        }
    }

    void Closed(
        const std::size_t index,
        const boost::system::error_code error)
    {
        if (!shuttingDown_ && !bots_.at(index).ticketReceived)
        {
            Fail("connection closed " + std::to_string(error.value()));
        }
    }

    void Finish()
    {
        exitCode_ = 0;
        std::cout << "bot tickets received: "
                  << ticketsReceived_ << '/' << bots_.size() << '\n';
        Shutdown();
    }

    void Timeout()
    {
        if (shuttingDown_)
        {
            return;
        }
        exitCode_ = 4;
        std::cerr << "bot client timed out after 30 seconds\n";
        Shutdown();
    }

    void Fail(const std::string& reason)
    {
        if (shuttingDown_)
        {
            return;
        }
        exitCode_ = 3;
        std::cerr << "bot protocol failure: " << reason << '\n';
        Shutdown();
    }

    void Shutdown()
    {
        shuttingDown_ = true;
        timer_.cancel();
        for (BotState& bot : bots_)
        {
            bot.client->Close();
        }
    }

    boost::asio::io_context& io_;
    dxa::bot_client::BotClientOptions options_;
    boost::asio::steady_timer timer_;
    std::vector<BotState> bots_;
    std::size_t ticketsReceived_ = 0U;
    int exitCode_ = 3;
    bool shuttingDown_ = false;
};
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
        const auto parsed =
            dxa::bot_client::ParseBotClientOptions(arguments);
        if (!parsed.options.has_value())
        {
            std::cerr << parsed.error << '\n';
            return 2;
        }

        boost::asio::io_context io;
        const auto coordinator = BotCoordinator::Create(io, *parsed.options);
        coordinator->Start();
        io.run();
        return coordinator->ExitCode();
    }
    catch (const std::exception& error)
    {
        std::cerr << "bot client failed: " << error.what() << '\n';
        return 3;
    }
}
