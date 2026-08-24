#pragma once

#include <dxa/protocol/GameTypes.hpp>
#include <dxa/protocol/Ids.hpp>
#include <dxa/protocol/WorkerControlMessages.hpp>

#include <chrono>
#include <cstdint>
#include <variant>
#include <vector>

namespace dxa::lobby
{
struct ReserveMatchAction
{
    dxa::protocol::ReservationId reservation;
    dxa::protocol::RoomId room;
    dxa::protocol::MatchId match;
    dxa::protocol::PlayerId requester;
    std::uint32_t requestId = 0U;
    std::uint32_t seed = 0U;
    std::chrono::steady_clock::time_point issuedAt;
    std::vector<dxa::protocol::ReservedParticipant> participants;

    [[nodiscard]] bool operator==(const ReserveMatchAction&) const = default;
};

struct CancelReservationAction
{
    dxa::protocol::ReservationId reservation;
    dxa::protocol::MatchId match;

    [[nodiscard]] bool operator==(const CancelReservationAction&) const = default;
};

using LobbyRuntimeAction = std::variant<
    ReserveMatchAction,
    CancelReservationAction>;

struct ReservationReadyEvent
{
    dxa::protocol::ReservationId reservation;
    dxa::protocol::MatchId match;
    dxa::protocol::WorkerId worker;
    dxa::protocol::GameEndpoint endpoint;

    [[nodiscard]] bool operator==(const ReservationReadyEvent&) const = default;
};

struct ReservationFailedEvent
{
    dxa::protocol::ReservationId reservation;
    dxa::protocol::MatchId match;

    [[nodiscard]] bool operator==(const ReservationFailedEvent&) const = default;
};

struct MatchFinishedEvent
{
    dxa::protocol::WorkerId worker;
    dxa::protocol::MatchId match;

    [[nodiscard]] bool operator==(const MatchFinishedEvent&) const = default;
};

struct MatchUnavailableEvent
{
    dxa::protocol::WorkerId worker;
    dxa::protocol::MatchId match;

    [[nodiscard]] bool operator==(const MatchUnavailableEvent&) const = default;
};

using WorkerEvent = std::variant<
    ReservationReadyEvent,
    ReservationFailedEvent,
    MatchFinishedEvent,
    MatchUnavailableEvent>;
} // namespace dxa::lobby
