#pragma once

#include <dxa/protocol/GameTypes.hpp>
#include <dxa/protocol/Ids.hpp>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace dxa::protocol
{
struct ReservedParticipant
{
    PlayerId player;
    MatchTicketValue ticket;

    [[nodiscard]] bool operator==(const ReservedParticipant&) const = default;
};

struct WorkerRegister
{
    WorkerId worker;
    std::string advertisedHost;
    std::uint16_t gameTcpPort = 0U;
    std::uint16_t gameUdpPort = 0U;
    std::uint8_t capacity = 1U;

    [[nodiscard]] bool operator==(const WorkerRegister&) const = default;
};

struct WorkerRegistered
{
    WorkerId worker;

    [[nodiscard]] bool operator==(const WorkerRegistered&) const = default;
};

struct ReserveMatch
{
    ReservationId reservation;
    MatchId match;
    std::uint32_t seed = 0U;
    std::uint32_t ticketLifetimeMilliseconds = 60000U;
    std::vector<ReservedParticipant> participants;

    [[nodiscard]] bool operator==(const ReserveMatch&) const = default;
};

struct ReserveMatchReady
{
    ReservationId reservation;
    MatchId match;

    [[nodiscard]] bool operator==(const ReserveMatchReady&) const = default;
};

struct ReserveMatchRejected
{
    ReservationId reservation;
    MatchId match;
    WorkerReservationReject reason = WorkerReservationReject::InvalidReservation;

    [[nodiscard]] bool operator==(const ReserveMatchRejected&) const = default;
};

struct CancelMatchReservation
{
    ReservationId reservation;
    MatchId match;

    [[nodiscard]] bool operator==(const CancelMatchReservation&) const = default;
};

struct MatchReservationCancelled
{
    ReservationId reservation;
    MatchId match;

    [[nodiscard]] bool operator==(const MatchReservationCancelled&) const = default;
};

struct MatchFinished
{
    MatchId match;
    EntityId winner;
    bool hasWinner = false;
    MatchCompletionReason reason = MatchCompletionReason::LastSurvivor;
    std::uint32_t finishedTick = 0U;

    [[nodiscard]] bool operator==(const MatchFinished&) const = default;
};

using WorkerToLobbyMessage = std::variant<
    WorkerRegister,
    ReserveMatchReady,
    ReserveMatchRejected,
    MatchReservationCancelled,
    MatchFinished>;

using LobbyToWorkerMessage = std::variant<
    WorkerRegistered,
    ReserveMatch,
    CancelMatchReservation>;
} // namespace dxa::protocol
