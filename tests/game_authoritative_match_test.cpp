#include <dxa/game_common/ArenaFingerprint.hpp>
#include <dxa/game_common/SnapshotAdapter.hpp>
#include <dxa/game_server/AuthoritativeMatch.hpp>
#include <dxa/game_server/UdpTokenSource.hpp>

#include <dxa/protocol/GameSnapshotCodec.hpp>
#include <dxa/protocol/GameUdpCodec.hpp>
#include <dxa/simulation/MatchConfig.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using namespace std::chrono_literals;
using dxa::game_server::AuthoritativeMatch;
using dxa::game_server::AuthoritativeMatchResult;
using dxa::game_server::GameConnectionId;
using dxa::game_server::GameTcpOutbound;
using dxa::game_server::GameUdpOutbound;
using dxa::game_server::IUdpTokenSource;
using dxa::game_server::UdpPeer;
using dxa::protocol::ClientDatagram;
using dxa::protocol::ClientInput;
using dxa::protocol::EntityId;
using dxa::protocol::GameClientHello;
using dxa::protocol::GameMatchResult;
using dxa::protocol::GameServerErrorCode;
using dxa::protocol::GameServerErrorMessage;
using dxa::protocol::GameServerWelcome;
using dxa::protocol::GameSnapshot;
using dxa::protocol::MatchCompletionReason;
using dxa::protocol::MatchId;
using dxa::protocol::MatchTicketValue;
using dxa::protocol::NetworkActorRole;
using dxa::protocol::NetworkMatchPhase;
using dxa::protocol::PlayerId;
using dxa::protocol::ReserveMatch;
using dxa::protocol::ReservedParticipant;
using dxa::protocol::ServerDatagram;
using dxa::protocol::SnapshotFragment;
using dxa::protocol::UdpBind;
using dxa::protocol::UdpBindAccepted;
using dxa::protocol::UdpSessionToken;
using dxa::protocol::WorkerToLobbyMessage;
using dxa::simulation::ArenaMapDefinition;
using dxa::simulation::DefaultMatchConfig;
using dxa::simulation::NavTriangleIndices;
using dxa::simulation::SurvivalArenaMapDefinition;

constexpr PlayerId PlayerA{2U};
constexpr PlayerId PlayerB{9U};
constexpr MatchId Match{7U};

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

[[nodiscard]] std::chrono::steady_clock::time_point TimeNs(
    const std::uint64_t nanoseconds)
{
    return std::chrono::steady_clock::time_point{
        std::chrono::nanoseconds{static_cast<std::int64_t>(nanoseconds)}};
}

[[nodiscard]] ReserveMatch Reservation(
    const std::uint32_t participantCount = 2U,
    const std::uint32_t lifetimeMilliseconds = 60000U)
{
    std::vector<ReservedParticipant> participants;
    participants.reserve(participantCount);
    if (participantCount >= 1U)
    {
        participants.push_back({PlayerB, Ticket(9U)});
    }
    if (participantCount >= 2U)
    {
        participants.push_back({PlayerA, Ticket(2U)});
    }
    for (std::uint32_t index = 2U; index < participantCount; ++index)
    {
        participants.push_back({
            PlayerId{index + 10U},
            Ticket(static_cast<std::uint8_t>(index + 10U))});
    }
    return ReserveMatch{
        dxa::protocol::ReservationId{1U},
        Match,
        20260824U,
        lifetimeMilliseconds,
        std::move(participants)};
}

[[nodiscard]] GameClientHello Hello(
    const PlayerId player,
    const MatchTicketValue& ticket)
{
    return {Match, player, ticket};
}

class DeterministicUdpTokenSource final : public IUdpTokenSource
{
public:
    explicit DeterministicUdpTokenSource(
        const std::optional<std::size_t> failAt = std::nullopt)
        : failAt_{failAt}
    {
    }

    [[nodiscard]] bool Fill(
        const std::span<std::byte, 16U> output) noexcept override
    {
        const std::size_t call = calls_++;
        if (failAt_.has_value() && call == *failAt_)
        {
            return false;
        }
        const UdpSessionToken token = Token(
            static_cast<std::uint8_t>(call + 1U));
        std::copy(token.begin(), token.end(), output.begin());
        return true;
    }

private:
    std::optional<std::size_t> failAt_;
    std::size_t calls_ = 0U;
};

