#include <dxa/lobby/WorkerRegistry.hpp>

#include <dxa/protocol/LobbyTypes.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dxa::lobby
{
namespace
{
using namespace std::chrono_literals;
using dxa::protocol::CancelMatchReservation;
using dxa::protocol::LobbyToWorkerMessage;
using dxa::protocol::MatchFinished;
using dxa::protocol::MatchId;
using dxa::protocol::MatchReservationCancelled;
using dxa::protocol::ReservationId;
using dxa::protocol::ReserveMatch;
using dxa::protocol::ReserveMatchReady;
using dxa::protocol::ReserveMatchRejected;
using dxa::protocol::WorkerId;
using dxa::protocol::WorkerRegister;
using dxa::protocol::WorkerRegistered;

template <typename... Functions>
struct Overloaded : Functions...
{
    using Functions::operator()...;
};

template <typename... Functions>
Overloaded(Functions...) -> Overloaded<Functions...>;

[[nodiscard]] bool IsVisibleAsciiHost(const std::string_view host) noexcept
{
    if (host.empty() || host.size() > 255U)
    {
        return false;
    }
    return std::all_of(host.begin(), host.end(), [](const char character) {
        const auto value = static_cast<unsigned char>(character);
        return value >= 0x21U && value <= 0x7EU;
    });
}

[[nodiscard]] bool IsValidRegistration(
    const WorkerConnectionId connection,
    const WorkerRegister& registration) noexcept
{
    return connection.value != 0U
        && registration.worker.value != 0U
        && IsVisibleAsciiHost(registration.advertisedHost)
        && registration.gameTcpPort != 0U
        && registration.gameUdpPort != 0U
        && registration.capacity == 1U;
}

[[nodiscard]] ReservationFailedEvent Failed(
    const ReservationId reservation,
    const MatchId match) noexcept
{
    return {reservation, match};
}
} // namespace

WorkerRegistryResult WorkerRegistry::Register(
    const WorkerConnectionId connection,
    const WorkerRegister& registration)
{
    WorkerRegistryResult result;
    if (!IsValidRegistration(connection, registration))
    {
        if (connection.value != 0U)
        {
            result.closeConnections.push_back(connection);
        }
        return result;
    }

    if (connections_.contains(connection))
    {
        return CloseForProtocolViolation(connection);
    }

    const EndpointKey endpoint{
        registration.advertisedHost,
        registration.gameTcpPort,
        registration.gameUdpPort};
    if (workers_.contains(registration.worker) || endpoints_.contains(endpoint))
    {
        result.closeConnections.push_back(connection);
        return result;
    }

    const auto [worker, workerInserted] = workers_.emplace(
        registration.worker,
        WorkerRecord{connection, registration});
    if (!workerInserted)
    {
        result.closeConnections.push_back(connection);
        return result;
    }
    const auto [connectionEntry, connectionInserted] = connections_.emplace(
        connection,
        registration.worker);
    static_cast<void>(connectionEntry);
    const auto [endpointEntry, endpointInserted] = endpoints_.emplace(
        endpoint,
        registration.worker);
    static_cast<void>(endpointEntry);
    if (!connectionInserted || !endpointInserted)
    {
        connections_.erase(connection);
        endpoints_.erase(endpoint);
        workers_.erase(worker);
        result.closeConnections.push_back(connection);
        return result;
    }

    result.accepted = true;
    result.outbound.push_back({
        connection,
        LobbyToWorkerMessage{WorkerRegistered{registration.worker}}});
    return result;
}

WorkerRegistryResult WorkerRegistry::Execute(
    const LobbyRuntimeAction& action,
    const std::chrono::steady_clock::time_point now)
{
    return std::visit(
        Overloaded{
            [&](const ReserveMatchAction& reservation) {
                WorkerRegistryResult result;
                const auto fail = [&] {
                    result.events.push_back(Failed(
                        reservation.reservation,
                        reservation.match));
                    return result;
                };

                if (reservation.reservation.value == 0U
                    || reservations_.contains(reservation.reservation)
                    || reservation.participants.size() < 2U
                    || reservation.participants.size()
                        > dxa::protocol::RoomCapacity)
                {
                    return fail();
                }

                const auto worker = std::find_if(
                    workers_.begin(),
                    workers_.end(),
                    [](const auto& entry) {
                        return entry.second.state == WorkerState::Idle;
                    });
                if (worker == workers_.end())
                {
                    return fail();
                }

                constexpr auto ticketLifetime = std::chrono::seconds{
                    dxa::protocol::MatchTicketLifetimeSeconds};
                const auto remaining = reservation.issuedAt
                    + ticketLifetime
                    - now;
                if (remaining <= std::chrono::steady_clock::duration::zero())
                {
                    return fail();
                }
                const auto maximumRemaining =
                    std::chrono::duration_cast<
                        std::chrono::steady_clock::duration>(ticketLifetime);
                const auto remainingMilliseconds =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::min(remaining, maximumRemaining));
                if (remainingMilliseconds <= 0ms)
                {
                    return fail();
                }

                WorkerRecord& record = worker->second;
                record.state = WorkerState::Reserved;
                record.reservation = reservation.reservation;
                record.match = reservation.match;
                const auto [tracked, inserted] = reservations_.emplace(
                    reservation.reservation,
                    worker->first);
                static_cast<void>(tracked);
                if (!inserted)
                {
                    ClearReservation(record);
                    return fail();
                }

                result.accepted = true;
                result.outbound.push_back({
                    record.connection,
                    LobbyToWorkerMessage{ReserveMatch{
                        reservation.reservation,
                        reservation.match,
                        reservation.seed,
                        static_cast<std::uint32_t>(
                            remainingMilliseconds.count()),
                        reservation.participants}}});
                result.timers.push_back({
                    ReservationTimerKind::Start,
                    reservation.reservation,
                    2s});
                return result;
            },
            [&](const CancelReservationAction& cancellation) {
                WorkerRegistryResult result;
                const auto reservation = reservations_.find(
                    cancellation.reservation);
                if (reservation == reservations_.end())
                {
                    return result;
                }
                auto worker = workers_.find(reservation->second);
                if (worker == workers_.end())
                {
                    reservations_.erase(reservation);
                    return result;
                }

                WorkerRecord& record = worker->second;
                if (record.state != WorkerState::Reserved
                    || record.reservation != cancellation.reservation
                    || record.match != cancellation.match)
                {
                    return result;
                }

                record.state = WorkerState::Cancelling;
                result.accepted = true;
                result.outbound.push_back({
                    record.connection,
                    LobbyToWorkerMessage{CancelMatchReservation{
                        cancellation.reservation,
                        cancellation.match}}});
                result.timers.push_back({
                    ReservationTimerKind::Cancel,
                    cancellation.reservation,
                    2s});
                return result;
            }},
        action);
}

