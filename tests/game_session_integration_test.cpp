#include <dxa/game_client/GameSession.hpp>
#include <dxa/game_client/GameNetworkRuntime.hpp>

#include <dxa/game_common/ArenaFingerprint.hpp>
#include <dxa/protocol/AsioFramedConnection.hpp>
#include <dxa/protocol/GameSnapshotCodec.hpp>
#include <dxa/protocol/GameTcpMessageCodec.hpp>
#include <dxa/protocol/GameUdpCodec.hpp>
#include <dxa/protocol/ReplicationSnapshotCodec.hpp>
#include <dxa/simulation/ArenaMap.hpp>

#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using namespace std::chrono_literals;
using boost::asio::ip::tcp;
using boost::asio::ip::udp;
using dxa::game_client::GameSceneFrame;
using dxa::game_client::GameSession;
using dxa::game_client::GameSessionStart;
using dxa::game_client::GameSessionState;
using dxa::protocol::AsioFramedConnection;
using dxa::protocol::ClientDatagram;
using dxa::protocol::ClientInput;
using dxa::protocol::EntityId;
using dxa::protocol::GameClientHello;
using dxa::protocol::GameMatchResult;
using dxa::protocol::GameServerErrorCode;
using dxa::protocol::GameServerErrorMessage;
using dxa::protocol::GameServerMessage;
using dxa::protocol::GameServerWelcome;
using dxa::protocol::GameSnapshot;
using dxa::protocol::MatchCompletionReason;
using dxa::protocol::MatchId;
using dxa::protocol::MatchTicket;
using dxa::protocol::MatchTicketValue;
using dxa::protocol::NetworkActorRole;
using dxa::protocol::NetworkActorSnapshot;
using dxa::protocol::NetworkNeutralArchetype;
using dxa::protocol::NetworkWeaponType;
using dxa::protocol::PlayerId;
using dxa::protocol::ServerDatagram;
using dxa::protocol::SnapshotFragment;
using dxa::protocol::UdpBind;
using dxa::protocol::UdpBindAccepted;
using dxa::protocol::UdpSessionToken;

[[nodiscard]] MatchTicketValue Ticket(const std::uint8_t seed)
{
    MatchTicketValue ticket;
    for (std::size_t index = 0U; index < ticket.size(); ++index)
    {
        ticket[index] = static_cast<std::byte>(
            static_cast<std::uint8_t>(seed + index));
    }
    return ticket;
}

[[nodiscard]] UdpSessionToken Token(const std::uint8_t seed)
{
    UdpSessionToken token;
    for (std::size_t index = 0U; index < token.size(); ++index)
    {
        token[index] = static_cast<std::byte>(
            static_cast<std::uint8_t>(seed + index));
    }
    return token;
}

template <typename Condition>
void WaitUntil(Condition condition)
{
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!condition())
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            throw std::runtime_error{"game session test timed out"};
        }
        std::this_thread::yield();
    }
}

class FakeGameServer
{
public:
    FakeGameServer()
        : work_{boost::asio::make_work_guard(io_)},
          acceptor_{
              io_,
              tcp::endpoint{
                  boost::asio::ip::make_address("127.0.0.1"),
                  0U}},
          udp_{io_, udp::endpoint{
              boost::asio::ip::make_address("127.0.0.1"),
              0U}}
    {
        AcceptTcp();
        ReceiveUdp();
        thread_ = std::thread{[this] { io_.run(); }};
    }