[[nodiscard]] AuthoritativeMatch CreateMatch(
    DeterministicUdpTokenSource& tokens,
    const std::chrono::steady_clock::time_point now = TimeNs(0U),
    const ReserveMatch& reservation = Reservation(),
    const ArenaMapDefinition& arena = SurvivalArenaMapDefinition())
{
    return AuthoritativeMatch::Create(
        reservation,
        arena,
        DefaultMatchConfig(),
        tokens,
        now);
}

template <typename Message>
[[nodiscard]] const Message& OnlyTcpMessage(
    const AuthoritativeMatchResult& result)
{
    if (result.tcp.size() != 1U)
    {
        throw std::logic_error{"expected one game TCP message"};
    }
    const auto* message = std::get_if<Message>(&result.tcp.front().message);
    if (message == nullptr)
    {
        throw std::logic_error{"unexpected game TCP message"};
    }
    return *message;
}

[[nodiscard]] GameServerErrorCode OnlyError(
    const AuthoritativeMatchResult& result)
{
    const GameServerErrorMessage& error =
        OnlyTcpMessage<GameServerErrorMessage>(result);
    if (!result.tcp.front().closeAfterWrite)
    {
        throw std::logic_error{"game error must close after write"};
    }
    return error.error;
}

[[nodiscard]] const dxa::protocol::MatchFinished& OnlyControlFinished(
    const AuthoritativeMatchResult& result)
{
    if (result.control.size() != 1U)
    {
        throw std::logic_error{"expected one worker completion"};
    }
    const auto* finished = std::get_if<dxa::protocol::MatchFinished>(
        &result.control.front());
    if (finished == nullptr)
    {
        throw std::logic_error{"unexpected worker control message"};
    }
    return *finished;
}

[[nodiscard]] UdpPeer Peer(
    const std::uint8_t suffix,
    const std::uint16_t port)
{
    UdpPeer peer;
    peer.address[0] = std::byte{127U};
    peer.address[3] = static_cast<std::byte>(suffix);
    peer.port = port;
    return peer;
}

[[nodiscard]] GameServerWelcome Authenticate(
    AuthoritativeMatch& match,
    const GameConnectionId connection,
    const PlayerId player,
    const MatchTicketValue& ticket,
    const std::chrono::steady_clock::time_point now = TimeNs(0U))
{
    const AuthoritativeMatchResult result = match.Authenticate(
        connection,
        Hello(player, ticket),
        now);
    return OnlyTcpMessage<GameServerWelcome>(result);
}

struct StartedPlayers
{
    UdpSessionToken tokenA;
    UdpSessionToken tokenB;
};

[[nodiscard]] StartedPlayers StartPlayers(
    AuthoritativeMatch& match)
{
    const GameServerWelcome first = Authenticate(
        match,
        GameConnectionId{10U},
        PlayerA,
        Ticket(2U));
    const GameServerWelcome second = Authenticate(
        match,
        GameConnectionId{11U},
        PlayerB,
        Ticket(9U));
    return {first.udpToken, second.udpToken};
}

void Bind(
    AuthoritativeMatch& match,
    const UdpPeer peer,
    const PlayerId player,
    const UdpSessionToken& token)
{
    const AuthoritativeMatchResult result = match.ReceiveClientDatagram(
        peer,
        ClientDatagram{UdpBind{Match, player, token}});
    if (result.udp.size() != 1U
        || !std::holds_alternative<UdpBindAccepted>(
            result.udp.front().datagram))
    {
        throw std::logic_error{"UDP bind failed"};
    }
}

[[nodiscard]] ClientInput MoveInput(
    const PlayerId player,
    const UdpSessionToken& token,
    const std::uint32_t sequence,
    const float x,
    const float z)
{
    ClientInput input;
    input.match = Match;
    input.player = player;
    input.token = token;
    input.inputSequence = sequence;
    input.moveDestination = {x, z};
    input.hasMoveDestination = true;
    return input;
}

