#include "support/game_network_fixture.hpp"

#include <dxa/bot_client/BotCoordinator.hpp>
#include <dxa/client/NetworkClientController.hpp>
#include <dxa/game_common/ArenaFingerprint.hpp>
#include <dxa/protocol/AsioFramedConnection.hpp>
#include <dxa/protocol/GameTcpMessageCodec.hpp>
#include <dxa/protocol/GameUdpCodec.hpp>
#include <dxa/protocol/LobbyMessageCodec.hpp>
#include <dxa/simulation/ArenaMap.hpp>

#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>

namespace
{
using namespace std::chrono_literals;
using boost::asio::ip::tcp;
using boost::asio::ip::udp;

[[nodiscard]] dxa::simulation::MatchConfig BotPlayMatchConfig()
{
    dxa::simulation::MatchConfig config =
        dxa::simulation::DefaultMatchConfig();
    config.meleeNeutralCount = 0U;
    config.rangedNeutralCount = 0U;
    config.rifleLootCount = 0U;
    config.arcPulseLootCount = 0U;
    config.medKitLootCount = 0U;
    return config;
}

[[nodiscard]] dxa::simulation::MatchConfig TwentyFourPlayerBotConfig()
{
    dxa::simulation::MatchConfig config = BotPlayMatchConfig();
    config.suddenDeathTick = 6U;
    config.hardTimeoutTick = 12U;
    return config;
}

[[nodiscard]] dxa::bot_client::BotClientOptions PlayOptions(
    const std::uint16_t port,
    const dxa::protocol::RoomId room)
{
    dxa::bot_client::BotClientOptions options;
    options.port = port;
    options.room = room;
    options.play = true;
    return options;
}

template <typename Condition>
void RunIoUntil(
    boost::asio::io_context& io,
    Condition condition)
{
    bool timedOut = false;
    boost::asio::steady_timer timer{io};
    timer.expires_after(5s);
    timer.async_wait([&timedOut](const boost::system::error_code error) {
        if (!error)
        {
            timedOut = true;
        }
    });
    io.restart();
    while (!condition() && !timedOut)
    {
        if (io.run_one() == 0U)
        {
            break;
        }
    }
    timer.cancel();
    io.restart();
    while (io.poll_one() != 0U)
    {
    }
    if (!condition())
    {
        throw std::runtime_error{"bot coordinator test timed out"};
    }
}

class SilentLobbyServer
{
public:
    explicit SilentLobbyServer(boost::asio::io_context& io)
        : acceptor_{
              io,
              tcp::endpoint{
                  boost::asio::ip::make_address("127.0.0.1"),
                  0U}}
    {
        acceptor_.async_accept(
            [this](const boost::system::error_code error, tcp::socket socket) {
                if (!error)
                {
                    socket_.emplace(std::move(socket));
                }
            });
    }

    [[nodiscard]] std::uint16_t Port() const
    {
        return acceptor_.local_endpoint().port();
    }

private:
    tcp::acceptor acceptor_;
    std::optional<tcp::socket> socket_;
};

enum class GameHandshakeResponse
{
    RejectAuthentication,
    WelcomeWithoutUdpBind
};

class ScriptedBotServers
{
public:
    ScriptedBotServers(
        boost::asio::io_context& io,
        const GameHandshakeResponse response)
        : lobbyAcceptor_{
              io,
              tcp::endpoint{
                  boost::asio::ip::make_address("127.0.0.1"),
                  0U}},
          gameAcceptor_{
              io,
              tcp::endpoint{
                  boost::asio::ip::make_address("127.0.0.1"),
                  0U}},
          gameUdp_{
              io,
              udp::endpoint{
                  boost::asio::ip::make_address("127.0.0.1"),
                  0U}},
          response_{response}
    {
        AcceptLobby();
        AcceptGame();
        ReceiveGameUdp();
    }