    ~FakeGameServer()
    {
        boost::asio::post(io_, [this] {
            if (tcp_)
            {
                tcp_->Close();
            }
            boost::system::error_code ignored;
            acceptor_.close(ignored);
            udp_.close(ignored);
            work_.reset();
        });
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

    [[nodiscard]] std::uint16_t TcpPort() const
    {
        return acceptor_.local_endpoint().port();
    }

    [[nodiscard]] std::uint16_t UdpPort() const
    {
        return udp_.local_endpoint().port();
    }

    [[nodiscard]] GameSessionStart StartFor(
        const PlayerId player = PlayerId{3U},
        const std::optional<dxa::protocol::ReplicationMode> mode =
            std::nullopt) const
    {
        MatchTicket ticket;
        ticket.match = MatchId{7U};
        ticket.ticket = Ticket(4U);
        ticket.host = "127.0.0.1";
        ticket.tcpPort = TcpPort();
        ticket.udpPort = UdpPort();
        ticket.expiresInSeconds = 60U;
        return GameSessionStart{
            player,
            ticket,
            1U,
            dxa::game_common::SurvivalArenaFingerprint(
                dxa::simulation::SurvivalArenaMapDefinition()),
            mode,
            {}};
    }

    void AcceptHelloAndWelcome(
        const EntityId actor = EntityId{0U},
        const std::optional<std::uint32_t> crc = std::nullopt,
        const dxa::protocol::ReplicationMode mode =
            dxa::protocol::ReplicationMode::FullState)
    {
        const GameClientHello hello = WaitForHello();
        const GameServerWelcome welcome{
            hello.match,
            hello.player,
            actor,
            dxa::protocol::GameTickRate,
            dxa::protocol::SnapshotRate,
            1U,
            crc.value_or(dxa::game_common::SurvivalArenaFingerprint(
                dxa::simulation::SurvivalArenaMapDefinition())),
            mode,
            Token(9U)};
        SendTcp(GameServerMessage{welcome});
    }

    void SendAuthFailure()
    {
        static_cast<void>(WaitForHello());
        SendTcp(GameServerMessage{GameServerErrorMessage{
            GameServerErrorCode::AuthenticationFailed}});
        CloseTcpAfterFlush();
    }

    void CloseAfterHello()
    {
        static_cast<void>(WaitForHello());
        boost::asio::post(io_, [this] {
            if (tcp_)
            {
                tcp_->Close();
            }
        });
    }

    void AcceptUdpBind()
    {
        const UdpBind bind = WaitForBindCount(1U);
        SendUdp(ServerDatagram{UdpBindAccepted{
            bind.match,
            bind.player,
            0U}});
    }

    void AcceptUdpBindAfterRetry()
    {
        const UdpBind bind = WaitForBindCount(2U);
        SendUdp(ServerDatagram{UdpBindAccepted{
            bind.match,
            bind.player,
            0U}});
    }

    void SendPayload(
        const std::uint32_t snapshotId,
        const std::uint32_t serverTick,
        const std::uint32_t ack,
        const dxa::protocol::SnapshotPayload& payload,
        const bool wrongSource = false,
        const MatchId match = MatchId{7U})
    {
        const auto fragments = dxa::protocol::FragmentSnapshot(
            match,
            snapshotId,
            serverTick,
            ack,
            dxa::protocol::EncodeSnapshotPayload(payload));
        for (const SnapshotFragment& fragment : fragments)
        {
            SendUdp(ServerDatagram{fragment}, wrongSource);
        }
    }

    void SendSnapshot(
        const std::uint32_t snapshotId,
        const std::uint32_t serverTick,
        const std::uint32_t ack,
        const float localX = 10.0F,
        const float remoteX = 20.0F,
        const bool wrongSource = false,
        const bool localAlive = true,
        const MatchId match = MatchId{7U})
    {
        GameSnapshot snapshot;
        snapshot.aliveContenders = localAlive ? 2U : 1U;
        snapshot.actors = {
            NetworkActorSnapshot{
                EntityId{0U},
                NetworkActorRole::Contender,
                NetworkNeutralArchetype::None,
                {localX, 0.0F},
                localAlive ? 100 : 0,
                localAlive,
                NetworkWeaponType::Blade,
                0U,
                0U},
            NetworkActorSnapshot{
                EntityId{1U},
                NetworkActorRole::Contender,
                NetworkNeutralArchetype::None,
                {remoteX, 0.0F},
                100,
                true,
                NetworkWeaponType::Blade,
                0U,
                0U}};
        dxa::protocol::SnapshotPayload payload;
        payload.header = {
            dxa::protocol::SnapshotPayloadKind::FullState,
            dxa::protocol::SnapshotValueEncoding::FullPrecision,
            0U,
            snapshotId};
        payload.fullPrecision = std::move(snapshot);
        SendPayload(
            snapshotId,
            serverTick,
            ack,
            payload,
            wrongSource,
            match);
    }

    [[nodiscard]] ClientInput WaitForInput(const std::uint32_t sequence)
    {
        std::unique_lock lock{mutex_};
        if (!condition_.wait_for(lock, 5s, [&] {
                return std::any_of(
                    clientDatagrams_.begin(),
                    clientDatagrams_.end(),
                    [sequence](const ClientDatagram& datagram) {
                        const auto* input = std::get_if<ClientInput>(&datagram);
                        return input != nullptr
                            && input->inputSequence == sequence;
                    });
            }))
        {
            throw std::runtime_error{"client input was not received"};
        }
        for (const ClientDatagram& datagram : clientDatagrams_)
        {
            if (const auto* input = std::get_if<ClientInput>(&datagram);
                input != nullptr && input->inputSequence == sequence)
            {
                return *input;
            }
        }
        throw std::logic_error{"client input disappeared"};
    }

    void SendResult(const MatchId match = MatchId{7U})
    {
        static_cast<void>(WaitForHello());
        SendTcp(GameServerMessage{GameMatchResult{
            match,
            EntityId{0U},
            true,
            MatchCompletionReason::LastSurvivor,
            12U}});
        CloseTcpAfterFlush();
    }

    [[nodiscard]] std::size_t UdpBindCount() const
    {
        std::scoped_lock lock{mutex_};
        return bindCount_;
    }

private:
    void AcceptTcp()
    {
        acceptor_.async_accept([this](
            const boost::system::error_code error,
            tcp::socket socket) {
            if (error)
            {
                return;
            }
            tcp_ = AsioFramedConnection::Create(
                std::move(socket),
                [this](dxa::protocol::RawFrame frame) {
                    const auto decoded =
                        dxa::protocol::DecodeGameClientMessage(
                            frame.type,
                            frame.payload);
                    if (decoded.message.has_value())
                    {
                        {
                            std::scoped_lock lock{mutex_};
                            clientMessages_.push_back(*decoded.message);
                        }
                        condition_.notify_all();
                    }
                },
                [](const boost::system::error_code) {});
            tcp_->Start();
        });
    }

    void ReceiveUdp()
    {
        udp_.async_receive_from(
            boost::asio::buffer(udpBuffer_),
            clientUdpEndpoint_,
            [this](
                const boost::system::error_code error,
                const std::size_t received) {
                if (!error)
                {
                    const auto decoded = dxa::protocol::DecodeClientDatagram(
                        std::span{udpBuffer_.data(), received});
                    if (decoded.datagram.has_value())
                    {
                        {
                            std::scoped_lock lock{mutex_};
                            clientDatagrams_.push_back(*decoded.datagram);
                            if (std::holds_alternative<UdpBind>(
                                    *decoded.datagram))
                            {
                                ++bindCount_;
                            }
                        }
                        condition_.notify_all();
                    }
                }
                if (udp_.is_open())
                {
                    ReceiveUdp();
                }
            });
    }

    [[nodiscard]] GameClientHello WaitForHello()
    {
        std::unique_lock lock{mutex_};
        if (!condition_.wait_for(lock, 5s, [this] {
                return !clientMessages_.empty();
            }))
        {
            throw std::runtime_error{"game hello was not received"};
        }
        return std::get<GameClientHello>(clientMessages_.front());
    }

    [[nodiscard]] UdpBind WaitForBindCount(const std::size_t count)
    {
        std::unique_lock lock{mutex_};
        if (!condition_.wait_for(lock, 5s, [this, count] {
                return bindCount_ >= count;
            }))
        {
            throw std::runtime_error{"UDP bind was not received"};
        }
        for (auto datagram = clientDatagrams_.rbegin();
             datagram != clientDatagrams_.rend();
             ++datagram)
        {
            if (const auto* bind = std::get_if<UdpBind>(&*datagram))
            {
                return *bind;
            }
        }
        throw std::logic_error{"UDP bind disappeared"};
    }

    void SendTcp(const GameServerMessage& message)
    {
        boost::asio::post(io_, [this, message] {
            if (tcp_)
            {
                static_cast<void>(tcp_->Send(
                    dxa::protocol::EncodeGameServerMessage(message)));
            }
        });
    }

    void CloseTcpAfterFlush()
    {
        boost::asio::post(io_, [this] {
            if (tcp_)
            {
                tcp_->CloseAfterFlush();
            }
        });
    }

    void SendUdp(
        const ServerDatagram& datagram,
        const bool wrongSource = false)
    {
        std::unique_lock lock{mutex_};
        if (!condition_.wait_for(lock, 5s, [this] {
                return clientUdpEndpoint_.port() != 0U;
            }))
        {
            throw std::runtime_error{"client UDP endpoint is absent"};
        }
        const udp::endpoint recipient = clientUdpEndpoint_;
        lock.unlock();
        const auto encoded = dxa::protocol::EncodeServerDatagram(datagram);
        boost::asio::post(io_, [this, recipient, bytes = encoded.bytes, wrongSource] {
            if (!wrongSource)
            {
                udp_.send_to(boost::asio::buffer(bytes), recipient);
                return;
            }
            udp::socket rogue{io_};
            rogue.open(recipient.protocol());
            rogue.bind(udp::endpoint{recipient.protocol(), 0U});
            rogue.send_to(boost::asio::buffer(bytes), recipient);
        });
    }

    boost::asio::io_context io_;
    boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type> work_;
    tcp::acceptor acceptor_;
    udp::socket udp_;
    std::shared_ptr<AsioFramedConnection> tcp_;
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<dxa::protocol::GameClientMessage> clientMessages_;
    std::vector<ClientDatagram> clientDatagrams_;
    std::size_t bindCount_ = 0U;
    udp::endpoint clientUdpEndpoint_;
    std::array<std::byte, dxa::protocol::MaxUdpDatagramBytes + 1U>
        udpBuffer_{};
};

[[nodiscard]] dxa::protocol::SnapshotPayload QuantizedKeyframe(
    const std::uint32_t snapshotId,
    const std::uint16_t localX)
{
    dxa::protocol::SnapshotPayload payload;
    payload.header = {
        dxa::protocol::SnapshotPayloadKind::Keyframe,
        dxa::protocol::SnapshotValueEncoding::Quantized,
        0U,
        snapshotId};
    payload.global.phase = dxa::protocol::NetworkMatchPhase::Running;
    payload.global.safeZoneStage =
        dxa::protocol::NetworkSafeZoneStage::Stage1;
    payload.global.safeZoneCenter = {32768U, 32768U};
    payload.global.safeZoneRadius =
        std::numeric_limits<std::uint16_t>::max();
    payload.global.aliveContenders = 2U;
    payload.actorValues = {
        dxa::protocol::QuantizedActorValue{
            EntityId{0U},
            NetworkActorRole::Contender,
            NetworkNeutralArchetype::None,
            {localX, 32768U},
            100U,
            true,
            NetworkWeaponType::Blade,
            0U,
            0U},
        dxa::protocol::QuantizedActorValue{
            EntityId{1U},
            NetworkActorRole::Contender,
            NetworkNeutralArchetype::None,
            {40000U, 32768U},
            100U,
            true,
            NetworkWeaponType::Blade,
            0U,
            0U}};
    return payload;
}
} // namespace

