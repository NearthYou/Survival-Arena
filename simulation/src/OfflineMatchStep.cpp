#include "OfflineMatchInternal.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace dxa::simulation
{
namespace
{
constexpr std::uint64_t FnvPrime = 1099511628211ULL;

[[nodiscard]] std::optional<std::size_t> FindActorIndex(
    const std::vector<CombatActor>& actors,
    const ActorId id)
{
    const auto found = std::lower_bound(
        actors.begin(),
        actors.end(),
        id,
        [](const CombatActor& actor, const ActorId candidate) {
            return actor.id < candidate;
        });
    if (found == actors.end() || found->id != id)
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(actors.begin(), found));
}

[[nodiscard]] bool CanAttackTarget(
    const CombatActor& attacker,
    const CombatActor& target) noexcept
{
    return attacker.id != target.id
        && target.alive
        && (attacker.role == ActorRole::Contender
            || target.role == ActorRole::Contender);
}

void AddRejectedEvent(
    std::vector<MatchEvent>& events,
    const std::uint32_t tick,
    const ActorId actor)
{
    events.push_back(MatchEvent{
        tick,
        MatchEventType::CommandRejected,
        actor});
}

void HashByte(std::uint64_t& hash, const std::uint8_t value) noexcept
{
    hash ^= value;
    hash *= FnvPrime;
}

void HashUint32(std::uint64_t& hash, std::uint32_t value) noexcept
{
    for (std::uint32_t byte = 0; byte < 4U; ++byte)
    {
        HashByte(hash, static_cast<std::uint8_t>(value & 0xFFU));
        value >>= 8U;
    }
}

template <typename T>
void HashOptionalId(std::uint64_t& hash, const std::optional<T> value) noexcept
{
    static_assert(std::is_same_v<T, ActorId> || std::is_same_v<T, LootId>);
    HashByte(hash, value.has_value() ? 1U : 0U);
    if (value.has_value())
    {
        HashUint32(hash, static_cast<std::uint32_t>(*value));
    }
}

void HashEvent(std::uint64_t& hash, const MatchEvent& event) noexcept
{
    HashUint32(hash, event.tick);
    HashUint32(hash, static_cast<std::uint32_t>(event.type));
    HashUint32(hash, event.actor);
    HashOptionalId(hash, event.subject);
    HashOptionalId(hash, event.loot);
    HashUint32(hash, std::bit_cast<std::uint32_t>(event.amount));
    HashByte(hash, event.weapon.has_value() ? 1U : 0U);
    if (event.weapon.has_value())
    {
        HashUint32(hash, static_cast<std::uint32_t>(*event.weapon));
    }
}

[[nodiscard]] auto EventSortKey(const MatchEvent& event)
{
    return std::tuple{
        static_cast<std::uint32_t>(event.type),
        event.actor,
        event.subject,
        event.loot};
}
} // namespace

void OfflineMatch::Impl::RecordEvents(std::vector<MatchEvent> tickEvents)
{
    std::stable_sort(
        tickEvents.begin(),
        tickEvents.end(),
        [](const MatchEvent& left, const MatchEvent& right) {
            return EventSortKey(left) < EventSortKey(right);
        });
    for (const MatchEvent& event : tickEvents)
    {
        HashEvent(eventChecksum, event);
        events.push_back(event);
    }
}

void OfflineMatch::Impl::Step()
{
    if (tick == std::numeric_limits<std::uint32_t>::max())
    {
        throw std::overflow_error{"offline match tick overflow"};
    }
    if (actors.size() != agents.size())
    {
        throw std::logic_error{"offline match actor and agent counts differ"};
    }

    ++tick;
    selectedCommands.clear();
    std::vector<MatchEvent> tickEvents;
    std::vector<MatchCommand> commands;
    commands.swap(queuedCommands);

    for (const MatchCommand& command : commands)
    {
        const std::optional<std::size_t> actorIndex = FindActorIndex(actors, command.actor);
        if (!actorIndex.has_value() || !actors[*actorIndex].alive)
        {
            AddRejectedEvent(tickEvents, tick, command.actor);
            continue;
        }
        if (!command.moveDestination.has_value() && !command.attackTarget.has_value())
        {
            AddRejectedEvent(tickEvents, tick, command.actor);
            continue;
        }

        const CombatActor& actor = actors[*actorIndex];
        if (command.moveDestination.has_value()
            && (!IsFinite(*command.moveDestination)
                || !navMesh.FindPath(actor.position, *command.moveDestination).has_value()))
        {
            AddRejectedEvent(tickEvents, tick, command.actor);
            continue;
        }

        if (command.attackTarget.has_value())
        {
            const std::optional<std::size_t> targetIndex =
                FindActorIndex(actors, *command.attackTarget);
            if (!targetIndex.has_value()
                || !CanAttackTarget(actor, actors[*targetIndex]))
            {
                AddRejectedEvent(tickEvents, tick, command.actor);
                continue;
            }
        }

        if (command.moveDestination.has_value()
            && !agents[*actorIndex].SetDestination(*command.moveDestination))
        {
            throw std::logic_error{"validated match destination was rejected by NavAgent"};
        }
        selectedCommands.insert_or_assign(command.actor, command);
    }

    const float tickSeconds = 1.0F / static_cast<float>(config.tickRate);
    for (std::size_t index = 0; index < actors.size(); ++index)
    {
        if (!actors[index].alive)
        {
            continue;
        }
        agents[index].Tick(tickSeconds);
        actors[index].position = agents[index].Position();
    }

    for (CombatActor& actor : actors)
    {
        if (actor.role != ActorRole::Contender || !actor.alive)
        {
            continue;
        }
        const std::optional<LootPickupResult> pickup = ResolveNearestLootPickup(
            actor,
            loot,
            config.pickupRadius);
        if (!pickup.has_value())
        {
            continue;
        }
        tickEvents.push_back(MatchEvent{
            tick,
            MatchEventType::LootPickedUp,
            actor.id,
            std::nullopt,
            pickup->loot});
        if (pickup->equippedWeapon.has_value())
        {
            tickEvents.push_back(MatchEvent{
                tick,
                MatchEventType::WeaponChanged,
                actor.id,
                std::nullopt,
                pickup->loot,
                0,
                pickup->equippedWeapon});
        }
        if (pickup->healedAmount > 0)
        {
            tickEvents.push_back(MatchEvent{
                tick,
                MatchEventType::ActorHealed,
                actor.id,
                std::nullopt,
                pickup->loot,
                pickup->healedAmount});
        }
    }

    TickWeaponCooldowns(actors);
    RecordEvents(std::move(tickEvents));
}
} // namespace dxa::simulation