    ~ScriptedBotServers()
    {
        if (lobby_)
        {
            lobby_->Close();
        }
        if (game_)
        {
            game_->Close();
        }
        boost::system::error_code ignored;
        lobbyAcceptor_.close(ignored);
        gameAcceptor_.close(ignored);
        gameUdp_.close(ignored);
    }

    [[nodiscard]] std::uint16_t LobbyPort() const
    {
        return lobbyAcceptor_.local_endpoint().port();
    }

    [[nodiscard]] bool GameHelloReceived() const noexcept
    {
        return gameHelloReceived_;
    }

    [[nodiscard]] bool UdpBindReceived() const noexcept
    {
        return udpBindReceived_;
    }

private:
    void AcceptLobby()
    {
        lobbyAcceptor_.async_accept([this](
            const boost::system::error_code error,
            tcp::socket socket) {
            if (error)
            {
                return;
            }
            lobby_ = dxa::protocol::AsioFramedConnection::Create(
                std::move(socket),
                [this](dxa::protocol::RawFrame frame) {
                    HandleLobby(std::move(frame));
                },
                [](const boost::system::error_code) {});
            lobby_->Start();
        });
    }

    void HandleLobby(dxa::protocol::RawFrame frame)
    {
        const auto decoded = dxa::protocol::DecodeClientMessage(
            frame.type,
            frame.payload);
        if (!decoded.message.has_value())
        {
            return;
        }
        if (const auto* hello = std::get_if<dxa::protocol::ClientHello>(
                &*decoded.message))
        {
            SendLobby(dxa::protocol::ServerMessage{
                dxa::protocol::ServerWelcome{
                    hello->requestId,
                    dxa::protocol::PlayerId{3U}}});
            return;
        }
        if (const auto* join = std::get_if<dxa::protocol::JoinRoomRequest>(
                &*decoded.message))
        {
            SendRoom(join->requestId, false);
            return;
        }
        if (const auto* ready = std::get_if<dxa::protocol::SetReadyRequest>(
                &*decoded.message);
            ready != nullptr && ready->ready)
        {
            SendRoom(ready->requestId, true);
            SendLobby(dxa::protocol::ServerMessage{
                dxa::protocol::MatchTicket{
                    ready->requestId,
                    dxa::protocol::MatchId{8U},
                    dxa::test::GameNetworkTicket(91U),
                    "127.0.0.1",
                    gameAcceptor_.local_endpoint().port(),
                    gameUdp_.local_endpoint().port(),
                    60U}});
        }
    }

    void SendRoom(const std::uint32_t requestId, const bool ready)
    {
        SendLobby(dxa::protocol::ServerMessage{
            dxa::protocol::RoomSnapshot{
                requestId,
                dxa::protocol::RoomId{1U},
                dxa::protocol::RoomState::Waiting,
                dxa::protocol::PlayerId{3U},
                {{dxa::protocol::PlayerId{3U}, ready}}}});
    }

    void SendLobby(const dxa::protocol::ServerMessage& message)
    {
        if (lobby_)
        {
            static_cast<void>(lobby_->Send(
                dxa::protocol::EncodeServerMessage(message)));
        }
    }