TEST(GameSession, AuthenticatesBindsPredictsAndPublishesScene)
{
    FakeGameServer server;
    GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
    session.Start(server.StartFor());
    server.AcceptHelloAndWelcome(EntityId{0U});
    server.AcceptUdpBind();
    server.SendSnapshot(1U, 2U, 0U);
    WaitUntil([&] { return session.SnapshotCount() == 1U; });
    session.FixedUpdate();
    ASSERT_EQ(GameSessionState::Running, session.State());
    ASSERT_TRUE(session.SetDestination({0.0F, 0.0F}));
    session.FixedUpdate();
    static_cast<void>(server.WaitForInput(1U));
    server.SendSnapshot(2U, 4U, 1U, 9.8F);
    WaitUntil([&] { return session.SnapshotCount() == 2U; });
    session.FixedUpdate();

    const GameSceneFrame scene = session.SampleScene();
    EXPECT_TRUE(scene.connected);
    EXPECT_EQ(EntityId{0U}, scene.localActor);
    EXPECT_EQ(1U, scene.lastAckInputSequence);
    EXPECT_EQ(2U, scene.snapshotCount);
}

TEST(GameSession, ShapedQueueOverflowIsVisibleFailure)
{
    FakeGameServer server;
    GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
    GameSessionStart start = server.StartFor();
    start.udpImpairment = {
        5000ms,
        0ms,
        0U,
        20260825U};
    start.maximumQueuedUdpDatagramsPerPeer = 1U;
    session.Start(std::move(start));
    server.AcceptHelloAndWelcome();

    WaitUntil([&] {
        return session.State() == GameSessionState::ProtocolError;
    });

    const auto metrics = session.Metrics();
    EXPECT_EQ(1U, metrics.udpDatagramsDelayed);
    EXPECT_EQ(1U, metrics.shapedQueueOverflows);
    EXPECT_EQ(0U, metrics.udpDatagramsDelivered);
}

