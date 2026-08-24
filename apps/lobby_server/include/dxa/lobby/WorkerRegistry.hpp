#pragma once

#include <dxa/lobby/LobbyRuntimeTypes.hpp>
#include <dxa/protocol/WorkerControlMessages.hpp>

#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace dxa::lobby
{
struct WorkerConnectionId
{
    std::uint64_t value = 0U;

    [[nodiscard]] auto operator<=>(const WorkerConnectionId&) const = default;
};

enum class WorkerState : std::uint8_t
{
    Idle,
    Reserved,
    Cancelling,
    Active
};

struct WorkerControlOutbound
{
    WorkerConnectionId recipient;
    dxa::protocol::LobbyToWorkerMessage message;

    [[nodiscard]] bool operator==(const WorkerControlOutbound&) const = default;
};

enum class ReservationTimerKind : std::uint8_t
{
    Start,
    Cancel
};

struct ReservationTimerDirective
{
    ReservationTimerKind kind = ReservationTimerKind::Start;
    dxa::protocol::ReservationId reservation;
    std::chrono::milliseconds duration{2000};

    [[nodiscard]] bool operator==(
        const ReservationTimerDirective&) const = default;
};

struct WorkerRegistryResult
{
    bool accepted = false;
    std::vector<WorkerControlOutbound> outbound;
    std::vector<WorkerEvent> events;
    std::vector<ReservationTimerDirective> timers;
    std::vector<WorkerConnectionId> closeConnections;
};

class WorkerRegistry
{
public:
    [[nodiscard]] WorkerRegistryResult Register(
        WorkerConnectionId connection,
        const dxa::protocol::WorkerRegister& registration);
    [[nodiscard]] WorkerRegistryResult Execute(
        const LobbyRuntimeAction& action,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] WorkerRegistryResult Receive(
        WorkerConnectionId connection,
        const dxa::protocol::WorkerToLobbyMessage& message);
    [[nodiscard]] WorkerRegistryResult Disconnect(
        WorkerConnectionId connection);
    [[nodiscard]] WorkerRegistryResult Timeout(
        dxa::protocol::ReservationId reservation);

    [[nodiscard]] std::size_t WorkerCount() const noexcept;
    [[nodiscard]] std::size_t IdleCount() const noexcept;

private:
    struct EndpointKey
    {
        std::string host;
        std::uint16_t tcpPort = 0U;
        std::uint16_t udpPort = 0U;

        [[nodiscard]] auto operator<=>(const EndpointKey&) const = default;
    };

    struct WorkerRecord
    {
        WorkerConnectionId connection;
        dxa::protocol::WorkerRegister registration;
        WorkerState state = WorkerState::Idle;
        std::optional<dxa::protocol::ReservationId> reservation;
        std::optional<dxa::protocol::MatchId> match;
    };

    [[nodiscard]] WorkerRegistryResult CloseForProtocolViolation(
        WorkerConnectionId connection);
    void ClearReservation(WorkerRecord& worker);

    std::map<dxa::protocol::WorkerId, WorkerRecord> workers_;
    std::map<WorkerConnectionId, dxa::protocol::WorkerId> connections_;
    std::map<dxa::protocol::ReservationId, dxa::protocol::WorkerId>
        reservations_;
    std::map<EndpointKey, dxa::protocol::WorkerId> endpoints_;
};
} // namespace dxa::lobby
