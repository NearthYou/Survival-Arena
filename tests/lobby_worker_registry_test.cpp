#include <dxa/lobby/WorkerRegistry.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using namespace std::chrono_literals;
using dxa::lobby::CancelReservationAction;
using dxa::lobby::LobbyRuntimeAction;
using dxa::lobby::MatchFinishedEvent;
using dxa::lobby::MatchUnavailableEvent;
using dxa::lobby::ReservationFailedEvent;
using dxa::lobby::ReservationReadyEvent;
using dxa::lobby::ReservationTimerKind;
using dxa::lobby::ReserveMatchAction;
using dxa::lobby::WorkerConnectionId;
using dxa::lobby::WorkerEvent;
using dxa::lobby::WorkerRegistry;
using dxa::lobby::WorkerRegistryResult;
using dxa::protocol::CancelMatchReservation;
using dxa::protocol::GameEndpoint;
using dxa::protocol::LobbyToWorkerMessage;
using dxa::protocol::MatchCompletionReason;
using dxa::protocol::MatchFinished;
using dxa::protocol::MatchId;
using dxa::protocol::MatchReservationCancelled;
using dxa::protocol::MatchTicketValue;
using dxa::protocol::PlayerId;
using dxa::protocol::ReservationId;
using dxa::protocol::ReserveMatch;
using dxa::protocol::ReserveMatchReady;
using dxa::protocol::ReserveMatchRejected;
using dxa::protocol::WorkerId;
using dxa::protocol::WorkerRegister;
using dxa::protocol::WorkerRegistered;
using dxa::protocol::WorkerReservationReject;
using dxa::protocol::WorkerToLobbyMessage;

[[nodiscard]] std::chrono::steady_clock::time_point Time(
    const std::int64_t milliseconds)
{
    return std::chrono::steady_clock::time_point{
        std::chrono::milliseconds{milliseconds}};
}

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

[[nodiscard]] ReserveMatchAction ReserveAction(
    const ReservationId reservation = ReservationId{1U},
    const MatchId match = MatchId{11U},
    const std::chrono::steady_clock::time_point issuedAt = Time(0))
{
    return ReserveMatchAction{
        reservation,
        dxa::protocol::RoomId{3U},
        match,
        PlayerId{5U},
        17U,
        20260824U,
        issuedAt,
        {
            {PlayerId{5U}, Ticket(1U)},
            {PlayerId{8U}, Ticket(2U)},
        }};
}

[[nodiscard]] WorkerRegister Registration(
    const WorkerId worker = WorkerId{3U},
    std::string host = "127.0.0.1",
    const std::uint16_t tcpPort = 7100U,
    const std::uint16_t udpPort = 7101U,
    const std::uint8_t capacity = 1U)
{
    return WorkerRegister{
        worker,
        std::move(host),
        tcpPort,
        udpPort,
        capacity};
}

[[nodiscard]] WorkerRegistry RegistryWithOneIdleWorker(
    const WorkerConnectionId connection = WorkerConnectionId{10U})
{
    WorkerRegistry registry;
    const WorkerRegistryResult registration = registry.Register(
        connection,
        Registration());
    if (!registration.accepted)
    {
        throw std::logic_error{"worker registration failed"};
    }
    return registry;
}

template <typename Value>
[[nodiscard]] const Value& OnlyOutbound(const WorkerRegistryResult& result)
{
    if (result.outbound.size() != 1U)
    {
        throw std::logic_error{"expected one worker outbound"};
    }
    const auto* value = std::get_if<Value>(&result.outbound.front().message);
    if (value == nullptr)
    {
        throw std::logic_error{"unexpected worker outbound type"};
    }
    return *value;
}

template <typename Value>
[[nodiscard]] const Value& OnlyEvent(const WorkerRegistryResult& result)
{
    if (result.events.size() != 1U)
    {
        throw std::logic_error{"expected one worker event"};
    }
    const auto* value = std::get_if<Value>(&result.events.front());
    if (value == nullptr)
    {
        throw std::logic_error{"unexpected worker event type"};
    }
    return *value;
}