TEST(GameSession, RejectsUnexpectedReplicationModeBeforeUdpBind)
{
    FakeGameServer server;
    GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
    session.Start(server.StartFor(
        PlayerId{3U},
        dxa::protocol::ReplicationMode::InterestDelta));
    server.AcceptHelloAndWelcome(
        EntityId{0U},
        std::nullopt,
        dxa::protocol::ReplicationMode::FullState);

    WaitUntil([&] {
        return session.State() == GameSessionState::ProtocolError;
    });
    EXPECT_EQ(0U, server.UdpBindCount());
}

TEST(GameSession, MissingDeltaBaseRequestsAndAppliesRecoveryKeyframe)
{
    FakeGameServer server;
    GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
    session.Start(server.StartFor(
        PlayerId{3U},
        dxa::protocol::ReplicationMode::InterestDelta));
    server.AcceptHelloAndWelcome(
        EntityId{0U},
        std::nullopt,
        dxa::protocol::ReplicationMode::InterestDelta);
    server.AcceptUdpBind();

    server.SendPayload(1U, 2U, 0U, QuantizedKeyframe(1U, 32768U));
    WaitUntil([&] { return session.SnapshotCount() == 1U; });
    session.FixedUpdate();
    ASSERT_EQ(GameSessionState::Running, session.State());
    session.FixedUpdate();
    const ClientInput first = server.WaitForInput(1U);
    EXPECT_EQ(1U, first.acknowledgedSnapshotId);
    EXPECT_FALSE(first.requestKeyframe);

    dxa::protocol::SnapshotPayload missing;
    missing.header = {
        dxa::protocol::SnapshotPayloadKind::Delta,
        dxa::protocol::SnapshotValueEncoding::Quantized,
        2U,
        3U};
    server.SendPayload(3U, 6U, 1U, missing);
    WaitUntil([&] { return session.SnapshotCount() == 2U; });
    session.FixedUpdate();
    const ClientInput requested = server.WaitForInput(2U);
    EXPECT_EQ(1U, requested.acknowledgedSnapshotId);
    EXPECT_TRUE(requested.requestKeyframe);

    server.SendPayload(4U, 8U, 2U, QuantizedKeyframe(4U, 33000U));
    WaitUntil([&] { return session.SnapshotCount() == 3U; });
    session.FixedUpdate();
    const ClientInput recovered = server.WaitForInput(3U);
    EXPECT_EQ(4U, recovered.acknowledgedSnapshotId);
    EXPECT_FALSE(recovered.requestKeyframe);

    const auto metrics = session.Metrics();
    EXPECT_EQ(2U, metrics.snapshotsApplied);
    EXPECT_EQ(1U, metrics.snapshotsDiscarded);
    EXPECT_EQ(2U, metrics.keyframesApplied);
    EXPECT_EQ(1U, metrics.keyframeRequests);
}

