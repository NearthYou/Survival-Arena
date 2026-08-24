#include <dxa/game_client/GameSession.hpp>

#include <dxa/game_client/ClientPredictor.hpp>
#include <dxa/game_client/RemoteInterpolator.hpp>
#include <dxa/game_client/SnapshotReassembler.hpp>

#include <dxa/protocol/AsioFramedConnection.hpp>
#include <dxa/protocol/GameTcpMessageCodec.hpp>
#include <dxa/protocol/GameUdpCodec.hpp>
#include <dxa/simulation/MatchConfig.hpp>

#include <boost/asio.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace dxa::game_client
{
namespace
{
using boost::asio::ip::tcp;
using boost::asio::ip::udp;

constexpr std::size_t SnapshotQueueCapacity = 64U;
constexpr auto BindRetryInterval = std::chrono::milliseconds{250};

[[nodiscard]] const dxa::protocol::NetworkActorSnapshot* FindActor(
    const dxa::protocol::GameSnapshot& snapshot,
    const dxa::protocol::EntityId actor) noexcept
{
    const auto found = std::lower_bound(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        actor,
        [](const dxa::protocol::NetworkActorSnapshot& candidate,
           const dxa::protocol::EntityId value) {
            return candidate.id < value;
        });
    return found == snapshot.actors.end() || found->id != actor
        ? nullptr
        : &*found;
}
} // namespace

struct GameSession::Impl
{
    explicit Impl(dxa::simulation::NavMesh sourceNavMesh)
        : navMesh{std::move(sourceNavMesh)},
          tcpResolver{io},
          udpResolver{io},
          udpSocket{io},
          bindTimer{io},
          interpolation{3U, dxa::protocol::MaxClientSnapshotBuffer}
    {
    }

    ~Impl()
    {
        Stop();
    }

    void Start(GameSessionStart sourceStart)
    {
        GameSessionState expected = GameSessionState::Idle;
        if (!state.compare_exchange_strong(
                expected,
                GameSessionState::Connecting))
        {
            throw std::logic_error{"game session can start only once"};
        }
        start = std::move(sourceStart);
        stopRequested.store(false);
        work.emplace(boost::asio::make_work_guard(io));
        boost::asio::post(io, [this] { ResolveTcp(); });
        networkThread = std::thread{[this] { io.run(); }};
    }

    void ResolveTcp()
    {
        if (stopRequested.load())
        {
            return;
        }
        tcpResolver.async_resolve(
            start.ticket.host,
            std::to_string(start.ticket.tcpPort),
            [this](
                const boost::system::error_code error,
                const tcp::resolver::results_type endpoints) {
                if (error || stopRequested.load())
                {
                    if (!stopRequested.load())
                    {
                        state.store(GameSessionState::Closed);
                    }
                    return;
                }
                auto socket = std::make_shared<tcp::socket>(io);
                boost::asio::async_connect(
                    *socket,
                    endpoints,
                    [this, socket](
                        const boost::system::error_code connectError,
                        const tcp::endpoint&) {
                        if (connectError || stopRequested.load())
                        {
                            if (!stopRequested.load())
                            {
                                state.store(GameSessionState::Closed);
                            }
                            return;
                        }
                        AttachTcp(std::move(*socket));
                    });
            });
    }

    void AttachTcp(tcp::socket socket)
    {
        tcpTransport = dxa::protocol::AsioFramedConnection::Create(
            std::move(socket),
            [this](dxa::protocol::RawFrame frame) {
                HandleTcpFrame(std::move(frame));
            },
            [this](const boost::system::error_code error) {
                static_cast<void>(error);
                if (stopRequested.load())
                {
                    return;
                }
                const GameSessionState current = state.load();
                if (current != GameSessionState::Finished
                    && current != GameSessionState::ProtocolError)
                {
                    state.store(GameSessionState::Closed);
                }
            });
        tcpTransport->Start();
        state.store(GameSessionState::Authenticating);
        const dxa::protocol::GameClientMessage hello{
            dxa::protocol::GameClientHello{
                start.ticket.match,
                start.player,
                start.ticket.ticket}};
        if (!tcpTransport->Send(
                dxa::protocol::EncodeGameClientMessage(hello)))
        {
            FailProtocol();
        }
    }

    void HandleTcpFrame(dxa::protocol::RawFrame frame)
    {
        const auto decoded = dxa::protocol::DecodeGameServerMessage(
            frame.type,
            frame.payload);
        if (!decoded.message.has_value())
        {
            FailProtocol();
            return;
        }
        if (const auto* welcome =
                std::get_if<dxa::protocol::GameServerWelcome>(
                    &*decoded.message))
        {
            if (state.load() != GameSessionState::Authenticating
                || welcome->match != start.ticket.match
                || welcome->player != start.player
                || welcome->tickRate != dxa::protocol::GameTickRate
                || welcome->snapshotRate != dxa::protocol::SnapshotRate
                || welcome->mapId != start.expectedMapId
                || welcome->navMeshCrc32 != start.expectedNavMeshCrc32)
            {
                FailProtocol();
                return;
            }
            localActor.store(welcome->actor.value);
            udpToken = welcome->udpToken;
            state.store(GameSessionState::BindingUdp);
            ResolveUdp();
            return;
        }
        if (std::holds_alternative<dxa::protocol::GameServerErrorMessage>(
                *decoded.message))
        {
            FailProtocol();
            return;
        }

        const auto result = std::get<dxa::protocol::GameMatchResult>(
            *decoded.message);
        const GameSessionState current = state.load();
        if ((current != GameSessionState::BindingUdp
             && current != GameSessionState::Synchronizing
             && current != GameSessionState::Running)
            || result.match != start.ticket.match)
        {
            FailProtocol();
            return;
        }
        {
            std::scoped_lock lock{resultMutex};
            matchResult = result;
        }
        bindTimer.cancel();
        boost::system::error_code ignored;
        udpSocket.cancel(ignored);
        udpSocket.close(ignored);
        state.store(GameSessionState::Finished);
    }

    void ResolveUdp()
    {
        udpResolver.async_resolve(
            udp::v4(),
            start.ticket.host,
            std::to_string(start.ticket.udpPort),
            [this](
                const boost::system::error_code error,
                const udp::resolver::results_type endpoints) {
                if (error || endpoints.empty() || stopRequested.load())
                {
                    if (!stopRequested.load())
                    {
                        FailProtocol();
                    }
                    return;
                }
                serverUdpEndpoint = *endpoints.begin();
                udpSocket.open(serverUdpEndpoint.protocol());
                udpSocket.bind(udp::endpoint{
                    serverUdpEndpoint.protocol(),
                    0U});
                ReceiveUdp();
                SendBind();
            });
    }

    void SendBind()
    {
        if (stopRequested.load()
            || state.load() != GameSessionState::BindingUdp)
        {
            return;
        }
        const auto encoded = dxa::protocol::EncodeClientDatagram(
            dxa::protocol::ClientDatagram{
                dxa::protocol::UdpBind{
                    start.ticket.match,
                    start.player,
                    udpToken}});
        auto bytes = std::make_shared<std::vector<std::byte>>(
            encoded.bytes);
        udpSocket.async_send_to(
            boost::asio::buffer(*bytes),
            serverUdpEndpoint,
            [bytes](
                const boost::system::error_code,
                const std::size_t) {});
        bindTimer.expires_after(BindRetryInterval);
        bindTimer.async_wait([this](const boost::system::error_code error) {
            if (!error)
            {
                SendBind();
            }
        });
    }

    void ReceiveUdp()
    {
        if (!udpSocket.is_open())
        {
            return;
        }
        udpSocket.async_receive_from(
            boost::asio::buffer(udpBuffer),
            udpRemoteEndpoint,
            [this](
                const boost::system::error_code error,
                const std::size_t received) {
                if (!error
                    && received <= dxa::protocol::MaxUdpDatagramBytes
                    && udpRemoteEndpoint == serverUdpEndpoint)
                {
                    const auto decoded = dxa::protocol::DecodeServerDatagram(
                        std::span{udpBuffer.data(), received});
                    if (decoded.datagram.has_value())
                    {
                        HandleServerDatagram(*decoded.datagram);
                    }
                }
                if (udpSocket.is_open() && !stopRequested.load())
                {
                    ReceiveUdp();
                }
            });
    }

    void HandleServerDatagram(const dxa::protocol::ServerDatagram& datagram)
    {
        if (const auto* accepted =
                std::get_if<dxa::protocol::UdpBindAccepted>(&datagram))
        {
            if (state.load() != GameSessionState::BindingUdp
                || accepted->match != start.ticket.match
                || accepted->player != start.player)
            {
                return;
            }
            bindTimer.cancel();
            state.store(GameSessionState::Synchronizing);
            return;
        }
        const GameSessionState current = state.load();
        if (current != GameSessionState::Synchronizing
            && current != GameSessionState::Running)
        {
            return;
        }
        const auto completed = reassembler.Push(
            std::get<dxa::protocol::SnapshotFragment>(datagram));
        if (!completed.has_value())
        {
            return;
        }
        {
            std::scoped_lock lock{snapshotMutex};
            if (snapshotQueue.size() >= SnapshotQueueCapacity)
            {
                snapshotQueue.pop_front();
                ++droppedSnapshots;
            }
            snapshotQueue.push_back(*completed);
        }
        ++snapshotCount;
    }

    void PostInput(const PredictedInput input)
    {
        boost::asio::post(io, [this, input] {
            if (stopRequested.load()
                || state.load() != GameSessionState::Running
                || !udpSocket.is_open())
            {
                return;
            }
            dxa::protocol::ClientInput datagram;
            datagram.match = start.ticket.match;
            datagram.player = start.player;
            datagram.token = udpToken;
            datagram.inputSequence = input.sequence;
            if (input.moveDestination.has_value())
            {
                datagram.hasMoveDestination = true;
                datagram.moveDestination = {
                    input.moveDestination->x,
                    input.moveDestination->z};
            }
            if (input.attackTarget.has_value())
            {
                datagram.hasAttackTarget = true;
                datagram.attackTarget =
                    dxa::protocol::EntityId{*input.attackTarget};
            }
            const auto encoded = dxa::protocol::EncodeClientDatagram(
                dxa::protocol::ClientDatagram{datagram});
            auto bytes = std::make_shared<std::vector<std::byte>>(
                encoded.bytes);
            udpSocket.async_send_to(
                boost::asio::buffer(*bytes),
                serverUdpEndpoint,
                [bytes](
                    const boost::system::error_code,
                    const std::size_t) {});
        });
    }

    void FixedUpdate()
    {
        std::vector<ReassembledSnapshot> pending;
        {
            std::scoped_lock lock{snapshotMutex};
            pending.assign(snapshotQueue.begin(), snapshotQueue.end());
            snapshotQueue.clear();
        }
        std::sort(
            pending.begin(),
            pending.end(),
            [](const ReassembledSnapshot& left,
               const ReassembledSnapshot& right) {
                return left.serverTick < right.serverTick;
            });

        bool becameRunning = false;
        for (ReassembledSnapshot& snapshot : pending)
        {
            const dxa::protocol::EntityId actor{localActor.load()};
            const dxa::protocol::NetworkActorSnapshot* local = FindActor(
                snapshot.snapshot,
                actor);
            if (local == nullptr)
            {
                continue;
            }
            try
            {
                if (!predictor)
                {
                    const auto config = dxa::simulation::DefaultMatchConfig();
                    predictor = std::make_unique<ClientPredictor>(
                        navMesh,
                        dxa::simulation::Vec2{
                            local->position.x,
                            local->position.z},
                        config.contenderSpeed,
                        0.1F);
                    state.store(GameSessionState::Running);
                    becameRunning = true;
                }
                predictor->Reconcile(
                    {local->position.x, local->position.z},
                    snapshot.ackInputSequence);
            }
            catch (const std::exception&)
            {
                FailFromCaller();
                return;
            }
            interpolation.Push(snapshot);
            const dxa::protocol::GameSnapshot remote =
                interpolation.Sample(actor);
            std::scoped_lock lock{sceneMutex};
            scene.connected = true;
            scene.localActor = actor;
            scene.localAlive = local->alive;
            scene.localPosition = predictor->Position();
            scene.actors = remote.actors;
            scene.zoneRadius = remote.safeZoneRadius;
            scene.lastAckInputSequence = snapshot.ackInputSequence;
            scene.snapshotCount = snapshotCount.load();
        }

        if (state.load() == GameSessionState::Running && predictor)
        {
            if (!becameRunning)
            {
                try
                {
                    PostInput(predictor->AdvanceTick());
                }
                catch (const std::exception&)
                {
                    FailFromCaller();
                    return;
                }
            }
            std::scoped_lock lock{sceneMutex};
            scene.localPosition = predictor->Position();
            scene.snapshotCount = snapshotCount.load();
        }
    }

    [[nodiscard]] bool SetDestination(
        const dxa::simulation::Vec2 destination)
    {
        return state.load() == GameSessionState::Running
            && predictor
            && predictor->SetDestination(destination);
    }

    void FailFromCaller()
    {
        state.store(GameSessionState::ProtocolError);
        boost::asio::post(io, [this] { CloseNetwork(); });
    }

    void FailProtocol()
    {
        state.store(GameSessionState::ProtocolError);
        CloseNetwork();
    }

    void CloseNetwork()
    {
        tcpResolver.cancel();
        udpResolver.cancel();
        bindTimer.cancel();
        if (tcpTransport)
        {
            tcpTransport->Close();
        }
        boost::system::error_code ignored;
        udpSocket.cancel(ignored);
        udpSocket.close(ignored);
    }

    void Stop()
    {
        const bool alreadyStopped = stopRequested.exchange(true);
        if (!alreadyStopped)
        {
            state.store(GameSessionState::Closed);
            if (networkThread.joinable())
            {
                boost::asio::post(io, [this] {
                    CloseNetwork();
                    work.reset();
                });
            }
            else
            {
                work.reset();
            }
        }
        if (networkThread.joinable()
            && networkThread.get_id() != std::this_thread::get_id())
        {
            networkThread.join();
        }
    }

    dxa::simulation::NavMesh navMesh;
    std::unique_ptr<ClientPredictor> predictor;
    RemoteInterpolator interpolation;
    GameSceneFrame scene;
    mutable std::mutex sceneMutex;

    boost::asio::io_context io;
    std::optional<boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type>> work;
    tcp::resolver tcpResolver;
    udp::resolver udpResolver;
    udp::socket udpSocket;
    boost::asio::steady_timer bindTimer;
    std::shared_ptr<dxa::protocol::AsioFramedConnection> tcpTransport;
    std::thread networkThread;
    GameSessionStart start;
    udp::endpoint serverUdpEndpoint;
    udp::endpoint udpRemoteEndpoint;
    std::array<std::byte, dxa::protocol::MaxUdpDatagramBytes + 1U>
        udpBuffer{};
    SnapshotReassembler reassembler;
    dxa::protocol::UdpSessionToken udpToken;
    std::atomic<std::uint32_t> localActor{0U};
    std::atomic<GameSessionState> state{GameSessionState::Idle};
    std::atomic<bool> stopRequested{false};

    std::mutex snapshotMutex;
    std::deque<ReassembledSnapshot> snapshotQueue;
    std::uint64_t droppedSnapshots = 0U;
    std::atomic<std::uint64_t> snapshotCount{0U};
    mutable std::mutex resultMutex;
    std::optional<dxa::protocol::GameMatchResult> matchResult;
};

GameSession::GameSession(dxa::simulation::NavMesh navMesh)
    : impl_{std::make_unique<Impl>(std::move(navMesh))}
{
}

GameSession::~GameSession() = default;

void GameSession::Start(GameSessionStart start)
{
    impl_->Start(std::move(start));
}

bool GameSession::SetDestination(
    const dxa::simulation::Vec2 destination)
{
    return impl_->SetDestination(destination);
}

void GameSession::FixedUpdate()
{
    impl_->FixedUpdate();
}

GameSceneFrame GameSession::SampleScene() const
{
    std::scoped_lock lock{impl_->sceneMutex};
    return impl_->scene;
}

GameSessionState GameSession::State() const noexcept
{
    return impl_->state.load();
}

std::optional<dxa::protocol::GameMatchResult> GameSession::Result() const
{
    std::scoped_lock lock{impl_->resultMutex};
    return impl_->matchResult;
}

std::uint64_t GameSession::SnapshotCount() const noexcept
{
    return impl_->snapshotCount.load();
}

void GameSession::Stop()
{
    impl_->Stop();
}
} // namespace dxa::game_client
