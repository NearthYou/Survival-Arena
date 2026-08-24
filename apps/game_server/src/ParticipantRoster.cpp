#include <dxa/game_server/ParticipantRoster.hpp>

#include <dxa/protocol/LobbyTypes.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dxa::game_server
{
ParticipantRoster::ParticipantRoster(
    std::vector<dxa::protocol::PlayerId> players)
{
    if (players.size() < 2U || players.size() > dxa::protocol::RoomCapacity)
    {
        throw std::invalid_argument{
            "participant roster requires 2 to 24 players"};
    }
    std::sort(players.begin(), players.end());
    if (std::adjacent_find(players.begin(), players.end()) != players.end())
    {
        throw std::invalid_argument{
            "participant roster players must be unique"};
    }

    slots_.reserve(players.size());
    for (std::size_t index = 0U; index < players.size(); ++index)
    {
        slots_.push_back(ParticipantSlot{
            players[index],
            dxa::protocol::EntityId{static_cast<std::uint32_t>(index)}});
    }
}

bool ParticipantRoster::Authenticate(
    const dxa::protocol::PlayerId player,
    const GameConnectionId connection,
    const dxa::protocol::UdpSessionToken token)
{
    ParticipantSlot* slot = Find(player);
    if (slot == nullptr
        || slot->state != ParticipantSlotState::Pending
        || connection.value == 0U)
    {
        return false;
    }
    const bool connectionInUse = std::any_of(
        slots_.begin(),
        slots_.end(),
        [connection](const ParticipantSlot& candidate) {
            return candidate.connection == connection;
        });
    if (connectionInUse)
    {
        return false;
    }

    slot->state = ParticipantSlotState::Authenticated;
    slot->connection = connection;
    slot->udpToken = token;
    return true;
}

bool ParticipantRoster::MarkUnavailable(const dxa::protocol::PlayerId player)
{
    ParticipantSlot* slot = Find(player);
    if (slot == nullptr || slot->state == ParticipantSlotState::Unavailable)
    {
        return false;
    }
    slot->state = ParticipantSlotState::Unavailable;
    slot->connection.reset();
    slot->udpToken.reset();
    return true;
}

dxa::protocol::EntityId ParticipantRoster::ActorFor(
    const dxa::protocol::PlayerId player) const
{
    const ParticipantSlot* slot = Find(player);
    if (slot == nullptr)
    {
        throw std::out_of_range{"participant player is absent"};
    }
    return slot->actor;
}

dxa::protocol::PlayerId ParticipantRoster::PlayerFor(
    const dxa::protocol::EntityId actor) const
{
    const std::size_t index = static_cast<std::size_t>(actor.value);
    if (index >= slots_.size() || slots_[index].actor != actor)
    {
        throw std::out_of_range{"participant actor is absent"};
    }
    return slots_[index].player;
}

std::optional<GameConnectionId> ParticipantRoster::ConnectionFor(
    const dxa::protocol::PlayerId player) const noexcept
{
    const ParticipantSlot* slot = Find(player);
    return slot == nullptr ? std::nullopt : slot->connection;
}

bool ParticipantRoster::ReadyToStart() const noexcept
{
    return std::all_of(
        slots_.begin(),
        slots_.end(),
        [](const ParticipantSlot& slot) {
            return slot.state != ParticipantSlotState::Pending;
        });
}

std::size_t ParticipantRoster::AuthenticatedCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        slots_.begin(),
        slots_.end(),
        [](const ParticipantSlot& slot) {
            return slot.state == ParticipantSlotState::Authenticated;
        }));
}

std::vector<dxa::protocol::PlayerId>
ParticipantRoster::UnavailablePlayers() const
{
    std::vector<dxa::protocol::PlayerId> players;
    for (const ParticipantSlot& slot : slots_)
    {
        if (slot.state == ParticipantSlotState::Unavailable)
        {
            players.push_back(slot.player);
        }
    }
    return players;
}

ParticipantSlot* ParticipantRoster::Find(
    const dxa::protocol::PlayerId player) noexcept
{
    const auto slot = std::lower_bound(
        slots_.begin(),
        slots_.end(),
        player,
        [](const ParticipantSlot& candidate,
           const dxa::protocol::PlayerId value) {
            return candidate.player < value;
        });
    return slot == slots_.end() || slot->player != player
        ? nullptr
        : &*slot;
}

const ParticipantSlot* ParticipantRoster::Find(
    const dxa::protocol::PlayerId player) const noexcept
{
    const auto slot = std::lower_bound(
        slots_.begin(),
        slots_.end(),
        player,
        [](const ParticipantSlot& candidate,
           const dxa::protocol::PlayerId value) {
            return candidate.player < value;
        });
    return slot == slots_.end() || slot->player != player
        ? nullptr
        : &*slot;
}
} // namespace dxa::game_server