TEST(GameSession, MetricsCountGameTrafficAndFreezeAfterResult)
{
    FakeGameServer server;
    GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
    session.Start(server.StartFor());
    server.AcceptHelloAndWelcome();
    server.AcceptUdpBind();
    server.SendSnapshot(1U, 2U, 0U);
    WaitUntil([&] { return session.SnapshotCount() == 1U; });
    session.FixedUpdate();

    const auto active = session.Metrics();
    const auto arena = dxa::simulation::SurvivalArenaMapDefinition();
    const auto expectedWelcomeBytes = dxa::protocol::EncodeTcpFrame(
        dxa::protocol::EncodeGameServerMessage(GameServerMessage{
            GameServerWelcome{
                MatchId{7U},
                PlayerId{3U},
                EntityId{0U},
                dxa::protocol::GameTickRate,
                dxa::protocol::SnapshotRate,
                1U,
                dxa::game_common::SurvivalArenaFingerprint(arena),
                dxa::protocol::ReplicationMode::FullState,
                Token(9U)}})).size();
    EXPECT_EQ(expectedWelcomeBytes, active.traffic.tcpReceivedBytes);
    EXPECT_EQ(0U, active.traffic.tcpSentBytes);
    EXPECT_GT(active.traffic.udpSentBytes, 0U);
    EXPECT_GT(active.traffic.udpReceivedBytes, 0U);
    EXPECT_EQ(1U, active.snapshotsApplied);
    EXPECT_EQ(0U, active.snapshotQueueDrops);

    server.SendResult();
    WaitUntil([&] { return session.State() == GameSessionState::Finished; });
    const auto frozen = session.Metrics();
    const auto expectedResultBytes = dxa::protocol::EncodeTcpFrame(
        dxa::protocol::EncodeGameServerMessage(GameServerMessage{
            GameMatchResult{
                MatchId{7U},
                EntityId{0U},
                true,
                MatchCompletionReason::LastSurvivor,
                12U}})).size();
    std::this_thread::sleep_for(20ms);

    EXPECT_EQ(frozen, session.Metrics());
    EXPECT_EQ(
        frozen.traffic.tcpReceivedBytes,
        active.traffic.tcpReceivedBytes + expectedResultBytes);
    EXPECT_GT(frozen.measurementNanoseconds, 0U);
}