    void AcceptGame()
    {
        gameAcceptor_.async_accept([this](
            const boost::system::error_code error,
            tcp::socket socket) {
            if (error)
            {
                return;
            }
            game_ = dxa::protocol::AsioFramedConnection::Create(
                std::move(socket),
                [this](dxa::protocol::RawFrame frame) {
                    const auto decoded =
                        dxa::protocol::DecodeGameClientMessage(
                            frame.type,
                            frame.payload);
                    if (!decoded.message.has_value())
                    {
                        return;
                    }
                    gameHelloReceived_ = true;
                    if (response_ ==
                        GameHandshakeResponse::RejectAuthentication)
                    {
                        static_cast<void>(game_->Send(
                            dxa::protocol::EncodeGameServerMessage(
                                dxa::protocol::GameServerMessage{
                                    dxa::protocol::GameServerErrorMessage{
                                        dxa::protocol::GameServerErrorCode::
                                            AuthenticationFailed}})));
                        game_->CloseAfterFlush();
                        return;
                    }
                    const auto& hello = std::get<
                        dxa::protocol::GameClientHello>(*decoded.message);
                    const auto arena =
                        dxa::simulation::SurvivalArenaMapDefinition();
                    dxa::protocol::UdpSessionToken token;
                    token.fill(static_cast<std::byte>(0xA5U));
                    static_cast<void>(game_->Send(
                        dxa::protocol::EncodeGameServerMessage(
                            dxa::protocol::GameServerMessage{
                                dxa::protocol::GameServerWelcome{
                                    hello.match,
                                    hello.player,
                                    dxa::protocol::EntityId{0U},
                                    dxa::protocol::GameTickRate,
                                    dxa::protocol::SnapshotRate,
                                    arena.mapId,
                                    dxa::game_common::
                                        SurvivalArenaFingerprint(arena),
                                    dxa::protocol::ReplicationMode::FullState,
                                    token}})));
                },
                [](const boost::system::error_code) {});
            game_->Start();
        });
    }

    void ReceiveGameUdp()
    {
        gameUdp_.async_receive_from(
            boost::asio::buffer(gameUdpBuffer_),
            gameUdpRemote_,
            [this](
                const boost::system::error_code error,
                const std::size_t received) {
                if (!error)
                {
                    const auto decoded =
                        dxa::protocol::DecodeClientDatagram(std::span{
                            gameUdpBuffer_.data(),
                            received});
                    udpBindReceived_ = decoded.datagram.has_value()
                        && std::holds_alternative<dxa::protocol::UdpBind>(
                            *decoded.datagram);
                }
                if (gameUdp_.is_open())
                {
                    ReceiveGameUdp();
                }
            });
    }

    tcp::acceptor lobbyAcceptor_;
    tcp::acceptor gameAcceptor_;
    udp::socket gameUdp_;
    GameHandshakeResponse response_;
    std::shared_ptr<dxa::protocol::AsioFramedConnection> lobby_;
    std::shared_ptr<dxa::protocol::AsioFramedConnection> game_;
    udp::endpoint gameUdpRemote_;
    std::array<std::byte, dxa::protocol::MaxUdpDatagramBytes + 1U>
        gameUdpBuffer_{};
    bool gameHelloReceived_ = false;
    bool udpBindReceived_ = false;
};

void WaitForThirdBotReady(
    dxa::test::GameNetworkFixture& fixture,
    const dxa::test::ReadyNetworkRoom& room)
{
    fixture.RunUntil([&room] {
        const auto* snapshot =
            dxa::test::LatestLobbyMessage<dxa::protocol::RoomSnapshot>(
                *room.host);
        return snapshot != nullptr
            && snapshot->members.size() == 3U
            && std::all_of(
                snapshot->members.begin(),
                snapshot->members.end(),
                [](const auto& member) { return member.ready; });
    });
}

void WaitForReadyMemberCount(
    dxa::test::GameNetworkFixture& fixture,
    const dxa::test::ReadyNetworkRoom& room,
    const std::size_t expected)
{
    fixture.RunUntil([&room, expected] {
        const auto* snapshot =
            dxa::test::LatestLobbyMessage<dxa::protocol::RoomSnapshot>(
                *room.host);
        return snapshot != nullptr
            && snapshot->members.size() == expected
            && std::all_of(
                snapshot->members.begin(),
                snapshot->members.end(),
                [](const auto& member) { return member.ready; });
    });
}

template <typename Condition>
void PumpNetworkControllerUntil(
    dxa::test::GameNetworkFixture& fixture,
    dxa::client::NetworkClientController& controller,
    Condition condition)
{
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!condition())
    {
        fixture.BotIo().restart();
        while (fixture.BotIo().poll_one() != 0U)
        {
        }
        controller.FixedUpdate({});
        if (std::chrono::steady_clock::now() >= deadline)
        {
            throw std::runtime_error{
                "network controller integration test timed out"};
        }
        std::this_thread::sleep_for(1ms);
    }
}
} // namespace

