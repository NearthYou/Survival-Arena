#include <dxa/game_server/AuthoritativeMatch.hpp>

#include <dxa/game_common/ArenaFingerprint.hpp>
#include <dxa/game_common/SnapshotAdapter.hpp>
#include <dxa/game_server/FixedTickScheduler.hpp>
#include <dxa/game_server/GameTicketStore.hpp>
#include <dxa/game_server/ServerMatchMetrics.hpp>
#include <dxa/game_server/SnapshotReplicator.hpp>

#include <dxa/protocol/GameUdpCodec.hpp>
#include <dxa/protocol/LobbyTypes.hpp>
#include <dxa/protocol/ReplicationSnapshotCodec.hpp>
#include <dxa/simulation/OfflineMatch.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace dxa::game_server
{
namespace
{
[[nodiscard]] std::vector<dxa::protocol::PlayerId> ReservationPlayers(
    const dxa::protocol::ReserveMatch& reservation)
{
    std::vector<dxa::protocol::PlayerId> players;
    players.reserve(reservation.participants.size());
    for (const dxa::protocol::ReservedParticipant& participant
         : reservation.participants)
    {
        players.push_back(participant.player);
    }
    return players;
}

[[nodiscard]] dxa::simulation::MatchConfig ServerMatchConfig(
    const dxa::protocol::ReserveMatch& reservation,
    dxa::simulation::MatchConfig config)
{
    config.contenderCount = static_cast<std::uint32_t>(
        reservation.participants.size());
    config.enableInternalBots = false;
    config.seed = reservation.seed;
    return config;
}

[[nodiscard]] dxa::simulation::NavMesh BuildArenaNavMesh(
    const dxa::simulation::ArenaMapDefinition& arena)
{
    return dxa::simulation::NavMesh::Build(
        arena.vertices,
        arena.triangles,
        arena.gridCellSize);
}

[[nodiscard]] dxa::protocol::MatchCompletionReason CompletionReason(
    const dxa::simulation::MatchEndReason reason)
{
    switch (reason)
    {
    case dxa::simulation::MatchEndReason::LastSurvivor:
        return dxa::protocol::MatchCompletionReason::LastSurvivor;
    case dxa::simulation::MatchEndReason::TimeLimit:
        return dxa::protocol::MatchCompletionReason::TimeLimit;
    }
    throw std::invalid_argument{"simulation completion reason is unknown"};
}

[[nodiscard]] const dxa::simulation::ActorSnapshot* FindActor(
    const dxa::simulation::MatchSnapshot& snapshot,
    const dxa::simulation::ActorId actor) noexcept
{
    const auto found = std::lower_bound(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        actor,
        [](const dxa::simulation::ActorSnapshot& candidate,
           const dxa::simulation::ActorId value) {
            return candidate.id < value;
        });
    return found == snapshot.actors.end() || found->id != actor
        ? nullptr
        : &*found;
}

[[nodiscard]] bool IsFinite(
    const dxa::protocol::NetworkVec2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.z);
}

[[nodiscard]] std::size_t ReplicationSampleCapacity(
    const dxa::simulation::MatchConfig& config)
{
    const std::size_t snapshots =
        static_cast<std::size_t>(config.hardTimeoutTick / 2U + 1U);
    if (snapshots
        > std::numeric_limits<std::size_t>::max()
            / dxa::protocol::RoomCapacity)
    {
        throw std::overflow_error{
            "replication metric sample capacity overflow"};
    }
    return snapshots * dxa::protocol::RoomCapacity;
}

[[nodiscard]] std::uint16_t MetricCount(const std::size_t count)
{
    if (count > std::numeric_limits<std::uint16_t>::max())
    {
        throw std::overflow_error{"replication metric visible count overflow"};
    }
    return static_cast<std::uint16_t>(count);
}
} // namespace

struct AuthoritativeMatch::Impl
{
    struct PlayerSession
    {
        GameConnectionId connection;
        dxa::protocol::UdpSessionToken token;
        std::optional<UdpPeer> peer{};
        std::uint32_t acknowledgedInput = 0U;
        std::optional<dxa::simulation::MatchCommand> persistentCommand{};
        bool connected = true;
    };