[[nodiscard]] const SnapshotFragment* FirstFragmentFor(
    const AuthoritativeMatchResult& result,
    const UdpPeer peer)
{
    for (const GameUdpOutbound& outbound : result.udp)
    {
        if (outbound.recipient != peer)
        {
            continue;
        }
        if (const auto* fragment =
                std::get_if<SnapshotFragment>(&outbound.datagram))
        {
            return fragment;
        }
    }
    return nullptr;
}

[[nodiscard]] GameSnapshot SnapshotFor(
    const AuthoritativeMatchResult& result,
    const UdpPeer peer)
{
    std::vector<const SnapshotFragment*> fragments;
    for (const GameUdpOutbound& outbound : result.udp)
    {
        if (outbound.recipient == peer)
        {
            if (const auto* fragment =
                    std::get_if<SnapshotFragment>(&outbound.datagram))
            {
                fragments.push_back(fragment);
            }
        }
    }
    if (fragments.empty())
    {
        throw std::logic_error{"snapshot fragments are absent"};
    }
    std::sort(
        fragments.begin(),
        fragments.end(),
        [](const SnapshotFragment* left, const SnapshotFragment* right) {
            return left->fragmentIndex < right->fragmentIndex;
        });
    std::vector<std::byte> payload;
    for (const SnapshotFragment* fragment : fragments)
    {
        payload.insert(
            payload.end(),
            fragment->bytes.begin(),
            fragment->bytes.end());
    }
    const auto decoded = dxa::protocol::DecodeGameSnapshot(payload);
    if (!decoded.snapshot.has_value())
    {
        throw std::logic_error{"snapshot payload decode failed"};
    }
    return *decoded.snapshot;
}

[[nodiscard]] ArenaMapDefinition DisconnectedArena()
{
    ArenaMapDefinition arena = SurvivalArenaMapDefinition();
    arena.vertices.push_back({200.0F, 200.0F});
    arena.vertices.push_back({220.0F, 200.0F});
    arena.vertices.push_back({200.0F, 220.0F});
    arena.triangles.push_back(NavTriangleIndices{{4U, 5U, 6U}});
    return arena;
}
} // namespace

TEST(AuthoritativeMatch, CreatesReservedPopulationWithoutInternalContenderBots)
{
    DeterministicUdpTokenSource tokens;
    dxa::simulation::MatchConfig config = DefaultMatchConfig();
    config.contenderCount = 24U;
    config.enableInternalBots = true;
    AuthoritativeMatch match = AuthoritativeMatch::Create(
        Reservation(),
        SurvivalArenaMapDefinition(),
        config,
        tokens,
        TimeNs(0U));

    const GameSnapshot snapshot = match.Snapshot();
    EXPECT_FALSE(match.Started());
    EXPECT_EQ(2U, snapshot.aliveContenders);
    EXPECT_EQ(102U, snapshot.actors.size());
    EXPECT_EQ(NetworkActorRole::Contender, snapshot.actors[0].role);
    EXPECT_EQ(NetworkActorRole::Contender, snapshot.actors[1].role);

    AuthoritativeMatch maximum = AuthoritativeMatch::Create(
        Reservation(24U),
        SurvivalArenaMapDefinition(),
        config,
        tokens,
        TimeNs(0U));
    EXPECT_EQ(124U, maximum.Snapshot().actors.size());
    EXPECT_NO_THROW(
        (void)dxa::protocol::EncodeGameSnapshot(maximum.Snapshot()));

    EXPECT_THROW(
        (void)AuthoritativeMatch::Create(
            Reservation(1U),
            SurvivalArenaMapDefinition(),
            config,
            tokens,
            TimeNs(0U)),
        std::invalid_argument);
    EXPECT_THROW(
        (void)AuthoritativeMatch::Create(
            Reservation(25U),
            SurvivalArenaMapDefinition(),
            config,
            tokens,
            TimeNs(0U)),
        std::invalid_argument);
}