WorkerRegistryResult WorkerRegistry::Receive(
    const WorkerConnectionId connection,
    const dxa::protocol::WorkerToLobbyMessage& message)
{
    const auto connectionEntry = connections_.find(connection);
    if (connectionEntry == connections_.end())
    {
        WorkerRegistryResult result;
        if (connection.value != 0U)
        {
            result.closeConnections.push_back(connection);
        }
        return result;
    }
    auto worker = workers_.find(connectionEntry->second);
    if (worker == workers_.end())
    {
        connections_.erase(connectionEntry);
        WorkerRegistryResult result;
        result.closeConnections.push_back(connection);
        return result;
    }

    WorkerRecord& record = worker->second;
    return std::visit(
        Overloaded{
            [&](const WorkerRegister&) {
                return CloseForProtocolViolation(connection);
            },
            [&](const ReserveMatchReady& ready) {
                if (record.state == WorkerState::Reserved)
                {
                    if (record.reservation != ready.reservation
                        || record.match != ready.match)
                    {
                        return CloseForProtocolViolation(connection);
                    }
                    reservations_.erase(ready.reservation);
                    record.state = WorkerState::Active;

                    WorkerRegistryResult result;
                    result.accepted = true;
                    result.events.push_back(ReservationReadyEvent{
                        ready.reservation,
                        ready.match,
                        record.registration.worker,
                        dxa::protocol::GameEndpoint{
                            record.registration.advertisedHost,
                            record.registration.gameTcpPort,
                            record.registration.gameUdpPort}});
                    return result;
                }
                if ((record.state == WorkerState::Cancelling
                     || record.state == WorkerState::Active)
                    && record.reservation == ready.reservation
                    && record.match == ready.match)
                {
                    WorkerRegistryResult result;
                    result.accepted = true;
                    return result;
                }
                if (record.state == WorkerState::Idle)
                {
                    WorkerRegistryResult result;
                    result.accepted = true;
                    return result;
                }
                return CloseForProtocolViolation(connection);
            },
            [&](const ReserveMatchRejected& rejected) {
                if (record.state == WorkerState::Reserved)
                {
                    if (record.reservation != rejected.reservation
                        || record.match != rejected.match)
                    {
                        return CloseForProtocolViolation(connection);
                    }
                    const ReservationFailedEvent event = Failed(
                        rejected.reservation,
                        rejected.match);
                    ClearReservation(record);

                    WorkerRegistryResult result;
                    result.accepted = true;
                    result.events.push_back(event);
                    return result;
                }
                if ((record.state == WorkerState::Cancelling
                     || record.state == WorkerState::Active)
                    && record.reservation == rejected.reservation
                    && record.match == rejected.match)
                {
                    WorkerRegistryResult result;
                    result.accepted = true;
                    return result;
                }
                if (record.state == WorkerState::Idle)
                {
                    WorkerRegistryResult result;
                    result.accepted = true;
                    return result;
                }
                return CloseForProtocolViolation(connection);
            },
            [&](const MatchReservationCancelled& cancelled) {
                if (record.state == WorkerState::Cancelling)
                {
                    if (record.reservation != cancelled.reservation
                        || record.match != cancelled.match)
                    {
                        return CloseForProtocolViolation(connection);
                    }
                    ClearReservation(record);
                    WorkerRegistryResult result;
                    result.accepted = true;
                    return result;
                }
                if (record.state == WorkerState::Idle
                    || record.state == WorkerState::Active)
                {
                    WorkerRegistryResult result;
                    result.accepted = true;
                    return result;
                }
                return CloseForProtocolViolation(connection);
            },
            [&](const MatchFinished& finished) {
                if (record.state == WorkerState::Active)
                {
                    if (record.match != finished.match)
                    {
                        return CloseForProtocolViolation(connection);
                    }
                    const MatchFinishedEvent event{
                        record.registration.worker,
                        finished.match};
                    ClearReservation(record);

                    WorkerRegistryResult result;
                    result.accepted = true;
                    result.events.push_back(event);
                    return result;
                }
                if (record.state == WorkerState::Idle)
                {
                    WorkerRegistryResult result;
                    result.accepted = true;
                    return result;
                }
                return CloseForProtocolViolation(connection);
            }},
        message);
}