TEST(GameSession, TwoSessionsShareOneRuntimeAndBothSynchronize)
{
    FakeGameServer firstServer;
    FakeGameServer secondServer;
    auto runtime = std::make_shared<dxa::game_client::GameNetworkRuntime>();
    ASSERT_TRUE(runtime->Start());

    dxa::game_client::GameSession first{
        dxa::simulation::BuildSurvivalArenaNavMesh(),
        runtime};
    dxa::game_client::GameSession second{
        dxa::simulation::BuildSurvivalArenaNavMesh(),
        runtime};
    first.Start(firstServer.StartFor(PlayerId{3U}));
    second.Start(secondServer.StartFor(PlayerId{4U}));

    firstServer.AcceptHelloAndWelcome();
    secondServer.AcceptHelloAndWelcome();
    firstServer.AcceptUdpBind();
    secondServer.AcceptUdpBind();
    firstServer.SendSnapshot(1U, 2U, 0U);
    secondServer.SendSnapshot(1U, 2U, 0U);

    WaitUntil([&] {
        first.FixedUpdate();
        second.FixedUpdate();
        return first.State() == GameSessionState::Running
            && second.State() == GameSessionState::Running;
    });
    EXPECT_EQ(1U, first.SnapshotCount());
    EXPECT_EQ(1U, second.SnapshotCount());

    first.Stop();
    EXPECT_EQ(GameSessionState::Running, second.State());
    second.Stop();
    runtime->Stop();
}