TEST(AuthoritativeMatch, WaitsForEverySlotAndMapsSortedPlayersToActors)
{
    DeterministicUdpTokenSource tokens;
    AuthoritativeMatch match = CreateMatch(tokens);

    const GameServerWelcome first = Authenticate(
        match, GameConnectionId{10U}, PlayerB, Ticket(9U));
    EXPECT_EQ(EntityId{1U}, first.actor);
    EXPECT_FALSE(match.Started());

    const GameServerWelcome second = Authenticate(
        match, GameConnectionId{11U}, PlayerA, Ticket(2U));
    EXPECT_EQ(EntityId{0U}, second.actor);
    EXPECT_TRUE(match.Started());
    EXPECT_EQ(NetworkMatchPhase::Running, match.Snapshot().phase);
    EXPECT_EQ(1U, second.mapId);
    EXPECT_EQ(
        dxa::game_common::SurvivalArenaFingerprint(
            SurvivalArenaMapDefinition()),
        second.navMeshCrc32);
}

TEST(AuthoritativeMatch, AuthenticationFailuresSharePublicErrorAndMismatchPreservesTicket)
{
    DeterministicUdpTokenSource tokens;
    AuthoritativeMatch match = CreateMatch(tokens);

    EXPECT_EQ(
        GameServerErrorCode::AuthenticationFailed,
        OnlyError(match.Authenticate(
            GameConnectionId{1U},
            Hello(PlayerB, Ticket(2U)),
            TimeNs(0U))));
    EXPECT_EQ(
        GameServerErrorCode::AuthenticationFailed,
        OnlyError(match.Authenticate(
            GameConnectionId{2U},
            Hello(PlayerA, Ticket(99U)),
            TimeNs(0U))));

    static_cast<void>(Authenticate(
        match, GameConnectionId{3U}, PlayerA, Ticket(2U)));
    EXPECT_EQ(
        GameServerErrorCode::AuthenticationFailed,
        OnlyError(match.Authenticate(
            GameConnectionId{4U},
            Hello(PlayerA, Ticket(2U)),
            TimeNs(0U))));
}

TEST(AuthoritativeMatch, ExactExpiryFailsAuthenticationAndFinishesWithoutClients)
{
    DeterministicUdpTokenSource tokens;
    AuthoritativeMatch match = CreateMatch(
        tokens,
        TimeNs(0U),
        Reservation(2U, 100U));

    const AuthoritativeMatchResult result = match.Authenticate(
        GameConnectionId{1U},
        Hello(PlayerA, Ticket(2U)),
        TimeNs(100000000ULL));

    EXPECT_EQ(GameServerErrorCode::AuthenticationFailed, OnlyError(result));
    const auto& finished = OnlyControlFinished(result);
    EXPECT_EQ(MatchCompletionReason::NoAuthenticatedPlayers, finished.reason);
    EXPECT_FALSE(finished.hasWinner);
    EXPECT_FALSE(match.NextDeadline().has_value());
}

TEST(AuthoritativeMatch, TokenFailureUsesInternalErrorAndResolvesFailedSlot)
{
    DeterministicUdpTokenSource tokens{0U};
    AuthoritativeMatch match = CreateMatch(tokens);

    EXPECT_EQ(
        GameServerErrorCode::InternalError,
        OnlyError(match.Authenticate(
            GameConnectionId{10U},
            Hello(PlayerA, Ticket(2U)),
            TimeNs(0U))));
    EXPECT_FALSE(match.Started());

    static_cast<void>(Authenticate(
        match, GameConnectionId{11U}, PlayerB, Ticket(9U)));
    EXPECT_TRUE(match.Started());
}

TEST(AuthoritativeMatch, UdpBindIsIdempotentAndRejectsWrongIdentityOrRebind)
{
    DeterministicUdpTokenSource tokens;
    AuthoritativeMatch match = CreateMatch(tokens);
    const StartedPlayers players = StartPlayers(match);
    const UdpPeer peerA = Peer(1U, 9001U);
    const UdpPeer peerB = Peer(2U, 9002U);

    const auto bind = ClientDatagram{UdpBind{
        Match, PlayerA, players.tokenA}};
    EXPECT_EQ(1U, match.ReceiveClientDatagram(peerA, bind).udp.size());
    EXPECT_EQ(1U, match.ReceiveClientDatagram(peerA, bind).udp.size());
    EXPECT_TRUE(match.ReceiveClientDatagram(peerB, bind).udp.empty());
    EXPECT_TRUE(match.ReceiveClientDatagram(
        peerA,
        ClientDatagram{UdpBind{Match, PlayerA, Token(99U)}}).udp.empty());
    EXPECT_TRUE(match.ReceiveClientDatagram(
        peerA,
        ClientDatagram{UdpBind{MatchId{8U}, PlayerA, players.tokenA}})
                    .udp.empty());
}