    Impl(
        const dxa::protocol::ReserveMatch& reservation,
        const dxa::simulation::ArenaMapDefinition& arena,
        dxa::simulation::MatchConfig config,
        IUdpTokenSource& source,
        const std::chrono::steady_clock::time_point now,
        ReplicationConfig replicationConfig)
        : matchId{reservation.match},
          mapId{arena.mapId},
          navMeshCrc32{dxa::game_common::SurvivalArenaFingerprint(arena)},
          navMesh{BuildArenaNavMesh(arena)},
          replicationMode{replicationConfig.mode},
          replicator{arena, std::move(replicationConfig)},
          metrics{
              reservation.match,
              config.hardTimeoutTick,
              ReplicationSampleCapacity(config)},
          simulation{dxa::simulation::OfflineMatch::Create(
              navMesh,
              ServerMatchConfig(reservation, std::move(config)))},
          roster{ReservationPlayers(reservation)},
          scheduler{dxa::protocol::GameTickRate, 5U},
          tokenSource{source},
          ticketExpiresAt{
              now
              + std::chrono::milliseconds{
                  reservation.ticketLifetimeMilliseconds}},
          lastObservedTime{now}
    {
        tickets.Load(
            reservation.match,
            reservation.participants,
            now,
            std::chrono::milliseconds{
                reservation.ticketLifetimeMilliseconds});
        simulation.Start();
    }

    void Stamp(AuthoritativeMatchResult& result) const noexcept
    {
        result.totalOverruns = totalOverruns;
    }

    void AddError(
        AuthoritativeMatchResult& result,
        const GameConnectionId connection,
        const dxa::protocol::GameServerErrorCode error) const
    {
        result.tcp.push_back(GameTcpOutbound{
            connection,
            dxa::protocol::GameServerMessage{
                dxa::protocol::GameServerErrorMessage{error}},
            true});
    }

    void FinishWithoutWinner(
        AuthoritativeMatchResult& result,
        const dxa::protocol::MatchCompletionReason reason)
    {
        if (terminal)
        {
            return;
        }
        const std::uint32_t tick = simulation.Snapshot().tick;
        result.control.push_back(dxa::protocol::WorkerToLobbyMessage{
            dxa::protocol::MatchFinished{
                matchId,
                dxa::protocol::EntityId{},
                false,
                reason,
                tick}});
        terminal = true;
        started = false;
    }

    void FinishFromSimulation(AuthoritativeMatchResult& result)
    {
        if (terminal)
        {
            return;
        }
        const dxa::simulation::MatchSnapshot snapshot = simulation.Snapshot();
        if (!snapshot.result.has_value())
        {
            throw std::logic_error{"finished simulation has no result"};
        }
        const dxa::protocol::MatchCompletionReason reason = CompletionReason(
            snapshot.result->reason);
        const dxa::protocol::EntityId winner{snapshot.result->winner};
        for (const auto& [player, session] : sessions)
        {
            static_cast<void>(player);
            if (!session.connected)
            {
                continue;
            }
            result.tcp.push_back(GameTcpOutbound{
                session.connection,
                dxa::protocol::GameServerMessage{
                    dxa::protocol::GameMatchResult{
                        matchId,
                        winner,
                        true,
                        reason,
                        snapshot.result->finishedTick}},
                true});
        }
        result.control.push_back(dxa::protocol::WorkerToLobbyMessage{
            dxa::protocol::MatchFinished{
                matchId,
                winner,
                true,
                reason,
                snapshot.result->finishedTick}});
        terminal = true;
        started = false;
    }

    void ResolveStart(
        const std::chrono::steady_clock::time_point now,
        AuthoritativeMatchResult& result)
    {
        if (started || terminal || !roster.ReadyToStart())
        {
            return;
        }
        if (roster.AuthenticatedCount() == 0U)
        {
            FinishWithoutWinner(
                result,
                dxa::protocol::MatchCompletionReason::NoAuthenticatedPlayers);
            return;
        }

        for (const dxa::protocol::PlayerId player
             : roster.UnavailablePlayers())
        {
            simulation.Submit(dxa::simulation::MatchLifecycleCommand{
                roster.ActorFor(player).value,
                dxa::simulation::ContenderExitReason::Disconnected});
        }
        scheduler.Start(now);
        started = true;
    }

