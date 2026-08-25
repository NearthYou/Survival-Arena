#include <dxa/bot_client/BotCoordinator.hpp>

#include <dxa/game_client/GameNetworkRuntime.hpp>
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
#include <limits>
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
    std::unique_ptr<dxa::game_client::GameSession> gameSession;
    BotSessionReport report;
    bool readySent = false;
    bool ticketReceived = false;
    bool finished = false;
    bool synchronizationReported = false;
    std::uint32_t destinationState = 0U;
    std::uint64_t runningTicks = 0U;
};

struct CanonicalArena
{
    dxa::simulation::NavMesh navMesh;
    std::uint32_t mapId = 0U;
    std::uint32_t fingerprint = 0U;
};

[[nodiscard]] CanonicalArena BuildCanonicalArena()
{
    dxa::simulation::ArenaMapDefinition definition =
        dxa::simulation::SurvivalArenaMapDefinition();
    const std::uint32_t mapId = definition.mapId;
    const std::uint32_t fingerprint =
        dxa::game_common::SurvivalArenaFingerprint(definition);
    dxa::simulation::NavMesh navMesh = dxa::simulation::NavMesh::Build(
        std::move(definition.vertices),
        std::move(definition.triangles),
        definition.gridCellSize);
    return {
        std::move(navMesh),
        mapId,
        fingerprint};
}

[[nodiscard]] bool IsAuthenticatedState(
    const dxa::game_client::GameSessionState state) noexcept
{
    return state == dxa::game_client::GameSessionState::BindingUdp
        || state == dxa::game_client::GameSessionState::Synchronizing
        || state == dxa::game_client::GameSessionState::Running
        || state == dxa::game_client::GameSessionState::Finished;
}

[[nodiscard]] std::uint32_t DestinationSeed(
    const dxa::protocol::MatchId match,
    const dxa::protocol::PlayerId player) noexcept
{
    return static_cast<std::uint32_t>(match.value)
        ^ static_cast<std::uint32_t>(match.value >> 32U)
        ^ player.value * 0x9E3779B9U;
}