[[nodiscard]] WorkerRegistryResult Assign(
    WorkerRegistry& registry,
    const ReserveMatchAction& action = ReserveAction(),
    const std::chrono::steady_clock::time_point now = Time(0))
{
    return registry.Execute(LobbyRuntimeAction{action}, now);
}

void Ready(
    WorkerRegistry& registry,
    const WorkerConnectionId connection = WorkerConnectionId{10U},
    const ReservationId reservation = ReservationId{1U},
    const MatchId match = MatchId{11U})
{
    const WorkerRegistryResult result = registry.Receive(
        connection,
        WorkerToLobbyMessage{ReserveMatchReady{reservation, match}});
    if (result.events.size() != 1U
        || !std::holds_alternative<ReservationReadyEvent>(result.events.front()))
    {
        throw std::logic_error{"worker ready transition failed"};
    }
}
} // namespace

TEST(WorkerRegistry, AssignsLowestIdleWorkerAndWaitsForReady)
{
    WorkerRegistry registry;
    EXPECT_TRUE(registry.Register(
        WorkerConnectionId{20U},
        Registration(WorkerId{8U}, "worker8", 7200U, 7201U)).accepted);
    EXPECT_TRUE(registry.Register(
        WorkerConnectionId{10U},
        Registration(WorkerId{3U}, "worker3", 7100U, 7101U)).accepted);

    const WorkerRegistryResult assigned = Assign(registry);

    ASSERT_EQ(1U, assigned.outbound.size());
    EXPECT_EQ(WorkerConnectionId{10U}, assigned.outbound.front().recipient);
    const ReserveMatch& reservation = OnlyOutbound<ReserveMatch>(assigned);
    EXPECT_EQ(ReservationId{1U}, reservation.reservation);
    EXPECT_EQ(MatchId{11U}, reservation.match);
    EXPECT_EQ(60000U, reservation.ticketLifetimeMilliseconds);
    ASSERT_EQ(1U, assigned.timers.size());
    EXPECT_EQ(ReservationTimerKind::Start, assigned.timers.front().kind);
    EXPECT_EQ(ReservationId{1U}, assigned.timers.front().reservation);
    EXPECT_EQ(2s, assigned.timers.front().duration);
    EXPECT_TRUE(assigned.events.empty());
    EXPECT_EQ(2U, registry.WorkerCount());
    EXPECT_EQ(1U, registry.IdleCount());
}

TEST(WorkerRegistry, RegistrationRepliesAndAcceptsVisibleAsciiBoundary)
{
    WorkerRegistry registry;
    const WorkerRegistryResult result = registry.Register(
        WorkerConnectionId{9U},
        Registration(
            WorkerId{7U},
            std::string(255U, '~'),
            1U,
            65535U));

    EXPECT_TRUE(result.accepted);
    EXPECT_EQ(WorkerRegistered{WorkerId{7U}},
              OnlyOutbound<WorkerRegistered>(result));
    EXPECT_TRUE(result.closeConnections.empty());
    EXPECT_EQ(1U, registry.WorkerCount());
    EXPECT_EQ(1U, registry.IdleCount());
}

TEST(WorkerRegistry, RejectsInvalidRegistrationBoundaries)
{
    const std::vector<WorkerRegister> invalid{
        Registration(WorkerId{}),
        Registration(WorkerId{1U}, ""),
        Registration(WorkerId{1U}, std::string(256U, 'a')),
        Registration(WorkerId{1U}, "bad\nworker"),
        Registration(WorkerId{1U}, "worker", 0U, 7101U),
        Registration(WorkerId{1U}, "worker", 7100U, 0U),
        Registration(WorkerId{1U}, "worker", 7100U, 7101U, 0U),
        Registration(WorkerId{1U}, "worker", 7100U, 7101U, 2U),
    };

    for (const WorkerRegister& registration : invalid)
    {
        WorkerRegistry registry;
        const WorkerRegistryResult result = registry.Register(
            WorkerConnectionId{1U},
            registration);
        EXPECT_FALSE(result.accepted);
        EXPECT_EQ(
            std::vector<WorkerConnectionId>{WorkerConnectionId{1U}},
            result.closeConnections);
        EXPECT_EQ(0U, registry.WorkerCount());
    }

    WorkerRegistry registry;
    const WorkerRegistryResult zeroConnection = registry.Register(
        WorkerConnectionId{},
        Registration());
    EXPECT_FALSE(zeroConnection.accepted);
    EXPECT_EQ(0U, registry.WorkerCount());
}