    void ExpirePending(
        const std::chrono::steady_clock::time_point now,
        AuthoritativeMatchResult& result)
    {
        for (const dxa::protocol::PlayerId player : tickets.PurgeExpired(now))
        {
            static_cast<void>(roster.MarkUnavailable(player));
        }
        ResolveStart(now, result);
    }

    [[nodiscard]] bool TokenAlreadyUsed(
        const dxa::protocol::UdpSessionToken& token) const noexcept
    {
        return std::any_of(
            sessions.begin(),
            sessions.end(),
            [&token](const auto& entry) {
                return entry.second.token == token;
            });
    }

    [[nodiscard]] PlayerSession* SessionFor(
        const dxa::protocol::MatchId match,
        const dxa::protocol::PlayerId player,
        const dxa::protocol::UdpSessionToken& token) noexcept
    {
        if (match != matchId)
        {
            return nullptr;
        }
        const auto session = sessions.find(player);
        if (session == sessions.end()
            || !session->second.connected
            || session->second.token != token)
        {
            return nullptr;
        }
        return &session->second;
    }

    [[nodiscard]] bool BuildPersistentCommand(
        const dxa::protocol::ClientInput& input,
        dxa::simulation::MatchCommand& command) const
    {
        if (!input.hasMoveDestination && !input.hasAttackTarget)
        {
            return false;
        }
        const dxa::protocol::EntityId actor = roster.ActorFor(input.player);
        const dxa::simulation::MatchSnapshot snapshot = simulation.Snapshot();
        const dxa::simulation::ActorSnapshot* source = FindActor(
            snapshot,
            actor.value);
        if (source == nullptr || !source->alive)
        {
            return false;
        }

        command.actor = actor.value;
        if (input.hasMoveDestination)
        {
            if (!IsFinite(input.moveDestination))
            {
                return false;
            }
            const dxa::simulation::Vec2 destination{
                input.moveDestination.x,
                input.moveDestination.z};
            if (!navMesh.FindPath(source->position, destination).has_value())
            {
                return false;
            }
            command.moveDestination = destination;
        }
        if (input.hasAttackTarget)
        {
            if (input.attackTarget == actor)
            {
                return false;
            }
            const dxa::simulation::ActorSnapshot* target = FindActor(
                snapshot,
                input.attackTarget.value);
            if (target == nullptr || !target->alive)
            {
                return false;
            }
            command.attackTarget = input.attackTarget.value;
        }
        return true;
    }

    void EmitSnapshot(AuthoritativeMatchResult& result)
    {
        if (nextSnapshotId == std::numeric_limits<std::uint32_t>::max())
        {
            throw std::overflow_error{"snapshot identity exhausted"};
        }
        const dxa::simulation::MatchSnapshot simulationSnapshot =
            simulation.Snapshot();
        const dxa::protocol::GameSnapshot networkSnapshot =
            dxa::game_common::ToGameSnapshot(simulationSnapshot);
        const std::uint32_t snapshotId = nextSnapshotId++;

        for (const auto& [player, session] : sessions)
        {
            if (!session.connected || !session.peer.has_value())
            {
                continue;
            }
            const auto encodeStartedAt = std::chrono::steady_clock::now();
            const ReplicationBuild build = replicator.Build(
                player,
                snapshotId,
                networkSnapshot);
            const std::vector<std::byte>& payload = build.encodedPayload;
            const auto encodeDuration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - encodeStartedAt);
            const auto fragments = dxa::protocol::FragmentSnapshot(
                matchId,
                snapshotId,
                simulationSnapshot.tick,
                session.acknowledgedInput,
                payload);
            for (const dxa::protocol::SnapshotFragment& fragment : fragments)
            {
                result.udp.push_back(GameUdpOutbound{
                    *session.peer,
                    dxa::protocol::ServerDatagram{fragment}});
            }
            metrics.RecordReplication(
                encodeDuration,
                static_cast<std::uint32_t>(payload.size()),
                MetricCount(fragments.size()),
                build.payload.header.kind
                    != dxa::protocol::SnapshotPayloadKind::Delta,
                MetricCount(build.visibleActorCount),
                MetricCount(build.visibleLootCount),
                build.fallbackKeyframe);
        }
    }

    dxa::protocol::MatchId matchId;
    std::uint32_t mapId = 1U;
    std::uint32_t navMeshCrc32 = 0U;
    dxa::simulation::NavMesh navMesh;
    dxa::protocol::ReplicationMode replicationMode =
        dxa::protocol::ReplicationMode::FullState;
    SnapshotReplicator replicator;
    ServerMatchMetrics metrics;
    dxa::simulation::OfflineMatch simulation;
    ParticipantRoster roster;
    GameTicketStore tickets;
    FixedTickScheduler scheduler;
    IUdpTokenSource& tokenSource;
    std::chrono::steady_clock::time_point ticketExpiresAt;
    std::chrono::steady_clock::time_point lastObservedTime;
    std::map<dxa::protocol::PlayerId, PlayerSession> sessions;
    std::map<GameConnectionId, dxa::protocol::PlayerId> connections;
    std::set<dxa::simulation::ActorId> pendingDisconnects;
    std::uint32_t nextSnapshotId = 1U;
    std::uint64_t totalOverruns = 0U;
    bool started = false;
    bool terminal = false;
};