TEST(GameServerIntegration, CompletesLobbyReservationAuthenticationAndDisconnectResult)
{
    dxa::test::GameNetworkFixture fixture;
    fixture.StartLobbyAndWorker();
    const dxa::test::ReadyNetworkRoom room =
        fixture.CreateReadyTwoPlayerRoom();
    fixture.StartMatch(room.host);
    const dxa::test::TicketPair tickets = fixture.WaitForTwoTickets(room);

    const auto host = fixture.Authenticate(tickets.host);
    const auto guest = fixture.Authenticate(tickets.guest);
    ASSERT_NE(nullptr, host->Welcome());
    ASSERT_NE(nullptr, guest->Welcome());
    host->BindUdp();
    guest->BindUdp();
    host->SendDestination({0.0F, 0.0F}, 1U);
    fixture.WaitForSnapshotAck(host, 1U);
    guest->CloseTcp();

    const dxa::protocol::GameMatchResult result = fixture.WaitForResult(host);
    EXPECT_TRUE(result.hasWinner);
    EXPECT_EQ(host->Actor(), result.winner);
    fixture.WaitForRoomCleanup(room);
}

TEST(GameServerIntegration, RejectsInvalidWrongPlayerAndReusedTickets)
{
    dxa::test::GameNetworkFixture fixture;
    fixture.StartLobbyAndWorker();
    const dxa::test::ReadyNetworkRoom room =
        fixture.CreateReadyTwoPlayerRoom();
    fixture.StartMatch(room.host);
    const dxa::test::TicketPair tickets = fixture.WaitForTwoTickets(room);

    dxa::test::GameTicketCredential invalid = tickets.host;
    invalid.ticket.ticket[0] = static_cast<std::byte>(
        std::to_integer<std::uint8_t>(invalid.ticket.ticket[0]) ^ 0xFFU);
    const auto invalidClient = fixture.Authenticate(invalid);
    ASSERT_NE(nullptr, invalidClient->Error());
    EXPECT_EQ(
        dxa::protocol::GameServerErrorCode::AuthenticationFailed,
        invalidClient->Error()->error);

    dxa::test::GameTicketCredential wrongPlayer = tickets.host;
    wrongPlayer.player = tickets.guest.player;
    const auto wrongClient = fixture.Authenticate(wrongPlayer);
    ASSERT_NE(nullptr, wrongClient->Error());
    EXPECT_EQ(
        dxa::protocol::GameServerErrorCode::AuthenticationFailed,
        wrongClient->Error()->error);

    const auto host = fixture.Authenticate(tickets.host);
    ASSERT_NE(nullptr, host->Welcome());
    const auto reused = fixture.Authenticate(tickets.host);
    ASSERT_NE(nullptr, reused->Error());
    EXPECT_EQ(
        dxa::protocol::GameServerErrorCode::AuthenticationFailed,
        reused->Error()->error);

    const auto guest = fixture.Authenticate(tickets.guest);
    ASSERT_NE(nullptr, guest->Welcome());
    guest->CloseTcp();
    static_cast<void>(fixture.WaitForResult(host));
    fixture.WaitForRoomCleanup(room);
}

