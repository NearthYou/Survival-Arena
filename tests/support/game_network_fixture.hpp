#pragma once

#include "lobby_network_fixture.hpp"

#include <dxa/game_server/GameServer.hpp>
#include <dxa/protocol/AsioFramedConnection.hpp>
#include <dxa/protocol/GameTcpMessageCodec.hpp>
#include <dxa/protocol/GameUdpCodec.hpp>
#include <dxa/protocol/WorkerControlMessageCodec.hpp>

#include <boost/asio.hpp>

#if defined(_WIN32)
#include <dxa/bot_client/BotCoordinator.hpp>
#include <dxa/client/NetworkClientController.hpp>
#include <dxa/engine/EngineApp.hpp>
#include <dxa/game_server/UdpTokenSource.hpp>

#include <gtest/gtest.h>

#include <Windows.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#if defined(_WIN32)
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#endif

namespace dxa::test
{
[[nodiscard]] inline dxa::protocol::MatchTicketValue GameNetworkTicket(
    const std::uint8_t seed)
{
    dxa::protocol::MatchTicketValue ticket;
    for (std::size_t index = 0U; index < ticket.size(); ++index)
    {
        ticket[index] = static_cast<std::byte>(
            static_cast<std::uint8_t>(seed + index));
    }
    return ticket;
}

template <typename Message>
[[nodiscard]] const Message* LatestLobbyMessage(
    const LobbyClientProbe& probe)
{
    for (auto message = probe.messages.rbegin();
         message != probe.messages.rend();
         ++message)
    {
        if (const auto* value = std::get_if<Message>(&*message))
        {
            return value;
        }
    }
    return nullptr;
}

struct ReadyNetworkRoom
{
    std::shared_ptr<LobbyClientProbe> host;
    std::shared_ptr<LobbyClientProbe> guest;
    dxa::protocol::RoomId room;
};

struct GameTicketCredential
{
    dxa::protocol::MatchTicket ticket;
    dxa::protocol::PlayerId player;
};

struct TicketPair
{
    GameTicketCredential host;
    GameTicketCredential guest;
};

class GameClientProbe final
    : public std::enable_shared_from_this<GameClientProbe>
{
public:
    [[nodiscard]] static std::shared_ptr<GameClientProbe> Connect(
        boost::asio::io_context& io,
        GameTicketCredential credential)
    {
        auto probe = std::shared_ptr<GameClientProbe>{
            new GameClientProbe{io, std::move(credential)}};
        probe->ConnectTcp();
        return probe;
    }

    ~GameClientProbe()
    {
        Close();
    }

    GameClientProbe(const GameClientProbe&) = delete;
    GameClientProbe& operator=(const GameClientProbe&) = delete;

    void BindUdp()
    {
        const dxa::protocol::GameServerWelcome* welcome = Welcome();
        if (welcome == nullptr)
        {
            throw std::logic_error{"game probe has no welcome"};
        }
        StartUdpReceive();
        SendUdp(dxa::protocol::ClientDatagram{
            dxa::protocol::UdpBind{
                welcome->match,
                welcome->player,
                welcome->udpToken}});
    }

    void SendDestination(
        const dxa::protocol::NetworkVec2 destination,
        const std::uint32_t sequence)
    {
        const dxa::protocol::GameServerWelcome* welcome = Welcome();
        if (welcome == nullptr)
        {
            throw std::logic_error{"game probe has no welcome"};
        }
        dxa::protocol::ClientInput input;
        input.match = welcome->match;
        input.player = welcome->player;
        input.token = welcome->udpToken;
        input.inputSequence = sequence;
        input.moveDestination = destination;
        input.hasMoveDestination = true;
        SendUdp(dxa::protocol::ClientDatagram{input});
    }

    void SendRogueBind(const dxa::protocol::UdpSessionToken& token)
    {
        const dxa::protocol::GameServerWelcome* welcome = Welcome();
        if (welcome == nullptr)
        {
            throw std::logic_error{"game probe has no welcome"};
        }
        boost::asio::ip::udp::socket rogue{io_};
        rogue.open(gameUdpEndpoint_.protocol());
        rogue.bind(boost::asio::ip::udp::endpoint{
            gameUdpEndpoint_.protocol(),
            0U});
        const auto encoded = dxa::protocol::EncodeClientDatagram(
            dxa::protocol::ClientDatagram{
                dxa::protocol::UdpBind{
                    welcome->match,
                    welcome->player,
                    token}});
        rogue.send_to(boost::asio::buffer(encoded.bytes), gameUdpEndpoint_);
    }

    void CloseTcp()
    {
        if (tcp_)
        {
            tcp_->Close();
        }
    }

    void Close()
    {
        CloseTcp();
        boost::system::error_code ignored;
        udp_.cancel(ignored);
        udp_.close(ignored);
    }

    [[nodiscard]] const dxa::protocol::GameServerWelcome* Welcome() const
    {
        return LatestTcp<dxa::protocol::GameServerWelcome>();
    }

    [[nodiscard]] const dxa::protocol::GameServerErrorMessage* Error() const
    {
        return LatestTcp<dxa::protocol::GameServerErrorMessage>();
    }

    [[nodiscard]] const dxa::protocol::GameMatchResult* Result() const
    {
        return LatestTcp<dxa::protocol::GameMatchResult>();
    }

    [[nodiscard]] bool TcpClosed() const noexcept
    {
        return tcpClosed_.has_value();
    }

    [[nodiscard]] std::optional<std::uint32_t> LatestSnapshotAck() const
    {
        for (auto datagram = datagrams_.rbegin();
             datagram != datagrams_.rend();
             ++datagram)
        {
            if (const auto* fragment =
                    std::get_if<dxa::protocol::SnapshotFragment>(&*datagram))
            {
                return fragment->ackInputSequence;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] dxa::protocol::EntityId Actor() const
    {
        const auto* welcome = Welcome();
        if (welcome == nullptr)
        {
            throw std::logic_error{"game probe has no actor"};
        }
        return welcome->actor;
    }

private:
    GameClientProbe(
        boost::asio::io_context& io,
        GameTicketCredential credential)
        : io_{io},
          credential_{std::move(credential)},
          udp_{io},
          gameTcpEndpoint_{
              boost::asio::ip::make_address(credential_.ticket.host),
              credential_.ticket.tcpPort},
          gameUdpEndpoint_{
              boost::asio::ip::make_address(credential_.ticket.host),
              credential_.ticket.udpPort}
    {
        udp_.open(gameUdpEndpoint_.protocol());
        udp_.bind(boost::asio::ip::udp::endpoint{
            gameUdpEndpoint_.protocol(),
            0U});
    }

    void ConnectTcp()
    {
        boost::asio::ip::tcp::socket socket{io_};
        socket.connect(gameTcpEndpoint_);
        const std::weak_ptr<GameClientProbe> weak = shared_from_this();
        tcp_ = dxa::protocol::AsioFramedConnection::Create(
            std::move(socket),
            [weak](dxa::protocol::RawFrame frame) {
                if (const auto probe = weak.lock())
                {
                    const auto decoded =
                        dxa::protocol::DecodeGameServerMessage(
                            frame.type,
                            frame.payload);
                    if (decoded.message.has_value())
                    {
                        probe->tcpMessages_.push_back(*decoded.message);
                    }
                    else
                    {
                        probe->tcpProtocolError_ = true;
                    }
                }
            },
            [weak](const boost::system::error_code error) {
                if (const auto probe = weak.lock())
                {
                    probe->tcpClosed_ = error;
                }
            });
        tcp_->Start();
        const auto encoded = dxa::protocol::EncodeGameClientMessage(
            dxa::protocol::GameClientMessage{
                dxa::protocol::GameClientHello{
                    credential_.ticket.match,
                    credential_.player,
                    credential_.ticket.ticket}});
        if (!tcp_->Send(encoded))
        {
            throw std::runtime_error{"game hello send failed"};
        }
    }

    void StartUdpReceive()
    {
        if (udpReadStarted_)
        {
            return;
        }
        udpReadStarted_ = true;
        ReceiveUdp();
    }

    void ReceiveUdp()
    {
        const std::weak_ptr<GameClientProbe> weak = shared_from_this();
        udp_.async_receive_from(
            boost::asio::buffer(udpBuffer_),
            udpRemote_,
            [weak](
                const boost::system::error_code error,
                const std::size_t received) {
                if (const auto probe = weak.lock())
                {
                    if (!error)
                    {
                        const auto decoded =
                            dxa::protocol::DecodeServerDatagram(
                                std::span{
                                    probe->udpBuffer_.data(),
                                    received});
                        if (decoded.datagram.has_value())
                        {
                            probe->datagrams_.push_back(*decoded.datagram);
                        }
                    }
                    if (probe->udp_.is_open())
                    {
                        probe->ReceiveUdp();
                    }
                }
            });
    }

    void SendUdp(const dxa::protocol::ClientDatagram& datagram)
    {
        const auto encoded = dxa::protocol::EncodeClientDatagram(datagram);
        udp_.send_to(boost::asio::buffer(encoded.bytes), gameUdpEndpoint_);
    }

    template <typename Message>
    [[nodiscard]] const Message* LatestTcp() const
    {
        for (auto message = tcpMessages_.rbegin();
             message != tcpMessages_.rend();
             ++message)
        {
            if (const auto* value = std::get_if<Message>(&*message))
            {
                return value;
            }
        }
        return nullptr;
    }

    boost::asio::io_context& io_;
    GameTicketCredential credential_;
    std::shared_ptr<dxa::protocol::AsioFramedConnection> tcp_;
    boost::asio::ip::udp::socket udp_;
    boost::asio::ip::tcp::endpoint gameTcpEndpoint_;
    boost::asio::ip::udp::endpoint gameUdpEndpoint_;
    boost::asio::ip::udp::endpoint udpRemote_;
    std::array<std::byte, dxa::protocol::MaxUdpDatagramBytes> udpBuffer_{};
    std::vector<dxa::protocol::GameServerMessage> tcpMessages_;
    std::vector<dxa::protocol::ServerDatagram> datagrams_;
    std::optional<boost::system::error_code> tcpClosed_;
    bool tcpProtocolError_ = false;
    bool udpReadStarted_ = false;
};

class GameNetworkFixture
{
public:
    explicit GameNetworkFixture(
        dxa::simulation::MatchConfig config =
            dxa::simulation::DefaultMatchConfig(),
        std::shared_ptr<dxa::game_server::IUdpTokenSource> tokenSource = {},
        const dxa::protocol::ReplicationMode replicationMode =
            dxa::protocol::ReplicationMode::FullState)
        : lobby_{},
          worker_{
              lobby_.Io(),
              MakeGameConfig(
                  lobby_.WorkerPort(),
                  std::move(config),
                  std::move(tokenSource),
                  replicationMode)}
    {
    }

    GameNetworkFixture(
        dxa::simulation::MatchConfig config,
        const dxa::protocol::ReplicationMode replicationMode)
        : GameNetworkFixture(
              std::move(config),
              {},
              replicationMode)
    {
    }

    ~GameNetworkFixture()
    {
        for (const auto& client : gameClients_)
        {
            client->Close();
        }
        worker_.Stop();
        lobby_.Io().restart();
        while (lobby_.Io().poll_one() != 0U)
        {
        }
    }

    void StartLobbyAndWorker()
    {
        worker_.Start();
        started_ = true;
    }

    [[nodiscard]] boost::asio::io_context& BotIo() noexcept
    {
        return lobby_.Io();
    }

    [[nodiscard]] std::uint16_t LobbyPort() const
    {
        return lobby_.Port();
    }

    [[nodiscard]] ReadyNetworkRoom CreateReadyTwoPlayerRoom()
    {
        if (!started_)
        {
            throw std::logic_error{"game network fixture is not started"};
        }
        const auto host = lobby_.AddClient();
        const auto guest = lobby_.AddClient();
        lobby_.ConnectAndWelcome(host);
        lobby_.ConnectAndWelcome(guest);
        static_cast<void>(host->client->CreateRoom());
        RunUntil([&host] {
            return LatestLobbyMessage<dxa::protocol::RoomSnapshot>(*host)
                != nullptr;
        });
        const dxa::protocol::RoomId room =
            LatestLobbyMessage<dxa::protocol::RoomSnapshot>(*host)->room;
        static_cast<void>(guest->client->JoinRoom(room));
        RunUntil([&host, &guest] {
            const auto* first =
                LatestLobbyMessage<dxa::protocol::RoomSnapshot>(*host);
            const auto* second =
                LatestLobbyMessage<dxa::protocol::RoomSnapshot>(*guest);
            return first != nullptr
                && second != nullptr
                && first->members.size() == 2U
                && second->members.size() == 2U;
        });
        static_cast<void>(host->client->SetReady(true));
        static_cast<void>(guest->client->SetReady(true));
        RunUntil([&host, &guest] {
            const auto* first =
                LatestLobbyMessage<dxa::protocol::RoomSnapshot>(*host);
            const auto* second =
                LatestLobbyMessage<dxa::protocol::RoomSnapshot>(*guest);
            return first != nullptr
                && second != nullptr
                && std::all_of(
                    first->members.begin(),
                    first->members.end(),
                    [](const auto& member) { return member.ready; })
                && std::all_of(
                    second->members.begin(),
                    second->members.end(),
                    [](const auto& member) { return member.ready; });
        });
        return {host, guest, room};
    }

    void StartMatch(const std::shared_ptr<LobbyClientProbe>& host)
    {
        static_cast<void>(host->client->StartMatch());
    }

    [[nodiscard]] TicketPair WaitForTwoTickets(
        const ReadyNetworkRoom& room)
    {
        RunUntil([&room] {
            return LatestLobbyMessage<dxa::protocol::MatchTicket>(*room.host)
                    != nullptr
                && LatestLobbyMessage<dxa::protocol::MatchTicket>(*room.guest)
                    != nullptr;
        });
        return TicketPair{
            {
                *LatestLobbyMessage<dxa::protocol::MatchTicket>(*room.host),
                LatestLobbyMessage<dxa::protocol::ServerWelcome>(
                    *room.host)->player,
            },
            {
                *LatestLobbyMessage<dxa::protocol::MatchTicket>(*room.guest),
                LatestLobbyMessage<dxa::protocol::ServerWelcome>(
                    *room.guest)->player,
            }};
    }

    [[nodiscard]] std::shared_ptr<GameClientProbe> Authenticate(
        GameTicketCredential credential)
    {
        auto client = GameClientProbe::Connect(
            lobby_.Io(),
            std::move(credential));
        gameClients_.push_back(client);
        RunUntil([&client] {
            return client->Welcome() != nullptr
                || client->Error() != nullptr
                || client->TcpClosed();
        });
        return client;
    }

    void WaitForSnapshotAck(
        const std::shared_ptr<GameClientProbe>& client,
        const std::uint32_t sequence)
    {
        RunUntil([&client, sequence] {
            return client->LatestSnapshotAck() == sequence;
        });
    }

    [[nodiscard]] dxa::protocol::GameMatchResult WaitForResult(
        const std::shared_ptr<GameClientProbe>& client)
    {
        RunUntil([&client] { return client->Result() != nullptr; });
        return *client->Result();
    }

    void WaitForRoomCleanup(const ReadyNetworkRoom& room)
    {
        RunUntil([&room] {
            const auto* rooms =
                LatestLobbyMessage<dxa::protocol::RoomListResponse>(
                    *room.host);
            return rooms != nullptr && rooms->rooms.empty();
        });
    }

    void StopWorker()
    {
        worker_.Stop();
    }

    [[nodiscard]] std::vector<dxa::game_server::ServerMatchMetricsSnapshot>
    CompletedMetrics() const
    {
        return worker_.CompletedMetrics();
    }

    template <typename Condition>
    void RunUntil(Condition condition)
    {
        lobby_.RunUntil(std::move(condition));
    }

private:
    [[nodiscard]] static dxa::game_server::GameServerConfig MakeGameConfig(
        const std::uint16_t controlPort,
        dxa::simulation::MatchConfig matchConfig,
        std::shared_ptr<dxa::game_server::IUdpTokenSource> tokenSource,
        const dxa::protocol::ReplicationMode replicationMode)
    {
        dxa::game_server::GameServerConfig config;
        config.options.lobbyControlPort = controlPort;
        config.options.gameTcpPort = 0U;
        config.options.gameUdpPort = 0U;
        config.options.replicationMode = replicationMode;
        config.matchConfig = std::move(matchConfig);
        config.udpTokenSource = std::move(tokenSource);
        return config;
    }

    LobbyNetworkFixture lobby_;
    dxa::game_server::GameServer worker_;
    std::vector<std::shared_ptr<GameClientProbe>> gameClients_;
    bool started_ = false;
};

#if defined(_WIN32)
class DeterministicVerticalTokenSource final
    : public dxa::game_server::IUdpTokenSource
{
public:
    [[nodiscard]] bool Fill(
        const std::span<std::byte, 16U> output) noexcept override
    {
        const std::uint8_t seed = next_.fetch_add(1U);
        for (std::size_t index = 0U; index < output.size(); ++index)
        {
            output[index] = static_cast<std::byte>(
                static_cast<std::uint8_t>(seed + index));
        }
        return true;
    }

    [[nodiscard]] std::uint32_t FillCount() const noexcept
    {
        return static_cast<std::uint32_t>(next_.load() - 0x41U);
    }

private:
    std::atomic<std::uint8_t> next_{0x41U};
};

class ScopedOutputCapture
{
public:
    ScopedOutputCapture()
    {
        testing::internal::CaptureStdout();
        testing::internal::CaptureStderr();
    }

    ~ScopedOutputCapture()
    {
        if (active_)
        {
            Finish();
        }
    }

    void Finish()
    {
        if (!active_)
        {
            return;
        }
        captured_ = testing::internal::GetCapturedStderr()
            + testing::internal::GetCapturedStdout();
        active_ = false;
    }

    [[nodiscard]] const std::string& Text() const noexcept
    {
        return captured_;
    }

private:
    std::string captured_;
    bool active_ = true;
};

class VerticalWatchdog
{
public:
    explicit VerticalWatchdog(const std::chrono::seconds timeout)
        : ownerThread_{GetCurrentThreadId()},
          thread_{[this, timeout] {
              std::unique_lock lock{mutex_};
              if (!condition_.wait_for(lock, timeout, [this] { return done_; }))
              {
                  timedOut_.store(true);
                  static_cast<void>(PostThreadMessageW(
                      ownerThread_,
                      WM_QUIT,
                      0,
                      0));
              }
          }}
    {
    }

    ~VerticalWatchdog()
    {
        Finish();
    }

    void Finish()
    {
        {
            std::scoped_lock lock{mutex_};
            done_ = true;
        }
        condition_.notify_all();
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

    [[nodiscard]] bool TimedOut() const noexcept
    {
        return timedOut_.load();
    }

private:
    DWORD ownerThread_ = 0U;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> timedOut_{false};
    bool done_ = false;
    std::thread thread_;
};

[[nodiscard]] inline dxa::simulation::MatchConfig
ShortNetworkVerticalMatchConfig()
{
    dxa::simulation::MatchConfig config =
        dxa::simulation::DefaultMatchConfig();
    config.meleeNeutralCount = 0U;
    config.rangedNeutralCount = 0U;
    config.rifleLootCount = 0U;
    config.arcPulseLootCount = 0U;
    config.medKitLootCount = 0U;
    config.suddenDeathTick = 30U;
    config.hardTimeoutTick = 60U;
    return config;
}

[[nodiscard]] inline std::string VerticalSecretHex(const std::uint8_t seed)
{
    std::ostringstream encoded;
    encoded << std::hex << std::setfill('0');
    for (std::uint8_t index = 0U; index < 16U; ++index)
    {
        encoded << std::setw(2)
                << static_cast<std::uint32_t>(
                    static_cast<std::uint8_t>(seed + index));
    }
    return encoded.str();
}

[[nodiscard]] inline std::size_t NetworkSecretLeakCount(
    std::string output,
    const std::uint32_t participantCount)
{
    std::transform(
        output.begin(),
        output.end(),
        output.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });

    std::size_t leaks = 0U;
    for (std::uint32_t index = 0U; index < participantCount; ++index)
    {
        const std::array secrets{
            VerticalSecretHex(static_cast<std::uint8_t>(1U + index)),
            VerticalSecretHex(static_cast<std::uint8_t>(0x41U + index))};
        leaks += static_cast<std::size_t>(std::count_if(
            secrets.begin(),
            secrets.end(),
            [&output](const std::string& secret) {
                return output.find(secret) != std::string::npos;
            }));
    }
    return leaks;
}

class NetworkVerticalFixture
{
public:
    explicit NetworkVerticalFixture(
        dxa::simulation::MatchConfig matchConfig,
        const dxa::protocol::ReplicationMode replicationMode =
            dxa::protocol::ReplicationMode::FullState)
        : tokens_{std::make_shared<DeterministicVerticalTokenSource>()},
          network_{std::move(matchConfig), tokens_, replicationMode},
          replicationMode_{replicationMode}
    {
    }

    ~NetworkVerticalFixture()
    {
        StopServers();
    }

    void StartServers()
    {
        network_.StartLobbyAndWorker();
        botWork_.emplace(boost::asio::make_work_guard(network_.BotIo()));
        botThread_ = std::thread{[this] { network_.BotIo().run(); }};
    }

    void StopServers()
    {
        if (stopped_.exchange(true))
        {
            return;
        }
        network_.StopWorker();
        botWork_.reset();
        network_.BotIo().stop();
        if (botThread_.joinable())
        {
            botThread_.join();
        }
    }

    [[nodiscard]] dxa::client::NetworkClientOptions HostOptions(
        const std::uint8_t expectedPlayers,
        const bool exitOnMatchResult = false) const
    {
        return {
            "127.0.0.1",
            network_.LobbyPort(),
            expectedPlayers,
            exitOnMatchResult,
            replicationMode_};
    }

    void WaitForRoom(dxa::client::NetworkClientController& host)
    {
        WaitUntil([&host] { return host.Room().has_value(); });
    }

    [[nodiscard]] boost::asio::io_context& BotIo() noexcept
    {
        return network_.BotIo();
    }

    [[nodiscard]] dxa::bot_client::BotClientOptions PlayBotOptions(
        const dxa::protocol::RoomId room,
        const std::uint32_t count = 1U) const
    {
        dxa::bot_client::BotClientOptions options;
        options.port = network_.LobbyPort();
        options.room = room;
        options.count = count;
        options.play = true;
        return options;
    }

    [[nodiscard]] dxa::engine::EngineRunOptions HiddenWarpHybridOptions(
        const std::uint32_t maximumFrames) const
    {
        return {
            320U,
            180U,
            maximumFrames,
            true,
            true,
            true,
            dxa::engine::GraphicsDriver::Warp,
            true,
            std::nullopt,
            dxa::engine::RenderPath::HybridDeferred};
    }

    void WaitForResults(
        dxa::client::NetworkClientController& host,
        const dxa::bot_client::BotCoordinator& bots,
        const std::size_t expectedBotSessions = 1U)
    {
        WaitUntil([&] {
            host.FixedUpdate({});
            const dxa::bot_client::BotCoordinatorReport report =
                bots.Report();
            return host.Result().has_value()
                && bots.Done()
                && report.result.has_value()
                && report.sessions.size() == expectedBotSessions
                && host.SnapshotCount() >= 2U
                && std::all_of(
                    report.sessions.begin(),
                    report.sessions.end(),
                    [](const dxa::bot_client::BotSessionReport& session) {
                        return session.exitCode == 0
                            && session.snapshotsApplied >= 2U;
                    });
        });
    }

    [[nodiscard]] std::uint32_t TokenFillCount() const noexcept
    {
        return tokens_->FillCount();
    }

    [[nodiscard]] std::filesystem::path ShaderPath() const
    {
        return std::filesystem::path{DXA_TEST_SHADER_PATH};
    }

    [[nodiscard]] std::filesystem::path AssetRoot() const
    {
        return std::filesystem::path{DXA_TEST_ASSET_ROOT};
    }

private:
    template <typename Condition>
    void WaitUntil(Condition condition)
    {
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::seconds{20};
        while (!condition())
        {
            if (std::chrono::steady_clock::now() >= deadline)
            {
                throw std::runtime_error{
                    "network vertical slice watchdog expired"};
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
    }

    std::shared_ptr<DeterministicVerticalTokenSource> tokens_;
    GameNetworkFixture network_;
    dxa::protocol::ReplicationMode replicationMode_ =
        dxa::protocol::ReplicationMode::FullState;
    std::optional<boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type>> botWork_;
    std::thread botThread_;
    std::atomic<bool> stopped_{false};
};
#endif

class ExpiringGameWorkerFixture
{
public:
    ExpiringGameWorkerFixture()
        : controlAcceptor_{
              io_,
              boost::asio::ip::tcp::endpoint{
                  boost::asio::ip::make_address("127.0.0.1"),
                  0U}},
          worker_{io_, MakeConfig(controlAcceptor_.local_endpoint().port())}
    {
        AcceptControl();
        worker_.Start();
    }

    ~ExpiringGameWorkerFixture()
    {
        worker_.Stop();
        boost::system::error_code ignored;
        controlAcceptor_.close(ignored);
        if (control_)
        {
            control_->Close();
        }
        io_.restart();
        while (io_.poll_one() != 0U)
        {
        }
    }

    [[nodiscard]] dxa::protocol::MatchFinished WaitForCompletion()
    {
        RunUntil([this] {
            return std::any_of(
                messages_.begin(),
                messages_.end(),
                [](const auto& message) {
                    return std::holds_alternative<
                        dxa::protocol::MatchFinished>(message);
                });
        });
        for (const auto& message : messages_)
        {
            if (const auto* finished =
                    std::get_if<dxa::protocol::MatchFinished>(&message))
            {
                return *finished;
            }
        }
        throw std::logic_error{"worker completion is absent"};
    }

private:
    [[nodiscard]] static dxa::game_server::GameServerConfig MakeConfig(
        const std::uint16_t controlPort)
    {
        dxa::game_server::GameServerConfig config;
        config.options.lobbyControlPort = controlPort;
        config.options.gameTcpPort = 0U;
        config.options.gameUdpPort = 0U;
        return config;
    }

    void AcceptControl()
    {
        controlAcceptor_.async_accept([this](
            const boost::system::error_code error,
            boost::asio::ip::tcp::socket socket) {
            if (error)
            {
                return;
            }
            control_ = dxa::protocol::AsioFramedConnection::Create(
                std::move(socket),
                [this](dxa::protocol::RawFrame frame) {
                    const auto decoded =
                        dxa::protocol::DecodeWorkerToLobbyMessage(
                            frame.type,
                            frame.payload);
                    if (!decoded.message.has_value())
                    {
                        return;
                    }
                    messages_.push_back(*decoded.message);
                    if (const auto* registration =
                            std::get_if<dxa::protocol::WorkerRegister>(
                                &messages_.back()))
                    {
                        Send(dxa::protocol::LobbyToWorkerMessage{
                            dxa::protocol::WorkerRegistered{
                                registration->worker}});
                        SendShortReservation();
                    }
                },
                [](const boost::system::error_code) {});
            control_->Start();
        });
    }

    void SendShortReservation()
    {
        const std::vector<dxa::protocol::ReservedParticipant> participants{
            {dxa::protocol::PlayerId{1U}, GameNetworkTicket(21U)},
            {dxa::protocol::PlayerId{2U}, GameNetworkTicket(22U)}};
        Send(dxa::protocol::LobbyToWorkerMessage{
            dxa::protocol::ReserveMatch{
                dxa::protocol::ReservationId{1U},
                dxa::protocol::MatchId{7U},
                20260824U,
                20U,
                participants}});
    }

    void Send(const dxa::protocol::LobbyToWorkerMessage& message)
    {
        if (!control_
            || !control_->Send(
                dxa::protocol::EncodeLobbyToWorkerMessage(message)))
        {
            throw std::runtime_error{"expiry control send failed"};
        }
    }

    template <typename Condition>
    void RunUntil(Condition condition)
    {
        bool timedOut = false;
        boost::asio::steady_timer timer{io_};
        timer.expires_after(std::chrono::seconds{5});
        timer.async_wait([&timedOut](const boost::system::error_code error) {
            if (!error)
            {
                timedOut = true;
            }
        });
        io_.restart();
        while (!condition() && !timedOut)
        {
            if (io_.run_one() == 0U)
            {
                break;
            }
        }
        timer.cancel();
        io_.restart();
        while (io_.poll_one() != 0U)
        {
        }
        if (!condition())
        {
            throw std::runtime_error{"expiry integration test timed out"};
        }
    }

    boost::asio::io_context io_;
    boost::asio::ip::tcp::acceptor controlAcceptor_;
    dxa::game_server::GameServer worker_;
    std::shared_ptr<dxa::protocol::AsioFramedConnection> control_;
    std::vector<dxa::protocol::WorkerToLobbyMessage> messages_;
};
} // namespace dxa::test