AuthoritativeMatch AuthoritativeMatch::Create(
    const dxa::protocol::ReserveMatch& reservation,
    const dxa::simulation::ArenaMapDefinition& arena,
    dxa::simulation::MatchConfig config,
    IUdpTokenSource& tokenSource,
    const std::chrono::steady_clock::time_point now,
    ReplicationConfig replication)
{
    if (reservation.reservation.value == 0U
        || reservation.ticketLifetimeMilliseconds == 0U
        || reservation.ticketLifetimeMilliseconds
            > dxa::protocol::MatchTicketLifetimeSeconds * 1000U)
    {
        throw std::invalid_argument{"game reservation lifetime is invalid"};
    }
    return AuthoritativeMatch{std::make_unique<Impl>(
        reservation,
        arena,
        std::move(config),
        tokenSource,
        now,
        std::move(replication))};
}

AuthoritativeMatch::AuthoritativeMatch(std::unique_ptr<Impl> impl) noexcept
    : impl_{std::move(impl)}
{
}

AuthoritativeMatch::~AuthoritativeMatch() = default;
AuthoritativeMatch::AuthoritativeMatch(AuthoritativeMatch&&) noexcept = default;
AuthoritativeMatch& AuthoritativeMatch::operator=(
    AuthoritativeMatch&&) noexcept = default;

AuthoritativeMatchResult AuthoritativeMatch::Authenticate(
    const GameConnectionId connection,
    const dxa::protocol::GameClientHello& hello,
    const std::chrono::steady_clock::time_point now)
{
    if (impl_ == nullptr)
    {
        throw std::logic_error{"authoritative match has been moved from"};
    }
    Impl& state = *impl_;
    state.lastObservedTime = now;
    AuthoritativeMatchResult result;
    state.ExpirePending(now, result);
    if (state.terminal || state.started
        || connection.value == 0U
        || state.connections.contains(connection)
        || state.sessions.contains(hello.player))
    {
        state.AddError(
            result,
            connection,
            dxa::protocol::GameServerErrorCode::AuthenticationFailed);
        state.Stamp(result);
        return result;
    }

    dxa::protocol::EntityId actor;
    try
    {
        actor = state.roster.ActorFor(hello.player);
    }
    catch (const std::out_of_range&)
    {
        state.AddError(
            result,
            connection,
            dxa::protocol::GameServerErrorCode::AuthenticationFailed);
        state.Stamp(result);
        return result;
    }

    const GameTicketConsumeResult consumed = state.tickets.Consume(
        hello.ticket,
        hello.match,
        hello.player,
        now);
    if (consumed != GameTicketConsumeResult::Accepted)
    {
        state.AddError(
            result,
            connection,
            dxa::protocol::GameServerErrorCode::AuthenticationFailed);
        state.Stamp(result);
        return result;
    }

    dxa::protocol::UdpSessionToken token;
    if (!state.tokenSource.Fill(token) || state.TokenAlreadyUsed(token))
    {
        static_cast<void>(state.roster.MarkUnavailable(hello.player));
        state.AddError(
            result,
            connection,
            dxa::protocol::GameServerErrorCode::InternalError);
        state.ResolveStart(now, result);
        state.Stamp(result);
        return result;
    }
    if (!state.roster.Authenticate(hello.player, connection, token))
    {
        static_cast<void>(state.roster.MarkUnavailable(hello.player));
        state.AddError(
            result,
            connection,
            dxa::protocol::GameServerErrorCode::InternalError);
        state.ResolveStart(now, result);
        state.Stamp(result);
        return result;
    }

    state.sessions.emplace(
        hello.player,
        Impl::PlayerSession{connection, token});
    state.connections.emplace(connection, hello.player);
    state.replicator.RegisterRecipient(hello.player, actor);
    result.tcp.push_back(GameTcpOutbound{
        connection,
        dxa::protocol::GameServerMessage{
            dxa::protocol::GameServerWelcome{
                state.matchId,
                hello.player,
                actor,
                dxa::protocol::GameTickRate,
                dxa::protocol::SnapshotRate,
                state.mapId,
                state.navMeshCrc32,
                state.replicationMode,
                token}},
        false});
    state.ResolveStart(now, result);
    state.Stamp(result);
    return result;
}

