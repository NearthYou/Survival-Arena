#include <dxa/game_client/GameSession.hpp>

#include <dxa/game_client/ClientPredictor.hpp>
#include <dxa/game_client/ClientSnapshotStream.hpp>
#include <dxa/game_client/GameNetworkRuntime.hpp>
#include <dxa/game_client/RemoteInterpolator.hpp>
#include <dxa/game_client/SnapshotReassembler.hpp>

#include <dxa/game_common/NetworkMetrics.hpp>
#include <dxa/protocol/AsioFramedConnection.hpp>
#include <dxa/protocol/GameTcpMessageCodec.hpp>
#include <dxa/protocol/GameUdpCodec.hpp>
#include <dxa/protocol/ReplicationSnapshotCodec.hpp>
#include <dxa/simulation/MatchConfig.hpp>

#include <boost/asio.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
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
    : public std::enable_shared_from_this<GameSession::Impl>
{
    struct PendingSnapshot
    {
        ReassembledSnapshot snapshot;
        std::vector<dxa::protocol::EntityId> resetInterpolationActors;
    };

    Impl(
        dxa::simulation::NavMesh sourceNavMesh,
        boost::asio::io_context& sourceIo)
        : navMesh{std::move(sourceNavMesh)},
          interpolation{3U, dxa::protocol::MaxClientSnapshotBuffer},
          io{sourceIo},
          tcpResolver{io},
          udpResolver{io},
          udpSocket{io},
          bindTimer{io}
    {
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
        const auto self = shared_from_this();
        boost::asio::post(io, [self] { self->ResolveTcp(); });
    }

    void ResolveTcp()
    {
        if (stopRequested.load())
        {
            return;
        }
        const auto self = shared_from_this();
        tcpResolver.async_resolve(
            start.ticket.host,
            std::to_string(start.ticket.tcpPort),
            [self](
                const boost::system::error_code error,
                const tcp::resolver::results_type endpoints) {
                if (error || self->stopRequested.load())
                {
                    if (!self->stopRequested.load())
                    {
                        self->state.store(GameSessionState::Closed);
                    }
                    return;
                }
                auto socket = std::make_shared<tcp::socket>(self->io);
                boost::asio::async_connect(
                    *socket,
                    endpoints,
                    [self, socket](
                        const boost::system::error_code connectError,
                        const tcp::endpoint&) {
                        if (connectError || self->stopRequested.load())
                        {
                            if (!self->stopRequested.load())
                            {
                                self->state.store(GameSessionState::Closed);
                            }
                            return;
                        }
                        self->AttachTcp(std::move(*socket));
                    });
            });
    }

    void AttachTcp(tcp::socket socket)
    {
        const std::weak_ptr<Impl> weak = weak_from_this();
        tcpTransport = dxa::protocol::AsioFramedConnection::Create(
            std::move(socket),
            [weak](dxa::protocol::RawFrame frame) {
                if (const auto self = weak.lock())
                {
                    self->HandleTcpFrame(std::move(frame));
                }
            },
            [weak](const boost::system::error_code error) {
                static_cast<void>(error);
                const auto self = weak.lock();
                if (!self || self->stopRequested.load())
                {
                    return;
                }
                const GameSessionState current = self->state.load();
                if (current != GameSessionState::Finished
                    && current != GameSessionState::ProtocolError)
                {
                    self->state.store(GameSessionState::Closed);
                }
            },
            [weak](
                const dxa::protocol::TrafficDirection direction,
                const std::size_t bytes) {
                if (const auto self = weak.lock())
                {
                    self->ObserveTcp(direction, bytes);
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
                || welcome->navMeshCrc32 != start.expectedNavMeshCrc32
                || (start.expectedReplicationMode.has_value()
                    && welcome->replicationMode
                        != *start.expectedReplicationMode))
            {
                FailProtocol();
                return;
            }
            dxa::game_common::GameTrafficTotals initialTraffic;
            initialTraffic.tcpReceivedBytes = pendingWelcomeTcpReceivedBytes;
            traffic.Start(initialTraffic);
            measurementStarted = true;
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
        traffic.Freeze();
    }

    void ObserveTcp(
        const dxa::protocol::TrafficDirection direction,
        const std::size_t bytes)
    {
        if (measurementStarted)
        {
            traffic.RecordTcp(direction, bytes);
            return;
        }
        if (direction == dxa::protocol::TrafficDirection::Received)
        {
            const std::uint64_t value = static_cast<std::uint64_t>(bytes);
            const std::uint64_t maximum =
                std::numeric_limits<std::uint64_t>::max();
            pendingWelcomeTcpReceivedBytes =
                value > maximum - pendingWelcomeTcpReceivedBytes
                ? maximum
                : pendingWelcomeTcpReceivedBytes + value;
        }
    }

    void ResolveUdp()
    {
        const auto self = shared_from_this();
        udpResolver.async_resolve(
            udp::v4(),
            start.ticket.host,
            std::to_string(start.ticket.udpPort),
            [self](
                const boost::system::error_code error,
                const udp::resolver::results_type endpoints) {
                if (error
                    || endpoints.empty()
                    || self->stopRequested.load())
                {
                    if (!self->stopRequested.load())
                    {
                        self->FailProtocol();
                    }
                    return;
                }
                self->serverUdpEndpoint = *endpoints.begin();
                self->udpSocket.open(self->serverUdpEndpoint.protocol());
                self->udpSocket.bind(udp::endpoint{
                    self->serverUdpEndpoint.protocol(),
                    0U});
                self->ReceiveUdp();
                self->SendBind();
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
        traffic.RecordUdp(
            dxa::protocol::TrafficDirection::Sent,
            bytes->size());
        udpSocket.async_send_to(
            boost::asio::buffer(*bytes),
            serverUdpEndpoint,
            [bytes](
                const boost::system::error_code,
                const std::size_t) {});
        bindTimer.expires_after(BindRetryInterval);
        const auto self = shared_from_this();
        bindTimer.async_wait([self](const boost::system::error_code error) {
            if (!error)
            {
                self->SendBind();
            }
        });
    }

    void ReceiveUdp()
    {
        if (!udpSocket.is_open())
        {
            return;
        }
        const auto self = shared_from_this();
        udpSocket.async_receive_from(
            boost::asio::buffer(udpBuffer),
            udpRemoteEndpoint,
            [self](
                const boost::system::error_code error,
                const std::size_t received) {
                if (!error)
                {
                    self->traffic.RecordUdp(
                        dxa::protocol::TrafficDirection::Received,
                        received);
                }
                if (!error
                    && received <= dxa::protocol::MaxUdpDatagramBytes
                    && self->udpRemoteEndpoint == self->serverUdpEndpoint)
                {
                    const auto decoded = dxa::protocol::DecodeServerDatagram(
                        std::span{self->udpBuffer.data(), received});
                    if (decoded.datagram.has_value())
                    {
                        self->HandleServerDatagram(*decoded.datagram);
                    }
                }
                if (self->udpSocket.is_open()
                    && !self->stopRequested.load())
                {
                    self->ReceiveUdp();
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
            if (std::holds_alternative<dxa::protocol::SnapshotFragment>(
                    datagram))
            {
                ++discardedSnapshots;
            }
            return;
        }
        const auto& fragment =
            std::get<dxa::protocol::SnapshotFragment>(datagram);
        if (fragment.match != start.ticket.match)
        {
            ++discardedSnapshots;
            return;
        }
        const auto completed = reassembler.PushBytes(fragment);
        if (!completed.has_value())
        {
            return;
        }
        const dxa::protocol::SnapshotPayloadDecodeResult decoded =
            dxa::protocol::DecodeSnapshotPayload(completed->bytes);
        if (!decoded.payload.has_value())
        {
            ++discardedSnapshots;
            FailProtocol();
            return;
        }

        SnapshotApplyResult applied;
        try
        {
            applied = snapshotStream.Apply(
                completed->snapshotId,
                *decoded.payload);
        }
        catch (const std::exception&)
        {
            ++discardedSnapshots;
            FailProtocol();
            return;
        }
        lastAppliedSnapshotId.store(applied.acknowledgedSnapshotId);
        const bool requestWasSet = keyframeRequested.exchange(
            applied.requestKeyframe);
        if (applied.requestKeyframe && !requestWasSet)
        {
            ++keyframeRequests;
        }
        if (!applied.world.has_value())
        {
            ++snapshotCount;
            ++discardedSnapshots;
            return;
        }

        PendingSnapshot pending;
        pending.snapshot = ReassembledSnapshot{
            completed->snapshotId,
            completed->serverTick,
            completed->ackInputSequence,
            std::move(*applied.world)};
        pending.resetInterpolationActors = std::move(applied.removedActors);
        pending.resetInterpolationActors.insert(
            pending.resetInterpolationActors.end(),
            applied.reenteredActors.begin(),
            applied.reenteredActors.end());
        std::sort(
            pending.resetInterpolationActors.begin(),
            pending.resetInterpolationActors.end());
        pending.resetInterpolationActors.erase(
            std::unique(
                pending.resetInterpolationActors.begin(),
                pending.resetInterpolationActors.end()),
            pending.resetInterpolationActors.end());
        if (decoded.payload->header.kind
            != dxa::protocol::SnapshotPayloadKind::Delta)
        {
            ++keyframesApplied;
        }
        {
            std::scoped_lock lock{snapshotMutex};
            if (snapshotQueue.size() >= SnapshotQueueCapacity)
            {
                snapshotQueue.pop_front();
                ++droppedSnapshots;
            }
            snapshotQueue.push_back(std::move(pending));
        }
        ++snapshotCount;
    }

    void PostInput(const PredictedInput input)
    {
        const auto self = shared_from_this();
        boost::asio::post(io, [self, input] {
            if (self->stopRequested.load()
                || self->state.load() != GameSessionState::Running
                || !self->udpSocket.is_open())
            {
                return;
            }
            dxa::protocol::ClientInput datagram;
            datagram.match = self->start.ticket.match;
            datagram.player = self->start.player;
            datagram.token = self->udpToken;
            datagram.inputSequence = input.sequence;
            datagram.acknowledgedSnapshotId =
                self->lastAppliedSnapshotId.load();
            datagram.requestKeyframe = self->keyframeRequested.load();
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
            self->traffic.RecordUdp(
                dxa::protocol::TrafficDirection::Sent,
                bytes->size());
            self->udpSocket.async_send_to(
                boost::asio::buffer(*bytes),
                self->serverUdpEndpoint,
                [bytes](
                    const boost::system::error_code,
                    const std::size_t) {});
        });
    }

    void FixedUpdate()
    {
        const GameSessionState entryState = state.load();
        if (entryState == GameSessionState::Finished
            || entryState == GameSessionState::ProtocolError
            || entryState == GameSessionState::Closed)
        {
            return;
        }
        std::vector<PendingSnapshot> pending;
        {
            std::scoped_lock lock{snapshotMutex};
            pending.assign(snapshotQueue.begin(), snapshotQueue.end());
            snapshotQueue.clear();
        }
        std::sort(
            pending.begin(),
            pending.end(),
            [](const PendingSnapshot& left,
               const PendingSnapshot& right) {
                return left.snapshot.serverTick < right.snapshot.serverTick;
            });

        bool becameRunning = false;
        for (PendingSnapshot& queued : pending)
        {
            ReassembledSnapshot& snapshot = queued.snapshot;
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
                    GameSessionState expected =
                        GameSessionState::Synchronizing;
                    if (state.compare_exchange_strong(
                            expected,
                            GameSessionState::Running))
                    {
                        becameRunning = true;
                    }
                    else if (expected != GameSessionState::Running)
                    {
                        return;
                    }
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
            interpolation.ForgetActors(queued.resetInterpolationActors);
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
            ++appliedSnapshotCount;
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
        const auto self = shared_from_this();
        boost::asio::post(io, [self] { self->CloseNetwork(); });
    }

    void FailProtocol()
    {
        state.store(GameSessionState::ProtocolError);
        CloseNetwork();
    }

    void CloseNetwork()
    {
        traffic.Freeze();
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

    void Stop(
        const bool runtimeStarted,
        const bool runningOnNetworkThread)
    {
        const bool alreadyStopped = stopRequested.exchange(true);
        if (alreadyStopped)
        {
            return;
        }
        state.store(GameSessionState::Closed);
        if (!runtimeStarted || runningOnNetworkThread)
        {
            CloseNetwork();
            return;
        }

        auto closed = std::make_shared<std::promise<void>>();
        std::future<void> completed = closed->get_future();
        const auto self = shared_from_this();
        boost::asio::post(io, [self, closed] {
            self->CloseNetwork();
            closed->set_value();
        });
        completed.wait();
    }

    dxa::simulation::NavMesh navMesh;
    std::unique_ptr<ClientPredictor> predictor;
    ClientSnapshotStream snapshotStream;
    RemoteInterpolator interpolation;
    GameSceneFrame scene;
    mutable std::mutex sceneMutex;

    boost::asio::io_context& io;
    tcp::resolver tcpResolver;
    udp::resolver udpResolver;
    udp::socket udpSocket;
    boost::asio::steady_timer bindTimer;
    std::shared_ptr<dxa::protocol::AsioFramedConnection> tcpTransport;
    GameSessionStart start;
    udp::endpoint serverUdpEndpoint;
    udp::endpoint udpRemoteEndpoint;
    std::array<std::byte, dxa::protocol::MaxUdpDatagramBytes + 1U>
        udpBuffer{};
    SnapshotReassembler reassembler;
    dxa::protocol::UdpSessionToken udpToken;
    std::atomic<std::uint32_t> localActor{0U};
    std::atomic<std::uint32_t> lastAppliedSnapshotId{0U};
    std::atomic<bool> keyframeRequested{false};
    std::atomic<GameSessionState> state{GameSessionState::Idle};
    std::atomic<bool> stopRequested{false};

    dxa::game_common::GameTrafficCounter traffic;
    std::uint64_t pendingWelcomeTcpReceivedBytes = 0U;
    bool measurementStarted = false;

    std::mutex snapshotMutex;
    std::deque<PendingSnapshot> snapshotQueue;
    std::atomic<std::uint64_t> discardedSnapshots{0U};
    std::atomic<std::uint64_t> droppedSnapshots{0U};
    std::atomic<std::uint64_t> snapshotCount{0U};
    std::atomic<std::uint64_t> appliedSnapshotCount{0U};
    std::atomic<std::uint64_t> keyframesApplied{0U};
    std::atomic<std::uint64_t> keyframeRequests{0U};
    mutable std::mutex resultMutex;
    std::optional<dxa::protocol::GameMatchResult> matchResult;
};

GameSession::GameSession(dxa::simulation::NavMesh navMesh)
    : runtime_{std::make_shared<GameNetworkRuntime>()},
      ownsRuntime_{true}
{
    if (!runtime_->Start())
    {
        throw std::runtime_error{"owned game network runtime failed to start"};
    }
    impl_ = std::make_shared<Impl>(
        std::move(navMesh),
        runtime_->Io());
}

GameSession::GameSession(
    dxa::simulation::NavMesh navMesh,
    std::shared_ptr<GameNetworkRuntime> runtime)
    : runtime_{std::move(runtime)}
{
    if (!runtime_)
    {
        throw std::invalid_argument{"shared game network runtime is required"};
    }
    if (!runtime_->Started())
    {
        throw std::logic_error{"shared game network runtime must be started"};
    }
    impl_ = std::make_shared<Impl>(
        std::move(navMesh),
        runtime_->Io());
}

GameSession::~GameSession()
{
    Stop();
}

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

dxa::game_common::GameSessionMetrics GameSession::Metrics() const
{
    dxa::game_common::GameSessionMetrics metrics;
    metrics.traffic = impl_->traffic.Totals();
    metrics.snapshotsApplied = impl_->appliedSnapshotCount.load();
    metrics.snapshotsDiscarded = impl_->discardedSnapshots.load();
    metrics.snapshotQueueDrops = impl_->droppedSnapshots.load();
    metrics.keyframesApplied = impl_->keyframesApplied.load();
    metrics.keyframeRequests = impl_->keyframeRequests.load();
    return metrics;
}

void GameSession::Stop()
{
    if (impl_)
    {
        impl_->Stop(
            runtime_->Started(),
            runtime_->RunningOnThisThread());
    }
    if (ownsRuntime_)
    {
        runtime_->Stop();
    }
}
} // namespace dxa::game_client