TEST(GameServerIntegration, RejectsBadTokenRebindAndOldInputWithoutStealingBinding)
{
    dxa::test::GameNetworkFixture fixture;
    fixture.StartLobbyAndWorker();
    const dxa::test::ReadyNetworkRoom room =
        fixture.CreateReadyTwoPlayerRoom();
    fixture.StartMatch(room.host);
    const dxa::test::TicketPair tickets = fixture.WaitForTwoTickets(room);
    const auto host = fixture.Authenticate(tickets.host);
    const auto guest = fixture.Authenticate(tickets.guest);
    ASSERT_NE(nullptr, host->Welcome());
    ASSERT_NE(nullptr, guest->Welcome());
    host->BindUdp();
    guest->BindUdp();

    dxa::protocol::UdpSessionToken wrong = host->Welcome()->udpToken;
    wrong[0] = static_cast<std::byte>(
        std::to_integer<std::uint8_t>(wrong[0]) ^ 0xFFU);
    host->SendRogueBind(wrong);
    host->SendRogueBind(host->Welcome()->udpToken);
    host->SendDestination({0.0F, 0.0F}, 2U);
    host->SendDestination({20.0F, 20.0F}, 1U);
    host->SendDestination({20.0F, 20.0F}, 2U);

    fixture.WaitForSnapshotAck(host, 2U);
    guest->CloseTcp();
    static_cast<void>(fixture.WaitForResult(host));
    fixture.WaitForRoomCleanup(room);
}

TEST(GameServerIntegration, ActiveWorkerControlCloseCleansLobbyRoom)
{
    dxa::test::GameNetworkFixture fixture;
    fixture.StartLobbyAndWorker();
    const dxa::test::ReadyNetworkRoom room =
        fixture.CreateReadyTwoPlayerRoom();
    fixture.StartMatch(room.host);
    static_cast<void>(fixture.WaitForTwoTickets(room));

    fixture.StopWorker();
    fixture.RunUntil([&room] {
        const auto* error =
            dxa::test::LatestLobbyMessage<dxa::protocol::ErrorResponse>(
                *room.host);
        const auto* rooms =
            dxa::test::LatestLobbyMessage<dxa::protocol::RoomListResponse>(
                *room.host);
        return error != nullptr
            && error->error == dxa::protocol::LobbyError::MatchUnavailable
            && rooms != nullptr
            && rooms->rooms.empty();
    });
}

TEST(GameServerIntegration, ExpiresUnauthenticatedTicketsThroughLiveMatchTimer)
{
    dxa::test::ExpiringGameWorkerFixture fixture;

    const dxa::protocol::MatchFinished finished = fixture.WaitForCompletion();

    EXPECT_EQ(
        dxa::protocol::MatchCompletionReason::NoAuthenticatedPlayers,
        finished.reason);
    EXPECT_FALSE(finished.hasWinner);
    EXPECT_EQ(0U, finished.finishedTick);
}

TEST(GameServerIntegration, CompletesThreeSequentialReservationsOnOneWorker)
{
    dxa::test::GameNetworkFixture fixture;
    fixture.StartLobbyAndWorker();
    for (std::uint32_t match = 0U; match < 3U; ++match)
    {
        static_cast<void>(match);
        const dxa::test::ReadyNetworkRoom room =
            fixture.CreateReadyTwoPlayerRoom();
        fixture.StartMatch(room.host);
        const dxa::test::TicketPair tickets = fixture.WaitForTwoTickets(room);
        const auto host = fixture.Authenticate(tickets.host);
        const auto guest = fixture.Authenticate(tickets.guest);
        ASSERT_NE(nullptr, host->Welcome());
        ASSERT_NE(nullptr, guest->Welcome());
        guest->CloseTcp();
        static_cast<void>(fixture.WaitForResult(host));
        fixture.WaitForRoomCleanup(room);
    }

    const auto metrics = fixture.CompletedMetrics();
    ASSERT_EQ(3U, metrics.size());
    EXPECT_EQ(dxa::protocol::MatchId{1U}, metrics[0].match);
    EXPECT_EQ(dxa::protocol::MatchId{2U}, metrics[1].match);
    EXPECT_EQ(dxa::protocol::MatchId{3U}, metrics[2].match);
}