WorkerRegistryResult WorkerRegistry::Disconnect(
    const WorkerConnectionId connection)
{
    const auto connectionEntry = connections_.find(connection);
    if (connectionEntry == connections_.end())
    {
        return {};
    }
    const auto worker = workers_.find(connectionEntry->second);
    if (worker == workers_.end())
    {
        connections_.erase(connectionEntry);
        return {};
    }

    const WorkerRecord record = worker->second;
    WorkerRegistryResult result;
    result.accepted = true;
    if (record.state == WorkerState::Reserved
        && record.reservation.has_value()
        && record.match.has_value())
    {
        result.events.push_back(Failed(
            *record.reservation,
            *record.match));
    }
    else if (record.state == WorkerState::Active
             && record.match.has_value())
    {
        result.events.push_back(MatchUnavailableEvent{
            record.registration.worker,
            *record.match});
    }

    if (record.reservation.has_value())
    {
        reservations_.erase(*record.reservation);
    }
    endpoints_.erase(EndpointKey{
        record.registration.advertisedHost,
        record.registration.gameTcpPort,
        record.registration.gameUdpPort});
    workers_.erase(worker);
    connections_.erase(connectionEntry);
    return result;
}

WorkerRegistryResult WorkerRegistry::Timeout(const ReservationId reservation)
{
    const auto tracked = reservations_.find(reservation);
    if (tracked == reservations_.end())
    {
        return {};
    }
    const auto worker = workers_.find(tracked->second);
    if (worker == workers_.end())
    {
        reservations_.erase(tracked);
        return {};
    }

    const WorkerConnectionId connection = worker->second.connection;
    WorkerRegistryResult result = Disconnect(connection);
    result.closeConnections.push_back(connection);
    return result;
}

std::size_t WorkerRegistry::WorkerCount() const noexcept
{
    return workers_.size();
}

std::size_t WorkerRegistry::IdleCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        workers_.begin(),
        workers_.end(),
        [](const auto& worker) {
            return worker.second.state == WorkerState::Idle;
        }));
}

WorkerRegistryResult WorkerRegistry::CloseForProtocolViolation(
    const WorkerConnectionId connection)
{
    WorkerRegistryResult result = Disconnect(connection);
    result.accepted = false;
    result.closeConnections.push_back(connection);
    return result;
}

void WorkerRegistry::ClearReservation(WorkerRecord& worker)
{
    if (worker.reservation.has_value())
    {
        reservations_.erase(*worker.reservation);
    }
    worker.state = WorkerState::Idle;
    worker.reservation.reset();
    worker.match.reset();
}
} // namespace dxa::lobby