TEST(WorkerRegistry, RejectsDuplicateWorkerAndEndpointWithoutEvictingOriginal)
{
    WorkerRegistry registry;
    ASSERT_TRUE(registry.Register(
        WorkerConnectionId{10U},
        Registration()).accepted);

    const WorkerRegistryResult duplicateWorker = registry.Register(
        WorkerConnectionId{11U},
        Registration(WorkerId{3U}, "worker3-alt", 7200U, 7201U));
    EXPECT_FALSE(duplicateWorker.accepted);
    EXPECT_EQ(
        std::vector<WorkerConnectionId>{WorkerConnectionId{11U}},
        duplicateWorker.closeConnections);

    const WorkerRegistryResult duplicateEndpoint = registry.Register(
        WorkerConnectionId{12U},
        Registration(WorkerId{4U}));
    EXPECT_FALSE(duplicateEndpoint.accepted);
    EXPECT_EQ(
        std::vector<WorkerConnectionId>{WorkerConnectionId{12U}},
        duplicateEndpoint.closeConnections);
    EXPECT_EQ(1U, registry.WorkerCount());
    EXPECT_EQ(1U, registry.IdleCount());
}

TEST(WorkerRegistry, NoIdleWorkerFailsReservationImmediately)
{
    WorkerRegistry registry;

    const WorkerRegistryResult result = Assign(registry);

    EXPECT_FALSE(result.accepted);
    EXPECT_TRUE(result.outbound.empty());
    EXPECT_TRUE(result.timers.empty());
    EXPECT_EQ(
        (ReservationFailedEvent{ReservationId{1U}, MatchId{11U}}),
        OnlyEvent<ReservationFailedEvent>(result));
}

TEST(WorkerRegistry, ExpiredReservationFailsWithoutConsumingIdleWorker)
{
    WorkerRegistry registry = RegistryWithOneIdleWorker();

    const WorkerRegistryResult result = Assign(
        registry,
        ReserveAction(ReservationId{1U}, MatchId{11U}, Time(0)),
        Time(60000));

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(
        (ReservationFailedEvent{ReservationId{1U}, MatchId{11U}}),
        OnlyEvent<ReservationFailedEvent>(result));
    EXPECT_EQ(1U, registry.IdleCount());
}

TEST(WorkerRegistry, FloorsRemainingTicketLifetimeToWholeMilliseconds)
{
    WorkerRegistry registry = RegistryWithOneIdleWorker();

    const WorkerRegistryResult result = Assign(
        registry,
        ReserveAction(ReservationId{1U}, MatchId{11U}, Time(500)),
        Time(1734));

    EXPECT_EQ(
        58766U,
        OnlyOutbound<ReserveMatch>(result).ticketLifetimeMilliseconds);
}

TEST(WorkerRegistry, RejectionReturnsWorkerToIdle)
{
    WorkerRegistry registry = RegistryWithOneIdleWorker();
    ASSERT_EQ(1U, Assign(registry).outbound.size());

    const WorkerRegistryResult rejected = registry.Receive(
        WorkerConnectionId{10U},
        WorkerToLobbyMessage{ReserveMatchRejected{
            ReservationId{1U},
            MatchId{11U},
            WorkerReservationReject::SimulationInitializationFailed}});

    EXPECT_TRUE(rejected.accepted);
    EXPECT_EQ(
        (ReservationFailedEvent{ReservationId{1U}, MatchId{11U}}),
        OnlyEvent<ReservationFailedEvent>(rejected));
    EXPECT_EQ(1U, registry.IdleCount());
    EXPECT_EQ(
        WorkerConnectionId{10U},
        Assign(
            registry,
            ReserveAction(ReservationId{2U}, MatchId{12U})).outbound.front().recipient);
}