TEST(GameServerIntegration, PlayBotUsesSharedGameSessionUntilResult)
{
    dxa::test::GameNetworkFixture fixture{BotPlayMatchConfig()};
    fixture.StartLobbyAndWorker();
    const dxa::test::ReadyNetworkRoom room =
        fixture.CreateReadyTwoPlayerRoom();

    dxa::bot_client::BotCoordinator bot{
        fixture.BotIo(),
        PlayOptions(fixture.LobbyPort(), room.room)};
    bot.Start();

    WaitForThirdBotReady(fixture, room);
    fixture.StartMatch(room.host);
    const dxa::test::TicketPair tickets = fixture.WaitForTwoTickets(room);
    const auto host = fixture.Authenticate(tickets.host);
    const auto guest = fixture.Authenticate(tickets.guest);

    fixture.RunUntil([&bot] { return bot.GameAuthenticated(); });
    fixture.RunUntil([&bot] { return bot.SnapshotCount() >= 2U; });
    host->CloseTcp();
    guest->CloseTcp();
    fixture.RunUntil([&bot] { return bot.Result().has_value(); });

    EXPECT_EQ(0, bot.ExitCode());
    const auto report = bot.Report();
    ASSERT_EQ(1U, report.sessions.size());
    EXPECT_EQ(0, report.sessions.front().exitCode);
    EXPECT_EQ(bot.Result(), report.result);
    bot.Stop();
}

TEST(GameServerIntegration, PlayCoordinatorReportsEverySession)
{
    dxa::test::GameNetworkFixture fixture{TwentyFourPlayerBotConfig()};
    fixture.StartLobbyAndWorker();
    const dxa::test::ReadyNetworkRoom room =
        fixture.CreateReadyTwoPlayerRoom();
    room.guest->client->Close();
    WaitForReadyMemberCount(fixture, room, 1U);

    dxa::bot_client::BotClientOptions options =
        PlayOptions(fixture.LobbyPort(), room.room);
    options.count = 23U;
    dxa::bot_client::BotCoordinator bots{fixture.BotIo(), options};
    bots.Start();

    WaitForReadyMemberCount(fixture, room, 24U);
    fixture.StartMatch(room.host);
    fixture.RunUntil([&room] {
        return dxa::test::LatestLobbyMessage<dxa::protocol::MatchTicket>(
            *room.host) != nullptr;
    });
    const auto host = fixture.Authenticate(dxa::test::GameTicketCredential{
        *dxa::test::LatestLobbyMessage<dxa::protocol::MatchTicket>(*room.host),
        dxa::test::LatestLobbyMessage<dxa::protocol::ServerWelcome>(
            *room.host)->player});

    fixture.RunUntil([&bots] { return bots.Done(); });

    const dxa::bot_client::BotCoordinatorReport report = bots.Report();
    ASSERT_EQ(23U, report.sessions.size());
    EXPECT_TRUE(std::all_of(
        report.sessions.begin(),
        report.sessions.end(),
        [](const dxa::bot_client::BotSessionReport& session) {
            return session.exitCode == 0
                && session.snapshotsApplied >= 2U
                && session.receivedTcpBytes > 0U
                && session.receivedUdpBytes > 0U;
        }));
    ASSERT_TRUE(report.result.has_value());
    EXPECT_TRUE(std::all_of(
        report.sessions.begin(),
        report.sessions.end(),
        [&report](const dxa::bot_client::BotSessionReport& session) {
            return session.match == report.result->match;
        }));
    EXPECT_EQ(0, report.exitCode);
    fixture.RunUntil([&host] { return host->Result() != nullptr; });
    EXPECT_EQ(*host->Result(), *report.result);

    const auto serverMetrics = fixture.CompletedMetrics();
    ASSERT_EQ(1U, serverMetrics.size());
    EXPECT_EQ(report.result->match, serverMetrics.front().match);
    EXPECT_EQ(12U, serverMetrics.front().tickSamples.size());
    EXPECT_EQ(6U, serverMetrics.front().replicationSamples.size());
    EXPECT_GT(serverMetrics.front().tcpBytes, 0U);
    EXPECT_GT(serverMetrics.front().udpBytes, 0U);
    EXPECT_GT(serverMetrics.front().payloadBytes, 0U);
    EXPECT_GT(
        serverMetrics.front().udpBytes,
        serverMetrics.front().payloadBytes);
    ASSERT_FALSE(serverMetrics.front().replicationSamples.empty());
    EXPECT_EQ(
        24U,
        serverMetrics.front().replicationSamples.front().visibleActors);
    EXPECT_EQ(
        0U,
        serverMetrics.front().replicationSamples.front().visibleLoot);
    EXPECT_TRUE(
        serverMetrics.front().replicationSamples.front().keyframe);
}