TEST(GameSession, RejectsMapMismatchBeforeUdpBind)
{
    FakeGameServer server;
    GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
    session.Start(server.StartFor());
    server.AcceptHelloAndWelcome(EntityId{0U}, 0xDEADBEEFU);

    WaitUntil([&] { return session.State() == GameSessionState::ProtocolError; });

    EXPECT_EQ(0U, server.UdpBindCount());
}

TEST(GameSession, SurfacesAuthenticationFailureAndHelloClose)
{
    {
        FakeGameServer server;
        GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
        session.Start(server.StartFor());
        server.SendAuthFailure();
        WaitUntil([&] {
            return session.State() == GameSessionState::ProtocolError;
        });
    }
    {
        FakeGameServer server;
        GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
        session.Start(server.StartFor());
        server.CloseAfterHello();
        WaitUntil([&] { return session.State() == GameSessionState::Closed; });
    }
}

TEST(GameSession, RetriesBindUntilAccepted)
{
    FakeGameServer server;
    GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
    session.Start(server.StartFor());
    server.AcceptHelloAndWelcome();
    server.AcceptUdpBindAfterRetry();
    server.SendSnapshot(1U, 2U, 0U);
    WaitUntil([&] { return session.SnapshotCount() == 1U; });
    session.FixedUpdate();

    EXPECT_GE(server.UdpBindCount(), 2U);
    EXPECT_EQ(GameSessionState::Running, session.State());
}

TEST(GameSession, RejectsWrongSourceDuplicateAndKeepsNewestSixtyFourSnapshots)
{
    FakeGameServer server;
    GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
    session.Start(server.StartFor());
    server.AcceptHelloAndWelcome();
    server.AcceptUdpBind();
    server.SendSnapshot(1U, 2U, 0U, 10.0F, 20.0F, true);
    server.SendSnapshot(1U, 2U, 0U);
    server.SendSnapshot(1U, 2U, 0U);
    for (std::uint32_t snapshot = 2U; snapshot <= 70U; ++snapshot)
    {
        server.SendSnapshot(
            snapshot,
            snapshot * 2U,
            0U,
            10.0F,
            static_cast<float>(snapshot));
    }
    WaitUntil([&] { return session.SnapshotCount() == 70U; });
    session.FixedUpdate();

    const GameSceneFrame scene = session.SampleScene();
    EXPECT_EQ(70U, scene.snapshotCount);
    const auto remote = std::find_if(
        scene.actors.begin(),
        scene.actors.end(),
        [](const auto& actor) { return actor.id == EntityId{1U}; });
    ASSERT_NE(scene.actors.end(), remote);
    EXPECT_GE(remote->position.x, 68.0F);
    const auto metrics = session.Metrics();
    EXPECT_EQ(64U, metrics.snapshotsApplied);
    EXPECT_EQ(6U, metrics.snapshotQueueDrops);
}