TEST(WorkerRegistry, ReadyTransitionsWorkerToActiveAndPublishesEndpoint)
{
    WorkerRegistry registry = RegistryWithOneIdleWorker();
    ASSERT_EQ(1U, Assign(registry).outbound.size());

    const WorkerRegistryResult ready = registry.Receive(
        WorkerConnectionId{10U},
        WorkerToLobbyMessage{ReserveMatchReady{
            ReservationId{1U}, MatchId{11U}}});

    EXPECT_TRUE(ready.accepted);
    EXPECT_EQ(
        (ReservationReadyEvent{
            ReservationId{1U},
            MatchId{11U},
            WorkerId{3U},
            GameEndpoint{"127.0.0.1", 7100U, 7101U}}),
        OnlyEvent<ReservationReadyEvent>(ready));
    EXPECT_EQ(0U, registry.IdleCount());
    EXPECT_TRUE(Assign(
        registry,
        ReserveAction(ReservationId{2U}, MatchId{12U})).outbound.empty());
}

TEST(WorkerRegistry, CancelWaitsForAcknowledgementBeforeReturningIdle)
{
    WorkerRegistry registry = RegistryWithOneIdleWorker();
    ASSERT_EQ(1U, Assign(registry).outbound.size());

    const WorkerRegistryResult cancelling = registry.Execute(
        LobbyRuntimeAction{CancelReservationAction{
            ReservationId{1U}, MatchId{11U}}},
        Time(1));

    EXPECT_TRUE(cancelling.accepted);
    EXPECT_EQ(
        (CancelMatchReservation{ReservationId{1U}, MatchId{11U}}),
        OnlyOutbound<CancelMatchReservation>(cancelling));
    ASSERT_EQ(1U, cancelling.timers.size());
    EXPECT_EQ(ReservationTimerKind::Cancel, cancelling.timers.front().kind);
    EXPECT_EQ(2s, cancelling.timers.front().duration);
    EXPECT_EQ(0U, registry.IdleCount());

    const WorkerRegistryResult cancelled = registry.Receive(
        WorkerConnectionId{10U},
        WorkerToLobbyMessage{MatchReservationCancelled{
            ReservationId{1U}, MatchId{11U}}});
    EXPECT_TRUE(cancelled.accepted);
    EXPECT_TRUE(cancelled.events.empty());
    EXPECT_EQ(1U, registry.IdleCount());
}

TEST(WorkerRegistry, TimeoutFailsReservationAndClosesWorker)
{
    WorkerRegistry registry = RegistryWithOneIdleWorker();
    ASSERT_EQ(1U, Assign(registry).timers.size());

    const WorkerRegistryResult timeout = registry.Timeout(ReservationId{1U});

    EXPECT_EQ(
        (ReservationFailedEvent{ReservationId{1U}, MatchId{11U}}),
        OnlyEvent<ReservationFailedEvent>(timeout));
    EXPECT_EQ(
        std::vector<WorkerConnectionId>{WorkerConnectionId{10U}},
        timeout.closeConnections);
    EXPECT_EQ(0U, registry.WorkerCount());
    EXPECT_EQ(0U, registry.IdleCount());
}

TEST(WorkerRegistry, CancelTimeoutClosesWorkerWithoutRepeatingLobbyFailure)
{
    WorkerRegistry registry = RegistryWithOneIdleWorker();
    ASSERT_EQ(1U, Assign(registry).outbound.size());
    ASSERT_EQ(
        1U,
        registry.Execute(
            LobbyRuntimeAction{CancelReservationAction{
                ReservationId{1U}, MatchId{11U}}},
            Time(1)).outbound.size());

    const WorkerRegistryResult timeout = registry.Timeout(ReservationId{1U});

    EXPECT_TRUE(timeout.events.empty());
    EXPECT_EQ(
        std::vector<WorkerConnectionId>{WorkerConnectionId{10U}},
        timeout.closeConnections);
    EXPECT_EQ(0U, registry.WorkerCount());
}