AuthoritativeMatchResult AuthoritativeMatch::ReceiveClientDatagram(
    const UdpPeer peer,
    const dxa::protocol::ClientDatagram& datagram)
{
    if (impl_ == nullptr)
    {
        throw std::logic_error{"authoritative match has been moved from"};
    }
    Impl& state = *impl_;
    AuthoritativeMatchResult result;
    if (state.terminal || peer.port == 0U)
    {
        state.Stamp(result);
        return result;
    }

    if (const auto* bind = std::get_if<dxa::protocol::UdpBind>(&datagram))
    {
        Impl::PlayerSession* session = state.SessionFor(
            bind->match,
            bind->player,
            bind->token);
        if (session == nullptr)
        {
            state.Stamp(result);
            return result;
        }
        if (session->peer.has_value() && *session->peer != peer)
        {
            state.Stamp(result);
            return result;
        }
        session->peer = peer;
        result.udp.push_back(GameUdpOutbound{
            peer,
            dxa::protocol::ServerDatagram{
                dxa::protocol::UdpBindAccepted{
                    state.matchId,
                    bind->player,
                    state.simulation.Snapshot().tick}}});
        state.Stamp(result);
        return result;
    }

    const auto& input = std::get<dxa::protocol::ClientInput>(datagram);
    Impl::PlayerSession* session = state.SessionFor(
        input.match,
        input.player,
        input.token);
    if (!state.started
        || session == nullptr
        || !session->peer.has_value()
        || *session->peer != peer
        || input.inputSequence == 0U
        || input.inputSequence <= session->acknowledgedInput)
    {
        state.Stamp(result);
        return result;
    }

    if (input.acknowledgedSnapshotId != 0U
        && !state.replicator.AcceptAcknowledgement(
            input.player,
            input.acknowledgedSnapshotId))
    {
        state.AddError(
            result,
            session->connection,
            dxa::protocol::GameServerErrorCode::ProtocolViolation);
        state.Stamp(result);
        return result;
    }
    if (input.requestKeyframe)
    {
        state.replicator.RequestKeyframe(input.player);
    }

    session->acknowledgedInput = input.inputSequence;
    dxa::simulation::MatchCommand command;
    if (state.BuildPersistentCommand(input, command))
    {
        session->persistentCommand = std::move(command);
    }
    state.Stamp(result);
    return result;
}

