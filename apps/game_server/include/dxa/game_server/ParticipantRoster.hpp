#pragma once

#include <dxa/protocol/GameTypes.hpp>
#include <dxa/protocol/Ids.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace dxa::game_server
{
enum class ParticipantSlotState : std::uint8_t
{
    Pending,
    Authenticated,
    Unavailable
};

struct GameConnectionId
{
    std::uint64_t value = 0U;

    [[nodiscard]] auto operator<=>(const GameConnectionId&) const = default;
};

struct ParticipantSlot
{
    dxa::protocol::PlayerId player;
    dxa::protocol::EntityId actor;
    ParticipantSlotState state = ParticipantSlotState::Pending;
    std::optional<GameConnectionId> connection{};
    std::optional<dxa::protocol::UdpSessionToken> udpToken{};
};

class ParticipantRoster
{
public:
    explicit ParticipantRoster(std::vector<dxa::protocol::PlayerId> players);

    [[nodiscard]] bool Authenticate(
        dxa::protocol::PlayerId player,
        GameConnectionId connection,
        dxa::protocol::UdpSessionToken token);
    [[nodiscard]] bool MarkUnavailable(dxa::protocol::PlayerId player);
    [[nodiscard]] dxa::protocol::EntityId ActorFor(
        dxa::protocol::PlayerId player) const;
    [[nodiscard]] dxa::protocol::PlayerId PlayerFor(
        dxa::protocol::EntityId actor) const;
    [[nodiscard]] std::optional<GameConnectionId> ConnectionFor(
        dxa::protocol::PlayerId player) const noexcept;
    [[nodiscard]] bool ReadyToStart() const noexcept;
    [[nodiscard]] std::size_t AuthenticatedCount() const noexcept;
    [[nodiscard]] std::vector<dxa::protocol::PlayerId>
    UnavailablePlayers() const;

private:
    [[nodiscard]] ParticipantSlot* Find(dxa::protocol::PlayerId player) noexcept;
    [[nodiscard]] const ParticipantSlot* Find(
        dxa::protocol::PlayerId player) const noexcept;

    std::vector<ParticipantSlot> slots_;
};
} // namespace dxa::game_server