TEST(GameServerIntegration, PlayBotPreservesLobbyOnlyTicketMode)
{
    dxa::test::GameNetworkFixture fixture{BotPlayMatchConfig()};
    fixture.StartLobbyAndWorker();
    const dxa::test::ReadyNetworkRoom room =
        fixture.CreateReadyTwoPlayerRoom();
    dxa::bot_client::BotClientOptions options =
        PlayOptions(fixture.LobbyPort(), room.room);
    options.play = false;
    dxa::bot_client::BotCoordinator bot{fixture.BotIo(), options};
    bot.Start();

    WaitForThirdBotReady(fixture, room);
    fixture.StartMatch(room.host);
    fixture.RunUntil([&bot] { return bot.Done(); });

    EXPECT_EQ(0, bot.ExitCode());
    EXPECT_FALSE(bot.GameAuthenticated());
    EXPECT_EQ(0U, bot.SnapshotCount());
    EXPECT_FALSE(bot.Result().has_value());
    const auto report = bot.Report();
    ASSERT_EQ(1U, report.sessions.size());
    EXPECT_EQ(0, report.sessions.front().exitCode);
}

TEST(GameServerIntegration, PlayBotReportsLobbyError)
{
    dxa::test::GameNetworkFixture fixture{BotPlayMatchConfig()};
    fixture.StartLobbyAndWorker();
    dxa::bot_client::BotCoordinator bot{
        fixture.BotIo(),
        PlayOptions(fixture.LobbyPort(), dxa::protocol::RoomId{999U})};
    bot.Start();

    fixture.RunUntil([&bot] { return bot.Done(); });

    EXPECT_EQ(3, bot.ExitCode());
    EXPECT_FALSE(bot.GameAuthenticated());
    EXPECT_FALSE(bot.Result().has_value());
    const auto report = bot.Report();
    ASSERT_EQ(1U, report.sessions.size());
    EXPECT_EQ(3, report.sessions.front().exitCode);
}

TEST(GameServerIntegration, PlayBotReportsGameAuthenticationFailure)
{
    boost::asio::io_context io;
    ScriptedBotServers servers{
        io,
        GameHandshakeResponse::RejectAuthentication};
    dxa::bot_client::BotCoordinator bot{
        io,
        PlayOptions(servers.LobbyPort(), dxa::protocol::RoomId{1U})};
    bot.Start();

    RunIoUntil(io, [&bot] { return bot.Done(); });

    EXPECT_TRUE(servers.GameHelloReceived());
    EXPECT_EQ(3, bot.ExitCode());
    EXPECT_FALSE(bot.GameAuthenticated());
    EXPECT_FALSE(bot.Result().has_value());
    const auto report = bot.Report();
    ASSERT_EQ(1U, report.sessions.size());
    EXPECT_EQ(3, report.sessions.front().exitCode);
    EXPECT_EQ(dxa::protocol::MatchId{8U}, report.sessions.front().match);
}