TEST(GameSession, DeliversResultAndRejectsImpossibleAck)
{
    {
        FakeGameServer server;
        GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
        session.Start(server.StartFor());
        server.AcceptHelloAndWelcome();
        server.SendResult();
        WaitUntil([&] { return session.State() == GameSessionState::Finished; });
        ASSERT_TRUE(session.Result().has_value());
        EXPECT_EQ(EntityId{0U}, session.Result()->winner);
    }
    {
        FakeGameServer server;
        GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
        session.Start(server.StartFor());
        server.AcceptHelloAndWelcome();
        server.AcceptUdpBind();
        server.SendSnapshot(1U, 2U, 99U);
        WaitUntil([&] { return session.SnapshotCount() == 1U; });
        session.FixedUpdate();
        EXPECT_EQ(GameSessionState::ProtocolError, session.State());
    }
}

TEST(GameSession, RejectsPrematureAndWrongMatchResult)
{
    {
        FakeGameServer server;
        GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
        session.Start(server.StartFor());
        server.SendResult();
        WaitUntil([&] {
            return session.State() == GameSessionState::ProtocolError;
        });
        EXPECT_FALSE(session.Result().has_value());
    }
    {
        FakeGameServer server;
        GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
        session.Start(server.StartFor());
        server.AcceptHelloAndWelcome();
        server.SendResult(MatchId{8U});
        WaitUntil([&] {
            return session.State() == GameSessionState::ProtocolError;
        });
        EXPECT_FALSE(session.Result().has_value());
    }
}

TEST(GameSession, PublishesLocalDeathWithoutDroppingConnection)
{
    FakeGameServer server;
    GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
    session.Start(server.StartFor());
    server.AcceptHelloAndWelcome();
    server.AcceptUdpBind();
    server.SendSnapshot(
        1U,
        2U,
        0U,
        10.0F,
        20.0F,
        false,
        false);
    WaitUntil([&] { return session.SnapshotCount() == 1U; });
    session.FixedUpdate();

    const GameSceneFrame scene = session.SampleScene();
    EXPECT_TRUE(scene.connected);
    EXPECT_FALSE(scene.localAlive);
}

TEST(GameSession, FinishedStateSurvivesQueuedInitialSnapshot)
{
    FakeGameServer server;
    GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
    session.Start(server.StartFor());
    server.AcceptHelloAndWelcome();
    server.AcceptUdpBind();
    server.SendSnapshot(1U, 2U, 0U);
    WaitUntil([&] { return session.SnapshotCount() == 1U; });
    server.SendResult();
    WaitUntil([&] { return session.State() == GameSessionState::Finished; });

    session.FixedUpdate();

    EXPECT_EQ(GameSessionState::Finished, session.State());
    EXPECT_TRUE(session.Result().has_value());
}

TEST(GameSession, RejectsSnapshotFromAnotherMatch)
{
    FakeGameServer server;
    GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
    session.Start(server.StartFor());
    server.AcceptHelloAndWelcome();
    server.AcceptUdpBind();
    server.SendSnapshot(
        1U,
        2U,
        0U,
        10.0F,
        20.0F,
        false,
        true,
        MatchId{8U});
    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(0U, session.SnapshotCount());
    EXPECT_EQ(GameSessionState::Synchronizing, session.State());
}

TEST(GameSession, KeepsTicketAndUdpTokenOutOfCapturedOutput)
{
    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    {
        FakeGameServer server;
        GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
        session.Start(server.StartFor());
        server.AcceptHelloAndWelcome();
        server.AcceptUdpBind();
        session.Stop();
    }
    const std::string capturedError = testing::internal::GetCapturedStderr();
    const std::string capturedOutput = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(capturedOutput.empty());
    EXPECT_TRUE(capturedError.empty());
}

TEST(GameSession, StopIsIdempotentAndDestructorJoins)
{
    FakeGameServer server;
    {
        GameSession session{dxa::simulation::BuildSurvivalArenaNavMesh()};
        session.Start(server.StartFor());
        session.Stop();
        session.Stop();
        EXPECT_EQ(GameSessionState::Closed, session.State());
    }
}