TEST(WorkerRegistry, MismatchedReservationClosesWorkerAndFailsActualPendingStart)
{
    WorkerRegistry registry = RegistryWithOneIdleWorker();
    ASSERT_EQ(1U, Assign(registry).outbound.size());

    const WorkerRegistryResult mismatch = registry.Receive(
        WorkerConnectionId{10U},
        WorkerToLobbyMessage{ReserveMatchReady{
            ReservationId{2U}, MatchId{11U}}});

    EXPECT_EQ(
        std::vector<WorkerConnectionId>{WorkerConnectionId{10U}},
        mismatch.closeConnections);
    EXPECT_EQ(
        (ReservationFailedEvent{ReservationId{1U}, MatchId{11U}}),
        OnlyEvent<ReservationFailedEvent>(mismatch));
    EXPECT_EQ(0U, registry.WorkerCount());
}

TEST(WorkerRegistry, ReservedDisconnectFailsPendingStart)
{
    WorkerRegistry registry = RegistryWithOneIdleWorker();
    ASSERT_EQ(1U, Assign(registry).outbound.size());

    const WorkerRegistryResult disconnected = registry.Disconnect(
        WorkerConnectionId{10U});

    EXPECT_EQ(
        (ReservationFailedEvent{ReservationId{1U}, MatchId{11U}}),
        OnlyEvent<ReservationFailedEvent>(disconnected));
    EXPECT_EQ(0U, registry.WorkerCount());
}

TEST(WorkerRegistry, ActiveDisconnectMakesMatchUnavailable)
{
    WorkerRegistry registry = RegistryWithOneIdleWorker();
    ASSERT_EQ(1U, Assign(registry).outbound.size());
    Ready(registry);

    const WorkerRegistryResult disconnected = registry.Disconnect(
        WorkerConnectionId{10U});

    EXPECT_EQ(
        (MatchUnavailableEvent{WorkerId{3U}, MatchId{11U}}),
        OnlyEvent<MatchUnavailableEvent>(disconnected));
    EXPECT_EQ(0U, registry.WorkerCount());
}

TEST(WorkerRegistry, MatchFinishedReturnsWorkerToIdleAndLateFrameIsIgnored)
{
    WorkerRegistry registry = RegistryWithOneIdleWorker();
    ASSERT_EQ(1U, Assign(registry).outbound.size());
    Ready(registry);
    const WorkerToLobbyMessage completion{MatchFinished{
        MatchId{11U},
        dxa::protocol::EntityId{4U},
        true,
        MatchCompletionReason::LastSurvivor,
        99U}};

    const WorkerRegistryResult finished = registry.Receive(
        WorkerConnectionId{10U},
        completion);

    EXPECT_EQ(
        (MatchFinishedEvent{WorkerId{3U}, MatchId{11U}}),
        OnlyEvent<MatchFinishedEvent>(finished));
    EXPECT_EQ(1U, registry.IdleCount());

    const WorkerRegistryResult late = registry.Receive(
        WorkerConnectionId{10U},
        completion);
    EXPECT_TRUE(late.events.empty());
    EXPECT_TRUE(late.closeConnections.empty());
    EXPECT_EQ(1U, registry.IdleCount());
}

TEST(WorkerRegistry, ReadyArrivingAfterCancelDoesNotReactivateWorker)
{
    WorkerRegistry registry = RegistryWithOneIdleWorker();
    ASSERT_EQ(1U, Assign(registry).outbound.size());
    ASSERT_EQ(
        1U,
        registry.Execute(
            LobbyRuntimeAction{CancelReservationAction{
                ReservationId{1U}, MatchId{11U}}},
            Time(1)).outbound.size());

    const WorkerRegistryResult lateReady = registry.Receive(
        WorkerConnectionId{10U},
        WorkerToLobbyMessage{ReserveMatchReady{
            ReservationId{1U}, MatchId{11U}}});

    EXPECT_TRUE(lateReady.events.empty());
    EXPECT_TRUE(lateReady.outbound.empty());
    EXPECT_EQ(0U, registry.IdleCount());
}