TEST(AuthoritativeMatch, InvalidNewInputAdvancesAckButKeepsPreviousValidCommand)
{
    DeterministicUdpTokenSource tokens;
    const ArenaMapDefinition arena = DisconnectedArena();
    AuthoritativeMatch match = CreateMatch(
        tokens, TimeNs(0U), Reservation(), arena);
    const StartedPlayers players = StartPlayers(match);
    const UdpPeer peerA = Peer(1U, 9001U);
    Bind(match, peerA, PlayerA, players.tokenA);
    const GameSnapshot before = match.Snapshot();
    const auto actor = std::find_if(
        before.actors.begin(),
        before.actors.end(),
        [](const auto& candidate) { return candidate.id == EntityId{0U}; });
    ASSERT_NE(before.actors.end(), actor);
    const float beforeDistance = std::hypot(actor->position.x, actor->position.z);

    static_cast<void>(match.ReceiveClientDatagram(
        peerA,
        ClientDatagram{MoveInput(
            PlayerA, players.tokenA, 1U, 0.0F, 0.0F)}));
    static_cast<void>(match.ReceiveClientDatagram(
        peerA,
        ClientDatagram{MoveInput(
            PlayerA, players.tokenA, 2U, 999.0F, 999.0F)}));
    static_cast<void>(match.ReceiveClientDatagram(
        peerA,
        ClientDatagram{MoveInput(
            PlayerA,
            players.tokenA,
            3U,
            std::numeric_limits<float>::quiet_NaN(),
            0.0F)}));
    static_cast<void>(match.ReceiveClientDatagram(
        peerA,
        ClientDatagram{MoveInput(
            PlayerA, players.tokenA, 4U, 205.0F, 205.0F)}));

    ClientInput missingTarget;
    missingTarget.match = Match;
    missingTarget.player = PlayerA;
    missingTarget.token = players.tokenA;
    missingTarget.inputSequence = 5U;
    missingTarget.hasAttackTarget = true;
    missingTarget.attackTarget = EntityId{999U};
    static_cast<void>(match.ReceiveClientDatagram(
        peerA, ClientDatagram{missingTarget}));
    missingTarget.inputSequence = 6U;
    missingTarget.attackTarget = EntityId{0U};
    static_cast<void>(match.ReceiveClientDatagram(
        peerA, ClientDatagram{missingTarget}));

    const AuthoritativeMatchResult output = match.Advance(
        TimeNs(66666666ULL));
    const SnapshotFragment* fragment = FirstFragmentFor(output, peerA);
    ASSERT_NE(nullptr, fragment);
    EXPECT_EQ(6U, fragment->ackInputSequence);
    const GameSnapshot after = SnapshotFor(output, peerA);
    const auto moved = std::find_if(
        after.actors.begin(),
        after.actors.end(),
        [](const auto& candidate) { return candidate.id == EntityId{0U}; });
    ASSERT_NE(after.actors.end(), moved);
    EXPECT_LT(std::hypot(moved->position.x, moved->position.z), beforeDistance);
}

TEST(AuthoritativeMatch, IgnoresOldDuplicateAndPostMaximumInputSequences)
{
    DeterministicUdpTokenSource tokens;
    AuthoritativeMatch match = CreateMatch(tokens);
    const StartedPlayers players = StartPlayers(match);
    const UdpPeer peerA = Peer(1U, 9001U);
    Bind(match, peerA, PlayerA, players.tokenA);

    static_cast<void>(match.ReceiveClientDatagram(
        peerA,
        ClientDatagram{MoveInput(
            PlayerA,
            players.tokenA,
            std::numeric_limits<std::uint32_t>::max(),
            0.0F,
            0.0F)}));
    static_cast<void>(match.ReceiveClientDatagram(
        peerA,
        ClientDatagram{MoveInput(
            PlayerA, players.tokenA, 1U, 20.0F, 20.0F)}));
    static_cast<void>(match.ReceiveClientDatagram(
        peerA,
        ClientDatagram{MoveInput(
            PlayerA,
            players.tokenA,
            std::numeric_limits<std::uint32_t>::max(),
            20.0F,
            20.0F)}));

    const AuthoritativeMatchResult output = match.Advance(
        TimeNs(66666666ULL));
    ASSERT_NE(nullptr, FirstFragmentFor(output, peerA));
    EXPECT_EQ(
        std::numeric_limits<std::uint32_t>::max(),
        FirstFragmentFor(output, peerA)->ackInputSequence);
}

