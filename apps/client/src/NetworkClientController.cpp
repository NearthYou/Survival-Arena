#include <dxa/client/NetworkClientController.hpp>

#include <dxa/client/LobbyHostFlow.hpp>
#include <dxa/game_client/GameSession.hpp>
#include <dxa/game_common/ArenaFingerprint.hpp>
#include <dxa/lobby_client/LobbyClient.hpp>
#include <dxa/simulation/ArenaMap.hpp>

#include <boost/asio.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <variant>

namespace dxa::client
{
namespace
{
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
    return CanonicalArena{
        std::move(navMesh),
        mapId,
        fingerprint};
}

[[nodiscard]] const char* RoomStateName(
    const dxa::protocol::RoomState state)
{
    switch (state)
    {
    case dxa::protocol::RoomState::Waiting:
        return "waiting";
    case dxa::protocol::RoomState::Starting:
        return "starting";
    case dxa::protocol::RoomState::InMatch:
        return "in_match";
    }
    return "unknown";
}
} // namespace

struct NetworkClientController::Impl final
    : public std::enable_shared_from_this<NetworkClientController::Impl>
{
    explicit Impl(NetworkClientOptions sourceOptions)
        : options{std::move(sourceOptions)},
          flow{options.expectedPlayers},
          arena{BuildCanonicalArena()},
          lobby{dxa::lobby_client::LobbyClient::Create(io)}
    {
        if (options.lobbyHost.empty()
            || options.lobbyHost.size() > 255U
            || options.lobbyPort == 0U)
        {
            throw std::invalid_argument{
                "network client lobby endpoint is invalid"};
        }
    }

    void Start()
    {
        if (started.exchange(true))
        {
            throw std::logic_error{
                "network client controller can start only once"};
        }
        if (stopRequested.load())
        {
            throw std::logic_error{
                "stopped network client controller cannot start"};
        }
        work.emplace(boost::asio::make_work_guard(io));
        const std::weak_ptr<Impl> weak = shared_from_this();
        lobby->AsyncConnect(
            options.lobbyHost,
            options.lobbyPort,
            dxa::lobby_client::LobbyClientCallbacks{
                [weak] {
                    if (const auto self = weak.lock())
                    {
                        self->Connected();
                    }
                },
                [weak](dxa::protocol::ServerMessage message) {
                    if (const auto self = weak.lock())
                    {
                        self->Message(std::move(message));
                    }
                },
                [weak](const boost::system::error_code error) {
                    if (const auto self = weak.lock())
                    {
                        self->Closed(error);
                    }
                }});
        lobbyThread = std::thread{[this] { io.run(); }};
    }

    void Connected()
    {
        if (stopRequested.load())
        {
            return;
        }
        try
        {
            static_cast<void>(lobby->Hello());
        }
        catch (const std::exception& error)
        {
            Fail(error.what());
        }
    }

    void Message(dxa::protocol::ServerMessage message)
    {
        if (stopRequested.load() || failed.load())
        {
            return;
        }
        try
        {
            HostCommand command = HostCommand::None;
            if (const auto* welcome =
                    std::get_if<dxa::protocol::ServerWelcome>(&message))
            {
                command = flow.OnWelcome(welcome->player);
                std::cout << "network lobby state=connected\n"
                          << std::flush;
            }
            else if (const auto* snapshot =
                         std::get_if<dxa::protocol::RoomSnapshot>(&message))
            {
                command = flow.OnRoomSnapshot(*snapshot);
                {
                    std::scoped_lock lock{statusMutex};
                    room = snapshot->room;
                }
                const std::size_t readyCount =
                    static_cast<std::size_t>(std::count_if(
                        snapshot->members.begin(),
                        snapshot->members.end(),
                        [](const dxa::protocol::RoomMemberView& member) {
                            return member.ready;
                        }));
                std::cout << "network room=" << snapshot->room.value
                          << " state=" << RoomStateName(snapshot->state)
                          << " players=" << snapshot->members.size()
                          << '/' << static_cast<std::uint32_t>(
                                 options.expectedPlayers)
                          << " ready=" << readyCount << '\n'
                          << std::flush;
            }
            else if (const auto* ticket =
                         std::get_if<dxa::protocol::MatchTicket>(&message))
            {
                flow.OnMatchTicket(*ticket);
                StartGame(*ticket);
                std::cout << "network match=" << ticket->match.value
                          << " state=assigned\n" << std::flush;
                return;
            }
            else if (const auto* error =
                         std::get_if<dxa::protocol::ErrorResponse>(&message))
            {
                flow.OnError(error->error);
                Fail("lobby_error_"
                     + std::to_string(
                         static_cast<std::uint16_t>(error->error)));
                return;
            }
            else if (std::holds_alternative<
                         dxa::protocol::RoomListResponse>(message))
            {
                if (!flow.TicketReceived())
                {
                    throw std::logic_error{
                        "network host received a room list before its match"};
                }
                std::cout << "network room="
                          << flow.Room()->value
                          << " state=closed\n" << std::flush;
                return;
            }
            else
            {
                throw std::logic_error{
                    "network host received an unexpected lobby message"};
            }
            Execute(command);
        }
        catch (const std::exception& error)
        {
            Fail(error.what());
        }
    }

    void Execute(const HostCommand command)
    {
        switch (command)
        {
        case HostCommand::None:
            return;
        case HostCommand::CreateRoom:
            static_cast<void>(lobby->CreateRoom());
            return;
        case HostCommand::SetReady:
            static_cast<void>(lobby->SetReady(true));
            return;
        case HostCommand::StartMatch:
            static_cast<void>(lobby->StartMatch());
            return;
        }
        throw std::logic_error{"network host command is unknown"};
    }

    void StartGame(const dxa::protocol::MatchTicket& ticket)
    {
        const std::optional<dxa::protocol::PlayerId> player = flow.Player();
        if (!player.has_value())
        {
            throw std::logic_error{"network host ticket has no player"};
        }
        auto session = std::make_shared<dxa::game_client::GameSession>(
            arena.navMesh);
        session->Start(dxa::game_client::GameSessionStart{
            *player,
            ticket,
            arena.mapId,
            arena.fingerprint});
        activeMatch.store(ticket.match.value);
        {
            std::scoped_lock lock{sessionMutex};
            if (gameSession)
            {
                session->Stop();
                throw std::logic_error{
                    "network host already owns a game session"};
            }
            if (stopRequested.load())
            {
                session->Stop();
                return;
            }
            gameSession = std::move(session);
        }
    }

    void Closed(const boost::system::error_code error)
    {
        if (!stopRequested.load())
        {
            Fail("lobby connection closed "
                 + std::to_string(error.value()));
        }
    }

    void Fail(std::string reason)
    {
        if (failed.exchange(true))
        {
            return;
        }
        lobby->Close();
        work.reset();
        std::cerr << "network client state=failed reason="
                  << reason << '\n';
    }

    [[nodiscard]] std::shared_ptr<dxa::game_client::GameSession>
    Session() const
    {
        std::scoped_lock lock{sessionMutex};
        return gameSession;
    }

    void FixedUpdate(const dxa::engine::RuntimeInputFrame& input)
    {
        if (failed.load())
        {
            throw std::runtime_error{"network client failed"};
        }
        const auto session = Session();
        if (!session)
        {
            return;
        }
        session->FixedUpdate();
        const std::uint64_t receivedSnapshots = session->SnapshotCount();
        snapshotCount.store(receivedSnapshots);
        if (receivedSnapshots >= 2U
            && !synchronizationReported.exchange(true))
        {
            std::cout << "network match=" << activeMatch.load()
                      << " state=synchronized snapshots="
                      << receivedSnapshots << '\n' << std::flush;
        }
        const dxa::game_client::GameSessionState state = session->State();
        if (state == dxa::game_client::GameSessionState::ProtocolError
            || state == dxa::game_client::GameSessionState::Closed)
        {
            throw std::runtime_error{"game session failed"};
        }
        if (state == dxa::game_client::GameSessionState::Finished
            && !resultReported.exchange(true))
        {
            const auto result = session->Result();
            if (!result.has_value())
            {
                throw std::runtime_error{
                    "game session finished without result"};
            }
            std::cout << "network match=" << result->match.value
                      << " state=finished winner=";
            if (result->hasWinner)
            {
                std::cout << result->winner.value;
            }
            else
            {
                std::cout << "none";
            }
            std::cout << " tick=" << result->finishedTick
                      << '\n' << std::flush;
        }
        if (state == dxa::game_client::GameSessionState::Running
            && input.moveDestination.has_value())
        {
            static_cast<void>(session->SetDestination(
                dxa::simulation::Vec2{
                    input.moveDestination->x,
                    input.moveDestination->z}));
        }
    }

    [[nodiscard]] dxa::engine::RuntimeSceneFrame SampleScene() const
    {
        dxa::engine::RuntimeSceneFrame output;
        const auto session = Session();
        if (!session)
        {
            return output;
        }
        const dxa::game_client::GameSceneFrame source =
            session->SampleScene();
        output.controlledPlayer = {
            source.localPosition.x,
            0.0F,
            source.localPosition.z};
        output.zoneRadius = source.zoneRadius;

        const std::size_t localIndex = source.localActor.value;
        if (localIndex >= output.players.size())
        {
            throw std::logic_error{
                "local actor does not fit a player render slot"};
        }
        output.players[localIndex] = {
            output.controlledPlayer,
            source.connected && source.localAlive};
        for (const dxa::protocol::NetworkActorSnapshot& actor
             : source.actors)
        {
            const dxa::engine::SceneCharacterState state{
                {actor.position.x, 0.0F, actor.position.z},
                actor.alive};
            if (actor.role == dxa::protocol::NetworkActorRole::Contender)
            {
                const std::size_t index = actor.id.value;
                if (index >= output.players.size())
                {
                    throw std::logic_error{
                        "contender does not fit a player render slot"};
                }
                output.players[index] = state;
                continue;
            }
            if (actor.role != dxa::protocol::NetworkActorRole::Neutral
                || actor.id.value < options.expectedPlayers)
            {
                throw std::logic_error{"network actor role or ID is invalid"};
            }
            const std::size_t index = actor.id.value
                - static_cast<std::size_t>(options.expectedPlayers);
            if (index >= output.ai.size())
            {
                throw std::logic_error{
                    "neutral actor does not fit an AI render slot"};
            }
            output.ai[index] = state;
        }
        return output;
    }

    void Stop()
    {
        const bool alreadyStopped = stopRequested.exchange(true);
        const auto session = Session();
        if (session)
        {
            session->Stop();
        }
        if (!alreadyStopped)
        {
            if (lobbyThread.joinable())
            {
                boost::asio::post(io, [this] {
                    lobby->Close();
                    work.reset();
                });
            }
            else
            {
                lobby->Close();
                work.reset();
            }
        }
        if (lobbyThread.joinable()
            && lobbyThread.get_id() != std::this_thread::get_id())
        {
            lobbyThread.join();
        }
    }

    NetworkClientOptions options;
    LobbyHostFlow flow;
    CanonicalArena arena;
    boost::asio::io_context io;
    std::optional<boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type>> work;
    std::shared_ptr<dxa::lobby_client::LobbyClient> lobby;
    std::thread lobbyThread;
    std::atomic<bool> started{false};
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> failed{false};
    std::atomic<std::uint64_t> snapshotCount{0U};
    std::atomic<std::uint64_t> activeMatch{0U};
    std::atomic<bool> synchronizationReported{false};
    std::atomic<bool> resultReported{false};

    mutable std::mutex sessionMutex;
    std::shared_ptr<dxa::game_client::GameSession> gameSession;
    mutable std::mutex statusMutex;
    std::optional<dxa::protocol::RoomId> room;
};

NetworkClientController::NetworkClientController(NetworkClientOptions options)
    : impl_{std::make_shared<Impl>(std::move(options))}
{
}

NetworkClientController::~NetworkClientController()
{
    Stop();
}

void NetworkClientController::Start()
{
    impl_->Start();
}

void NetworkClientController::FixedUpdate(
    const dxa::engine::RuntimeInputFrame& input)
{
    impl_->FixedUpdate(input);
}

dxa::engine::RuntimeSceneFrame NetworkClientController::SampleScene()
{
    return impl_->SampleScene();
}

std::optional<dxa::protocol::RoomId> NetworkClientController::Room() const
{
    std::scoped_lock lock{impl_->statusMutex};
    return impl_->room;
}

std::optional<dxa::protocol::GameMatchResult>
NetworkClientController::Result() const
{
    const auto session = impl_->Session();
    return session ? session->Result() : std::nullopt;
}

std::uint64_t NetworkClientController::SnapshotCount() const noexcept
{
    return impl_->snapshotCount.load();
}

void NetworkClientController::Stop()
{
    impl_->Stop();
}
} // namespace dxa::client