TEST(GameServerIntegration, PlayBotTimesOutAfterGameAuthentication)
{
    boost::asio::io_context io;
    ScriptedBotServers servers{
        io,
        GameHandshakeResponse::WelcomeWithoutUdpBind};
    dxa::bot_client::BotCoordinator bot{
        io,
        PlayOptions(servers.LobbyPort(), dxa::protocol::RoomId{1U}),
        dxa::bot_client::BotCoordinatorTimeouts{1s, 100ms}};
    bot.Start();

    RunIoUntil(io, [&bot] { return bot.Done(); });

    EXPECT_TRUE(servers.GameHelloReceived());
    EXPECT_TRUE(servers.UdpBindReceived());
    EXPECT_TRUE(bot.GameAuthenticated());
    EXPECT_EQ(4, bot.ExitCode());
    EXPECT_EQ(0U, bot.SnapshotCount());
    EXPECT_FALSE(bot.Result().has_value());
    const auto report = bot.Report();
    ASSERT_EQ(1U, report.sessions.size());
    EXPECT_EQ(4, report.sessions.front().exitCode);
}

TEST(GameServerIntegration, PlayBotUsesTimeoutExitCode)
{
    boost::asio::io_context io;
    SilentLobbyServer server{io};
    dxa::bot_client::BotCoordinator bot{
        io,
        PlayOptions(server.Port(), dxa::protocol::RoomId{1U}),
        dxa::bot_client::BotCoordinatorTimeouts{20ms, 100ms}};
    bot.Start();

    RunIoUntil(io, [&bot] { return bot.Done(); });

    EXPECT_EQ(4, bot.ExitCode());
    EXPECT_FALSE(bot.GameAuthenticated());
}

TEST(GameServerIntegration, PlayBotStopIsIdempotentBeforeWelcome)
{
    boost::asio::io_context io;
    SilentLobbyServer server{io};
    dxa::bot_client::BotCoordinator bot{
        io,
        PlayOptions(server.Port(), dxa::protocol::RoomId{1U})};
    bot.Start();

    bot.Stop();
    bot.Stop();

    EXPECT_TRUE(bot.Done());
    EXPECT_EQ(3, bot.ExitCode());
    const auto report = bot.Report();
    ASSERT_EQ(1U, report.sessions.size());
    EXPECT_EQ(3, report.sessions.front().exitCode);
}

TEST(GameServerIntegration, NetworkHostControllerRunsUntilGameResult)
{
    dxa::test::GameNetworkFixture fixture{BotPlayMatchConfig()};
    fixture.StartLobbyAndWorker();
    dxa::client::NetworkClientController controller{
        dxa::client::NetworkClientOptions{
            "127.0.0.1",
            fixture.LobbyPort(),
            2U}};
    controller.Start();
    PumpNetworkControllerUntil(
        fixture,
        controller,
        [&controller] { return controller.Room().has_value(); });

    dxa::bot_client::BotCoordinator bot{
        fixture.BotIo(),
        PlayOptions(fixture.LobbyPort(), *controller.Room())};
    bot.Start();
    PumpNetworkControllerUntil(
        fixture,
        controller,
        [&] {
            return bot.GameAuthenticated()
                && bot.SnapshotCount() >= 2U
                && controller.SnapshotCount() >= 2U;
        });

    const dxa::engine::RuntimeSceneFrame scene = controller.SampleScene();
    EXPECT_GT(
        std::count_if(
            scene.players.begin(),
            scene.players.end(),
            [](const auto& player) { return player.active; }),
        0U);
    EXPECT_GT(scene.zoneRadius, 0.0F);

    bot.Stop();
    PumpNetworkControllerUntil(
        fixture,
        controller,
        [&controller] { return controller.Result().has_value(); });

    EXPECT_TRUE(controller.Result()->hasWinner);
    EXPECT_NO_THROW(controller.FixedUpdate({}));
    controller.Stop();
}