[[nodiscard]] std::size_t NextDestination(BotState& bot) noexcept
{
    bot.destinationState = bot.destinationState * 1664525U + 1013904223U;
    return static_cast<std::size_t>(
        bot.destinationState % Destinations.size());
}
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
        if (options.play)
        {
            arena.emplace(BuildCanonicalArena());
            gameRuntime =
                std::make_shared<dxa::game_client::GameNetworkRuntime>();
            if (!gameRuntime->Start())
            {
                throw std::runtime_error{"bot game network runtime failed to start"};
            }
        }
        bots.resize(options.count);
        for (BotState& bot : bots)
        {
            bot.report.exitCode = 3;
        }
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
                {
                    std::scoped_lock lock{reportMutex};
                    bot.report.player = welcome->player;
                }
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
                {
                    std::scoped_lock lock{reportMutex};
                    bot.report.match = ticket->match;
                }
                if (options.play)
                {
                    StartGame(index, *bot.player, *ticket);
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
        const std::size_t index,
        const dxa::protocol::PlayerId player,
        const dxa::protocol::MatchTicket& ticket)
    {
        if (activeMatch.has_value() && *activeMatch != ticket.match)
        {
            Fail("bot tickets disagree on match identity");
            return;
        }
        activeMatch = ticket.match;
        std::cout << "bot match assigned: match="
                  << ticket.match.value << '\n' << std::flush;
        lobbyTimer.cancel();
        if (!gameRuntime || !arena.has_value())
        {
            Fail("bot game network runtime is absent");
            return;
        }

        BotState& bot = bots.at(index);
        bot.destinationState = DestinationSeed(ticket.match, player);
        bot.gameSession = std::make_unique<dxa::game_client::GameSession>(
            arena->navMesh,
            gameRuntime);
        bot.gameSession->Start(dxa::game_client::GameSessionStart{
            player,
            ticket,
            arena->mapId,
            arena->fingerprint});

        if (gameTickStarted)
        {
            return;
        }
        gameTickStarted = true;
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
        if (shuttingDown || !gameTickStarted)
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
        if (shuttingDown || !gameTickStarted)
        {
            return;
        }
        try
        {
            bool everySessionAuthenticated = !bots.empty();
            bool everySessionStarted = !bots.empty();
            std::uint64_t minimumSnapshots =
                std::numeric_limits<std::uint64_t>::max();
            for (std::size_t index = 0U; index < bots.size(); ++index)
            {
                BotState& bot = bots[index];
                if (!bot.gameSession)
                {
                    everySessionAuthenticated = false;
                    everySessionStarted = false;
                    minimumSnapshots = 0U;
                    continue;
                }

                if (!bot.finished)
                {
                    bot.gameSession->FixedUpdate();
                }
                const dxa::game_common::GameSessionMetrics metrics =
                    bot.gameSession->Metrics();
                const std::uint64_t receivedSnapshots =
                    metrics.snapshotsApplied;
                minimumSnapshots = std::min(
                    minimumSnapshots,
                    receivedSnapshots);
                {
                    std::scoped_lock lock{reportMutex};
                    bot.report.snapshotsApplied = receivedSnapshots;
                    bot.report.receivedTcpBytes =
                        metrics.traffic.tcpReceivedBytes;
                    bot.report.receivedUdpBytes =
                        metrics.traffic.udpReceivedBytes;
                    bot.report.discardedSnapshots =
                        metrics.snapshotsDiscarded;
                    bot.report.keyframeRequests = metrics.keyframeRequests;
                }
                if (receivedSnapshots >= 2U
                    && !bot.synchronizationReported)
                {
                    bot.synchronizationReported = true;
                    std::cout << "bot match synchronized: match="
                              << activeMatch->value
                              << " player=" << bot.player->value
                              << " snapshots=" << receivedSnapshots
                              << '\n' << std::flush;
                }

                const dxa::game_client::GameSessionState state =
                    bot.gameSession->State();
                everySessionAuthenticated =
                    everySessionAuthenticated
                    && IsAuthenticatedState(state);
                if (bot.finished)
                {
                    continue;
                }

                if (state == dxa::game_client::GameSessionState::Running)
                {
                    ++bot.runningTicks;
                    if ((bot.runningTicks - 1U) % 90U == 0U
                        && !bot.gameSession->SetDestination(
                            Destinations[NextDestination(bot)]))
                    {
                        Fail("bot destination was rejected");
                        return;
                    }
                }
                else if (state ==
                             dxa::game_client::GameSessionState::Finished)
                {
                    const auto value = bot.gameSession->Result();
                    if (!value.has_value())
                    {
                        Fail("game finished without result");
                        return;
                    }
                    FinishGame(index, *value);
                    if (shuttingDown)
                    {
                        return;
                    }
                }
                else if (state ==
                             dxa::game_client::GameSessionState::ProtocolError
                         || state ==
                             dxa::game_client::GameSessionState::Closed)
                {
                    Fail("game session failed");
                    return;
                }
            }
            gameAuthenticated.store(everySessionAuthenticated);
            snapshotCount.store(
                everySessionStarted ? minimumSnapshots : 0U);
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
        {
            std::scoped_lock lock{reportMutex};
            for (BotState& bot : bots)
            {
                bot.report.exitCode = 0;
            }
        }
        exitCode.store(0);
        std::cout << "bot tickets received: "
                  << ticketsReceived << '/' << bots.size() << '\n';
        Shutdown();
    }

    void FinishGame(
        const std::size_t index,
        const dxa::protocol::GameMatchResult& value)
    {
        BotState& bot = bots.at(index);
        bool mismatch = false;
        {
            std::scoped_lock lock{reportMutex};
            mismatch = bot.report.match != value.match
                || (result.has_value() && *result != value);
            if (!mismatch)
            {
                if (!result.has_value())
                {
                    result = value;
                }
                bot.report.exitCode = 0;
            }
        }
        if (mismatch)
        {
            Fail("bot game results disagree");
            return;
        }
        if (bot.finished)
        {
            return;
        }
        bot.finished = true;
        ++finishedSessions;
        if (finishedSessions != bots.size())
        {
            return;
        }
        gameAuthenticated.store(true);
        exitCode.store(0);
        std::cout << "bot match finished: match="
                  << value.match.value
                  << " sessions=" << finishedSessions << '\n';
        Shutdown();
    }

    void SetUnfinishedExitCode(const int code)
    {
        std::scoped_lock lock{reportMutex};
        for (BotState& bot : bots)
        {
            if (!bot.finished)
            {
                bot.report.exitCode = code;
            }
        }
    }

    void Timeout(const std::string& phase)
    {
        if (shuttingDown)
        {
            return;
        }
        SetUnfinishedExitCode(4);
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
        SetUnfinishedExitCode(3);
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
        for (BotState& bot : bots)
        {
            if (bot.gameSession)
            {
                bot.gameSession->Stop();
            }
            if (bot.client)
            {
                bot.client->Close();
            }
        }
        if (gameRuntime)
        {
            gameRuntime->Stop();
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
    std::optional<CanonicalArena> arena;
    std::shared_ptr<dxa::game_client::GameNetworkRuntime> gameRuntime;
    std::chrono::steady_clock::time_point gameEpoch;
    std::chrono::steady_clock::time_point nextGameTick;
    std::size_t ticketsReceived = 0U;
    std::size_t finishedSessions = 0U;
    std::uint64_t gameTickOrdinal = 0U;
    std::atomic<bool> gameAuthenticated{false};
    std::atomic<std::uint64_t> snapshotCount{0U};
    std::optional<dxa::protocol::MatchId> activeMatch;
    mutable std::mutex reportMutex;
    std::optional<dxa::protocol::GameMatchResult> result;
    std::atomic<int> exitCode{3};
    std::atomic<bool> done{false};
    bool started = false;
    bool gameTickStarted = false;
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
    std::scoped_lock lock{impl_->reportMutex};
    return impl_->result;
}

BotCoordinatorReport BotCoordinator::Report() const
{
    BotCoordinatorReport report;
    std::scoped_lock lock{impl_->reportMutex};
    report.sessions.reserve(impl_->bots.size());
    for (const BotState& bot : impl_->bots)
    {
        report.sessions.push_back(bot.report);
    }
    report.result = impl_->result;
    report.exitCode = impl_->exitCode.load();
    return report;
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
