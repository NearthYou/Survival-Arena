#include <dxa/bot_client/BotCoordinator.hpp>

#include <dxa/game_client/GameSession.hpp>
#include <dxa/game_common/ArenaFingerprint.hpp>
#include <dxa/lobby_client/LobbyClient.hpp>
#include <dxa/simulation/ArenaMap.hpp>

#include <boost/asio/steady_timer.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <ratio>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace dxa::bot_client
{
namespace
{
constexpr std::array<dxa::simulation::Vec2, 4U> Destinations{
    dxa::simulation::Vec2{0.0F, 0.0F},
    dxa::simulation::Vec2{20.0F, 20.0F},
    dxa::simulation::Vec2{-20.0F, 20.0F},
    dxa::simulation::Vec2{-20.0F, -20.0F}};

[[nodiscard]] std::chrono::steady_clock::time_point GameTickDeadline(
    const std::chrono::steady_clock::time_point epoch,
    const std::uint64_t ordinal)
{
    using ExactTickDuration =
        std::chrono::duration<std::uint64_t, std::ratio<1, 30>>;
    return epoch + std::chrono::duration_cast<
        std::chrono::steady_clock::duration>(ExactTickDuration{ordinal});
}

struct BotState
{
    std::shared_ptr<dxa::lobby_client::LobbyClient> client;
    std::optional<dxa::protocol::PlayerId> player;
    bool readySent = false;
    bool ticketReceived = false;
};
} // namespace

struct BotCoordinator::Impl final
    : public std::enable_shared_from_this<BotCoordinator::Impl>
{
    Impl(
        boost::asio::io_context& sourceIo,
        BotClientOptions sourceOptions,
        BotCoordinatorTimeouts sourceTimeouts)
        : io{sourceIo},
          options{std::move(sourceOptions)},
          timeouts{sourceTimeouts},
          lobbyTimer{io},
          gameTimer{io},
          gameTimeoutTimer{io}
    {
        if (options.count == 0U || options.count > 23U)
        {
            throw std::invalid_argument{"bot count must be between 1 and 23"};
        }
        if (options.play && options.count != 1U)
        {
            throw std::invalid_argument{"bot play mode requires one bot"};
        }
        if (options.room.value == 0U)
        {
            throw std::invalid_argument{"bot room must be nonzero"};
        }
        if (timeouts.lobby <= std::chrono::milliseconds::zero()
            || timeouts.game <= std::chrono::milliseconds::zero())
        {
            throw std::invalid_argument{"bot timeouts must be positive"};
        }
    }

    void Start()
    {
        if (started || shuttingDown)
        {
            throw std::logic_error{"bot coordinator can start only once"};
        }
        started = true;
        bots.resize(options.count);
        const std::weak_ptr<Impl> weak = shared_from_this();
        for (std::size_t index = 0U; index < bots.size(); ++index)
        {
            bots[index].client =
                dxa::lobby_client::LobbyClient::Create(io);
            bots[index].client->AsyncConnect(
                options.host,
                options.port,
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
        lobbyTimer.expires_after(timeouts.lobby);
        lobbyTimer.async_wait([weak](const boost::system::error_code error) {
            if (!error)
            {
                if (const auto self = weak.lock())
                {
                    self->Timeout("lobby");
                }
            }
        });
    }

    void Connected(const std::size_t index)
    {
        if (shuttingDown)
        {
            return;
        }
        try
        {
            if (!bots.at(index).client->Hello())
            {
                Fail("lobby hello send failed");
            }
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
        if (shuttingDown)
        {
            return;
        }
        try
        {
            BotState& bot = bots.at(index);
            if (const auto* welcome =
                    std::get_if<dxa::protocol::ServerWelcome>(&message))
            {
                if (bot.player.has_value())
                {
                    Fail("duplicate lobby welcome");
                    return;
                }
                bot.player = welcome->player;
                if (!bot.client->JoinRoom(options.room))
                {
                    Fail("lobby join send failed");
                }
                return;
            }
            if (const auto* room =
                    std::get_if<dxa::protocol::RoomSnapshot>(&message))
            {
                if (!bot.player.has_value())
                {
                    Fail("room snapshot before lobby welcome");
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
                    if (!bot.client->SetReady(true))
                    {
                        Fail("lobby ready send failed");
                    }
                }
                return;
            }
            if (const auto* ticket =
                    std::get_if<dxa::protocol::MatchTicket>(&message))
            {
                if (!bot.player.has_value())
                {
                    Fail("match ticket before lobby welcome");
                    return;
                }
                if (bot.ticketReceived)
                {
                    return;
                }
                bot.ticketReceived = true;
                ++ticketsReceived;
                if (options.play)
                {
                    StartGame(*bot.player, *ticket);
                }
                else if (ticketsReceived == bots.size())
                {
                    FinishLobby();
                }
                return;
            }
            if (const auto* error =
                    std::get_if<dxa::protocol::ErrorResponse>(&message))
            {
                Fail("lobby server error " + std::to_string(
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
        if (shuttingDown)
        {
            return;
        }
        const BotState& bot = bots.at(index);
        if (options.play || !bot.ticketReceived)
        {
            Fail("lobby connection closed " +
                 std::to_string(error.value()));
        }
    }

    void StartGame(
        const dxa::protocol::PlayerId player,
        const dxa::protocol::MatchTicket& ticket)
    {
        lobbyTimer.cancel();
        const dxa::simulation::ArenaMapDefinition arena =
            dxa::simulation::SurvivalArenaMapDefinition();
        gameSession = std::make_unique<dxa::game_client::GameSession>(
            dxa::simulation::BuildSurvivalArenaNavMesh());
        gameSession->Start(dxa::game_client::GameSessionStart{
            player,
            ticket,
            arena.mapId,
            dxa::game_common::SurvivalArenaFingerprint(arena)});

        const std::weak_ptr<Impl> weak = shared_from_this();
        gameTimeoutTimer.expires_after(timeouts.game);
        gameTimeoutTimer.async_wait(
            [weak](const boost::system::error_code error) {
                if (!error)
                {
                    if (const auto self = weak.lock())
                    {
                        self->Timeout("game");
                    }
                }
            });
        gameEpoch = std::chrono::steady_clock::now();
        gameTickOrdinal = 1U;
        nextGameTick = GameTickDeadline(gameEpoch, gameTickOrdinal);
        ScheduleGameTick();
    }

    void ScheduleGameTick()
    {
        if (shuttingDown || !gameSession)
        {
            return;
        }
        const std::weak_ptr<Impl> weak = shared_from_this();
        gameTimer.expires_at(nextGameTick);
        gameTimer.async_wait([weak](const boost::system::error_code error) {
            if (error)
            {
                return;
            }
            if (const auto self = weak.lock())
            {
                self->GameTick();
            }
        });
    }

    void GameTick()
    {
        if (shuttingDown || !gameSession)
        {
            return;
        }
        try
        {
            gameSession->FixedUpdate();
            snapshotCount.store(gameSession->SnapshotCount());
            const dxa::game_client::GameSessionState state =
                gameSession->State();
            if (state == dxa::game_client::GameSessionState::BindingUdp
                || state == dxa::game_client::GameSessionState::Synchronizing
                || state == dxa::game_client::GameSessionState::Running)
            {
                gameAuthenticated.store(true);
            }
            if (state == dxa::game_client::GameSessionState::Running)
            {
                ++runningTicks;
                if ((runningTicks - 1U) % 90U == 0U)
                {
                    if (!gameSession->SetDestination(
                            Destinations[nextDestination]))
                    {
                        Fail("bot destination was rejected");
                        return;
                    }
                    nextDestination =
                        (nextDestination + 1U) % Destinations.size();
                }
            }
            else if (state == dxa::game_client::GameSessionState::Finished)
            {
                const auto value = gameSession->Result();
                if (!value.has_value())
                {
                    Fail("game finished without result");
                    return;
                }
                FinishGame(*value);
                return;
            }
            else if (state ==
                         dxa::game_client::GameSessionState::ProtocolError
                     || state == dxa::game_client::GameSessionState::Closed)
            {
                Fail("game session failed");
                return;
            }
        }
        catch (const std::exception& error)
        {
            Fail(error.what());
            return;
        }

        ++gameTickOrdinal;
        nextGameTick = GameTickDeadline(gameEpoch, gameTickOrdinal);
        ScheduleGameTick();
    }

    void FinishLobby()
    {
        exitCode.store(0);
        std::cout << "bot tickets received: "
                  << ticketsReceived << '/' << bots.size() << '\n';
        Shutdown();
    }

    void FinishGame(const dxa::protocol::GameMatchResult& value)
    {
        {
            std::scoped_lock lock{resultMutex};
            result = value;
        }
        exitCode.store(0);
        std::cout << "bot match finished: match="
                  << value.match.value << '\n';
        Shutdown();
    }

    void Timeout(const std::string& phase)
    {
        if (shuttingDown)
        {
            return;
        }
        exitCode.store(4);
        std::cerr << "bot " << phase << " timed out\n";
        Shutdown();
    }

    void Fail(const std::string& reason)
    {
        if (shuttingDown)
        {
            return;
        }
        exitCode.store(3);
        std::cerr << "bot protocol failure: " << reason << '\n';
        Shutdown();
    }

    void Shutdown()
    {
        if (shuttingDown)
        {
            return;
        }
        shuttingDown = true;
        lobbyTimer.cancel();
        gameTimer.cancel();
        gameTimeoutTimer.cancel();
        if (gameSession)
        {
            gameSession->Stop();
        }
        for (BotState& bot : bots)
        {
            if (bot.client)
            {
                bot.client->Close();
            }
        }
        done.store(true);
    }

    boost::asio::io_context& io;
    BotClientOptions options;
    BotCoordinatorTimeouts timeouts;
    boost::asio::steady_timer lobbyTimer;
    boost::asio::steady_timer gameTimer;
    boost::asio::steady_timer gameTimeoutTimer;
    std::vector<BotState> bots;
    std::unique_ptr<dxa::game_client::GameSession> gameSession;
    std::chrono::steady_clock::time_point gameEpoch;
    std::chrono::steady_clock::time_point nextGameTick;
    std::size_t ticketsReceived = 0U;
    std::size_t nextDestination = 0U;
    std::uint64_t runningTicks = 0U;
    std::uint64_t gameTickOrdinal = 0U;
    std::atomic<bool> gameAuthenticated{false};
    std::atomic<std::uint64_t> snapshotCount{0U};
    mutable std::mutex resultMutex;
    std::optional<dxa::protocol::GameMatchResult> result;
    std::atomic<int> exitCode{3};
    std::atomic<bool> done{false};
    bool started = false;
    bool shuttingDown = false;
};

BotCoordinator::BotCoordinator(
    boost::asio::io_context& io,
    BotClientOptions options,
    const BotCoordinatorTimeouts timeouts)
    : impl_{std::make_shared<Impl>(io, std::move(options), timeouts)}
{
}

BotCoordinator::~BotCoordinator()
{
    Stop();
}

void BotCoordinator::Start()
{
    impl_->Start();
}

bool BotCoordinator::GameAuthenticated() const noexcept
{
    return impl_->gameAuthenticated.load();
}

std::uint64_t BotCoordinator::SnapshotCount() const noexcept
{
    return impl_->snapshotCount.load();
}

std::optional<dxa::protocol::GameMatchResult> BotCoordinator::Result() const
{
    std::scoped_lock lock{impl_->resultMutex};
    return impl_->result;
}

int BotCoordinator::ExitCode() const noexcept
{
    return impl_->exitCode.load();
}

bool BotCoordinator::Done() const noexcept
{
    return impl_->done.load();
}

void BotCoordinator::Stop()
{
    impl_->Shutdown();
}
} // namespace dxa::bot_client