TEST(AuthoritativeMatch, EmitsBoundedRecipientSnapshotsEverySecondTick)
{
    DeterministicUdpTokenSource tokens;
    AuthoritativeMatch match = CreateMatch(tokens);
    const StartedPlayers players = StartPlayers(match);
    const UdpPeer peerA = Peer(1U, 9001U);
    const UdpPeer peerB = Peer(2U, 9002U);
    Bind(match, peerA, PlayerA, players.tokenA);

    const AuthoritativeMatchResult first = match.Advance(
        TimeNs(33333333ULL));
    EXPECT_EQ(1U, first.ticksExecuted);
    EXPECT_TRUE(first.udp.empty());

    static_cast<void>(match.ReceiveClientDatagram(
        peerA,
        ClientDatagram{MoveInput(
            PlayerA, players.tokenA, 3U, 0.0F, 0.0F)}));
    const AuthoritativeMatchResult second = match.Advance(
        TimeNs(66666666ULL));
    EXPECT_EQ(1U, second.ticksExecuted);
    ASSERT_FALSE(second.udp.empty());
    EXPECT_EQ(nullptr, FirstFragmentFor(second, peerB));
    EXPECT_EQ(3U, FirstFragmentFor(second, peerA)->ackInputSequence);
    for (const GameUdpOutbound& outbound : second.udp)
    {
        EXPECT_LE(
            dxa::protocol::EncodeServerDatagram(outbound.datagram).bytes.size(),
            dxa::protocol::MaxUdpDatagramBytes);
    }

    Bind(match, peerB, PlayerB, players.tokenB);
    const AuthoritativeMatchResult fourth = match.Advance(
        TimeNs(133333333ULL));
    ASSERT_NE(nullptr, FirstFragmentFor(fourth, peerA));
    ASSERT_NE(nullptr, FirstFragmentFor(fourth, peerB));
    EXPECT_EQ(
        FirstFragmentFor(fourth, peerA)->snapshotId,
        FirstFragmentFor(fourth, peerB)->snapshotId);
    EXPECT_EQ(
        FirstFragmentFor(fourth, peerA)->fullPayloadCrc32,
        FirstFragmentFor(fourth, peerB)->fullPayloadCrc32);
}

TEST(AuthoritativeMatch, PropagatesBoundedCatchUpAndCumulativeOverruns)
{
    DeterministicUdpTokenSource tokens;
    AuthoritativeMatch match = CreateMatch(tokens);
    static_cast<void>(StartPlayers(match));

    const AuthoritativeMatchResult overrun = match.Advance(
        TimeNs(1000000000ULL));

    EXPECT_EQ(5U, overrun.ticksExecuted);
    EXPECT_TRUE(overrun.overrun);
    EXPECT_EQ(std::chrono::milliseconds{800}, overrun.overrunLateness);
    EXPECT_EQ(1U, overrun.totalOverruns);

    const AuthoritativeMatchResult idle = match.Advance(
        TimeNs(1000000000ULL));
    EXPECT_EQ(0U, idle.ticksExecuted);
    EXPECT_FALSE(idle.overrun);
    EXPECT_EQ(1U, idle.totalOverruns);
}

