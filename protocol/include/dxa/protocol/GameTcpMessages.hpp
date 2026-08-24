#pragma once

#include <dxa/protocol/GameTypes.hpp>
#include <dxa/protocol/Ids.hpp>

#include <cstdint>
#include <variant>

namespace dxa::protocol
{
struct GameClientHello
{
    MatchId match;
    PlayerId player;
    MatchTicketValue ticket;

    [[nodiscard]] bool operator==(const GameClientHello&) const = default;
};

struct GameServerWelcome
{
    MatchId match;
    PlayerId player;
    EntityId actor;
    std::uint16_t tickRate = GameTickRate;
    std::uint16_t snapshotRate = SnapshotRate;
    std::uint32_t mapId = 1U;
    std::uint32_t navMeshCrc32 = 0U;
    UdpSessionToken udpToken;

    [[nodiscard]] bool operator==(const GameServerWelcome&) const = default;
};

struct GameServerErrorMessage
{
    GameServerErrorCode error = GameServerErrorCode::AuthenticationFailed;

    [[nodiscard]] bool operator==(const GameServerErrorMessage&) const = default;
};

struct GameMatchResult
{
    MatchId match;
    EntityId winner;
    bool hasWinner = false;
    MatchCompletionReason reason = MatchCompletionReason::LastSurvivor;
    std::uint32_t finishedTick = 0U;

    [[nodiscard]] bool operator==(const GameMatchResult&) const = default;
};

using GameClientMessage = std::variant<GameClientHello>;
using GameServerMessage = std::variant<
    GameServerWelcome,
    GameServerErrorMessage,
    GameMatchResult>;
} // namespace dxa::protocol