AuthoritativeMatchResult AuthoritativeMatch::Disconnect(
    const GameConnectionId connection)
{
    if (impl_ == nullptr)
    {
        throw std::logic_error{"authoritative match has been moved from"};
    }
    Impl& state = *impl_;
    AuthoritativeMatchResult result;
    const auto connected = state.connections.find(connection);
    if (connected == state.connections.end() || state.terminal)
    {
        state.Stamp(result);
        return result;
    }

    const dxa::protocol::PlayerId player = connected->second;
    state.connections.erase(connected);
    auto session = state.sessions.find(player);
    if (session == state.sessions.end())
    {
        throw std::logic_error{"game connection has no participant session"};
    }
    session->second.connected = false;
    session->second.peer.reset();
    session->second.persistentCommand.reset();
    state.replicator.RemoveRecipient(player);
    static_cast<void>(state.roster.MarkUnavailable(player));

    if (!state.started)
    {
        state.ResolveStart(state.lastObservedTime, result);
    }
    else if (state.connections.empty())
    {
        state.FinishWithoutWinner(
            result,
            dxa::protocol::MatchCompletionReason::NoConnectedPlayers);
    }
    else
    {
        state.pendingDisconnects.insert(state.roster.ActorFor(player).value);
    }
    state.Stamp(result);
    return result;
}

AuthoritativeMatchResult AuthoritativeMatch::Advance(
    const std::chrono::steady_clock::time_point now)
{
    if (impl_ == nullptr)
    {
        throw std::logic_error{"authoritative match has been moved from"};
    }
    Impl& state = *impl_;
    state.lastObservedTime = now;
    AuthoritativeMatchResult result;
    if (state.terminal)
    {
        state.Stamp(result);
        return result;
    }
    if (!state.started)
    {
        state.ExpirePending(now, result);
        if (state.terminal || !state.started)
        {
            state.Stamp(result);
            return result;
        }
    }

    const TickAdvanceResult advance = state.scheduler.Advance(now);
    result.overrun = advance.rebased;
    result.overrunLateness = advance.lateness;
    if (advance.rebased)
    {
        ++state.totalOverruns;
    }

    for (std::uint32_t due = 0U; due < advance.ticksDue; ++due)
    {
        const auto tickStartedAt = std::chrono::steady_clock::now();
        for (const auto& [player, session] : state.sessions)
        {
            static_cast<void>(player);
            if (session.connected && session.persistentCommand.has_value())
            {
                state.simulation.Submit(*session.persistentCommand);
            }
        }
        for (const dxa::simulation::ActorId actor : state.pendingDisconnects)
        {
            state.simulation.Submit(dxa::simulation::MatchLifecycleCommand{
                actor,
                dxa::simulation::ContenderExitReason::Disconnected});
        }
        state.pendingDisconnects.clear();

        state.simulation.Step();
        ++result.ticksExecuted;
        static_cast<void>(state.simulation.DrainEvents());
        const dxa::simulation::MatchSnapshot snapshot =
            state.simulation.Snapshot();
        if (snapshot.result.has_value())
        {
            state.FinishFromSimulation(result);
        }
        if (snapshot.tick % 2U == 0U)
        {
            state.EmitSnapshot(result);
        }
        state.metrics.RecordTick(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - tickStartedAt));
        if (state.terminal)
        {
            break;
        }
    }
    state.Stamp(result);
    return result;
}

std::optional<std::chrono::steady_clock::time_point>
AuthoritativeMatch::NextDeadline() const
{
    if (impl_ == nullptr)
    {
        throw std::logic_error{"authoritative match has been moved from"};
    }
    if (impl_->terminal)
    {
        return std::nullopt;
    }
    if (impl_->started)
    {
        return impl_->scheduler.NextDeadline();
    }
    return impl_->ticketExpiresAt;
}

bool AuthoritativeMatch::Started() const noexcept
{
    return impl_ != nullptr && impl_->started && !impl_->terminal;
}

dxa::protocol::GameSnapshot AuthoritativeMatch::Snapshot() const
{
    if (impl_ == nullptr)
    {
        throw std::logic_error{"authoritative match has been moved from"};
    }
    return dxa::game_common::ToGameSnapshot(impl_->simulation.Snapshot());
}

ServerMatchMetricsSnapshot AuthoritativeMatch::Metrics(
    const dxa::game_common::GameTrafficTotals traffic) const
{
    if (impl_ == nullptr)
    {
        throw std::logic_error{"authoritative match has been moved from"};
    }
    return impl_->metrics.Snapshot(traffic, impl_->totalOverruns);
}
} // namespace dxa::game_server