TEST(AuthoritativeMatch, TicketExpiryResolvesRemainingSlotAndStartsMatch)
{
    DeterministicUdpTokenSource tokens;
    AuthoritativeMatch match = CreateMatch(
        tokens,
        TimeNs(0U),
        Reservation(2U, 100U));
    static_cast<void>(Authenticate(
        match,
        GameConnectionId{10U},
        PlayerA,
        Ticket(2U),
        TimeNs(50000000ULL)));
    EXPECT_FALSE(match.Started());

    const AuthoritativeMatchResult expiry = match.Advance(
        TimeNs(100000000ULL));
    EXPECT_TRUE(expiry.control.empty());
    EXPECT_TRUE(match.Started());
    ASSERT_TRUE(match.NextDeadline().has_value());
    EXPECT_EQ(TimeNs(133333333ULL), *match.NextDeadline());

    const AuthoritativeMatchResult finished = match.Advance(
        TimeNs(133333333ULL));
    EXPECT_EQ(1U, finished.ticksExecuted);
    EXPECT_EQ(
        MatchCompletionReason::LastSurvivor,
        OnlyControlFinished(finished).reason);
    ASSERT_EQ(1U, finished.tcp.size());
    EXPECT_TRUE(std::holds_alternative<GameMatchResult>(
        finished.tcp.front().message));
    EXPECT_TRUE(finished.tcp.front().closeAfterWrite);
}

TEST(AuthoritativeMatch, AllExpiredTicketsFinishWithoutSimulationTick)
{
    DeterministicUdpTokenSource tokens;
    AuthoritativeMatch match = CreateMatch(
        tokens,
        TimeNs(0U),
        Reservation(2U, 100U));

    const AuthoritativeMatchResult result = match.Advance(
        TimeNs(100000000ULL));

    EXPECT_EQ(0U, result.ticksExecuted);
    EXPECT_EQ(
        MatchCompletionReason::NoAuthenticatedPlayers,
        OnlyControlFinished(result).reason);
    EXPECT_FALSE(match.Started());
    EXPECT_FALSE(match.NextDeadline().has_value());
}

TEST(AuthoritativeMatch, OneDisconnectEliminatesOnNextTickAndPublishesResults)
{
    DeterministicUdpTokenSource tokens;
    AuthoritativeMatch match = CreateMatch(tokens);
    static_cast<void>(StartPlayers(match));

    const AuthoritativeMatchResult disconnected = match.Disconnect(
        GameConnectionId{11U});
    EXPECT_TRUE(disconnected.control.empty());
    const AuthoritativeMatchResult finished = match.Advance(
        TimeNs(33333333ULL));

    const auto& control = OnlyControlFinished(finished);
    EXPECT_EQ(MatchCompletionReason::LastSurvivor, control.reason);
    EXPECT_TRUE(control.hasWinner);
    EXPECT_EQ(EntityId{0U}, control.winner);
    ASSERT_EQ(1U, finished.tcp.size());
    EXPECT_EQ(GameConnectionId{10U}, finished.tcp.front().recipient);
    const auto* reliable = std::get_if<GameMatchResult>(
        &finished.tcp.front().message);
    ASSERT_NE(nullptr, reliable);
    EXPECT_EQ(EntityId{0U}, reliable->winner);
    EXPECT_TRUE(finished.tcp.front().closeAfterWrite);
}

TEST(AuthoritativeMatch, AllConnectedSessionsLostFinishWithoutZeroSurvivorStep)
{
    DeterministicUdpTokenSource tokens;
    AuthoritativeMatch match = CreateMatch(tokens);
    static_cast<void>(StartPlayers(match));

    EXPECT_TRUE(match.Disconnect(GameConnectionId{10U}).control.empty());
    const AuthoritativeMatchResult result = match.Disconnect(
        GameConnectionId{11U});

    const auto& finished = OnlyControlFinished(result);
    EXPECT_EQ(MatchCompletionReason::NoConnectedPlayers, finished.reason);
    EXPECT_FALSE(finished.hasWinner);
    EXPECT_EQ(0U, finished.finishedTick);
    EXPECT_TRUE(result.tcp.empty());
    EXPECT_FALSE(match.Started());
    EXPECT_FALSE(match.NextDeadline().has_value());
}

TEST(SnapshotAdapter, RejectsUnknownSimulationEnum)
{
    dxa::simulation::MatchSnapshot snapshot;
    snapshot.phase = static_cast<dxa::simulation::MatchPhase>(99);
    EXPECT_THROW(
        (void)dxa::game_common::ToGameSnapshot(snapshot),
        std::invalid_argument);
}
