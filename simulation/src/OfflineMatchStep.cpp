#include "OfflineMatchInternal.hpp"

#include <dxa/simulation/MatchResolution.hpp>
#include <dxa/simulation/SafeZone.hpp>

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

[[nodiscard]] std::vector<ContenderRankInput> BuildContenderRanks(
    const std::vector<CombatActor>& actors)
{
    std::vector<ContenderRankInput> ranks;
    for (const CombatActor& actor : actors)
    {
        if (actor.role == ActorRole::Contender)
        {
            ranks.push_back(ContenderRankInput{
                actor.id,
                actor.alive,
                actor.health,
                actor.eliminations});
        }
    }
    if (ranks.empty())
    {
        throw std::logic_error{"offline match has no contenders"};
    }
    return ranks;
}

[[nodiscard]] std::uint32_t CountAliveContenders(
    const std::vector<CombatActor>& actors) noexcept
{
    return static_cast<std::uint32_t>(std::count_if(
        actors.begin(),
        actors.end(),
        [](const CombatActor& actor) {
            return actor.role == ActorRole::Contender && actor.alive;
        }));
}

[[nodiscard]] ActorId SoleAliveContender(const std::vector<CombatActor>& actors)
{
    std::optional<ActorId> winner;
    for (const CombatActor& actor : actors)
    {
        if (actor.role != ActorRole::Contender || !actor.alive)
        {
            continue;
        }
        if (winner.has_value())
        {
            throw std::logic_error{"more than one contender remains alive"};
        }
        winner = actor.id;
    }
    if (!winner.has_value())
    {
        throw std::logic_error{"no contender remains alive"};
    }
    return *winner;
}

void PreserveCombatWipe(
    std::vector<CombatActor>& actors,
    const std::vector<ContenderRankInput>& preCombatRanks,
    CombatResolution& resolution)
{
    if (CountAliveContenders(actors) != 0U)
    {
        return;
    }

    const ActorId protectedActor = SelectSurvivalWinner(preCombatRanks);
    const std::optional<std::size_t> protectedIndex =
        FindActorIndex(actors, protectedActor);
    if (!protectedIndex.has_value())
    {
        throw std::logic_error{"combat wipe winner is missing"};
    }

    const auto death = std::find_if(
        resolution.deaths.begin(),
        resolution.deaths.end(),
        [protectedActor](const DeathRecord& record) {
            return record.victim == protectedActor;
        });
    if (death == resolution.deaths.end())
    {
        throw std::logic_error{"combat wipe winner has no death record"};
    }
    if (death->killer.has_value())
    {
        const std::optional<std::size_t> killerIndex =
            FindActorIndex(actors, *death->killer);
        if (!killerIndex.has_value() || actors[*killerIndex].eliminations == 0U)
        {
            throw std::logic_error{"combat wipe killer attribution is inconsistent"};
        }
        --actors[*killerIndex].eliminations;
    }
    resolution.deaths.erase(death);

    const auto priorRank = std::find_if(
        preCombatRanks.begin(),
        preCombatRanks.end(),
        [protectedActor](const ContenderRankInput& rank) {
            return rank.id == protectedActor;
        });
    const auto damage = std::find_if(
        resolution.damage.begin(),
        resolution.damage.end(),
        [protectedActor](const DamageRecord& record) {
            return record.target == protectedActor;
        });
    if (priorRank == preCombatRanks.end() || damage == resolution.damage.end())
    {
        throw std::logic_error{"combat wipe damage record is inconsistent"};
    }
    if (priorRank->health <= 1)
    {
        resolution.damage.erase(damage);
    }
    else
    {
        damage->amount = priorRank->health - 1;
    }
    actors[*protectedIndex].health = 1;
    actors[*protectedIndex].alive = true;
}

void AddCombatEvents(
    const std::uint32_t tick,
    const CombatResolution& resolution,
    std::vector<MatchEvent>& events)
{
    for (const DamageRecord& damage : resolution.damage)
    {
        if (damage.amount <= 0)
        {
            throw std::logic_error{"combat emitted non-positive damage"};
        }
        events.push_back(MatchEvent{
            tick,
            MatchEventType::DamageApplied,
            damage.target,
            damage.primarySource,
            std::nullopt,
            damage.amount});
    }
    for (const DeathRecord& death : resolution.deaths)
    {
        events.push_back(MatchEvent{
            tick,
            MatchEventType::ActorDied,
            death.victim,
            death.killer});
    }
}

void ApplySafeZoneDamage(
    std::vector<CombatActor>& actors,
    const std::uint32_t tick,
    const std::uint32_t tickRate,
    std::vector<MatchEvent>& events)
{
    const std::int32_t damage = SafeZoneDamageForTick(tick, tickRate);
    if (damage == 0)
    {
        return;
    }

    const SafeZoneState zone = EvaluateSafeZone(tick, tickRate);
    const std::vector<ContenderRankInput> preZoneRanks = BuildContenderRanks(actors);
    std::uint32_t rawContenderSurvivors = 0;
    for (const CombatActor& actor : actors)
    {
        if (actor.role != ActorRole::Contender || !actor.alive)
        {
            continue;
        }
        if (!IsOutsideSafeZone(actor.position, zone) || actor.health > damage)
        {
            ++rawContenderSurvivors;
        }
    }

    std::optional<ActorId> protectedActor;
    if (rawContenderSurvivors == 0U)
    {
        protectedActor = SelectSurvivalWinner(preZoneRanks);
    }

    for (CombatActor& actor : actors)
    {
        if (!actor.alive || !IsOutsideSafeZone(actor.position, zone))
        {
            continue;
        }
        const std::int32_t targetHealth =
            protectedActor.has_value()
                && actor.role == ActorRole::Contender
                && actor.id == *protectedActor
            ? 1
            : std::max(0, actor.health - damage);
        const std::int32_t appliedDamage = actor.health - targetHealth;
        actor.health = targetHealth;
        if (appliedDamage > 0)
        {
            events.push_back(MatchEvent{
                tick,
                MatchEventType::DamageApplied,
                actor.id,
                std::nullopt,
                std::nullopt,
                appliedDamage});
        }
        if (actor.health == 0)
        {
            actor.alive = false;
            events.push_back(MatchEvent{
                tick,
                MatchEventType::ActorDied,
                actor.id});
        }
    }
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

    const auto processCommand = [&](const MatchCommand& command) {
        const std::optional<std::size_t> actorIndex = FindActorIndex(actors, command.actor);
        if (!actorIndex.has_value() || !actors[*actorIndex].alive)
        {
            AddRejectedEvent(tickEvents, tick, command.actor);
            return;
        }
        if (!command.moveDestination.has_value() && !command.attackTarget.has_value())
        {
            AddRejectedEvent(tickEvents, tick, command.actor);
            return;
        }

        const CombatActor& actor = actors[*actorIndex];
        if (command.moveDestination.has_value()
            && (!IsFinite(*command.moveDestination)
                || !navMesh.FindPath(actor.position, *command.moveDestination).has_value()))
        {
            AddRejectedEvent(tickEvents, tick, command.actor);
            return;
        }

        if (command.attackTarget.has_value())
        {
            const std::optional<std::size_t> targetIndex =
                FindActorIndex(actors, *command.attackTarget);
            if (!targetIndex.has_value()
                || !CanAttackTarget(actor, actors[*targetIndex]))
            {
                AddRejectedEvent(tickEvents, tick, command.actor);
                return;
            }
        }

        if (command.moveDestination.has_value()
            && !agents[*actorIndex].SetDestination(*command.moveDestination))
        {
            throw std::logic_error{"validated match destination was rejected by NavAgent"};
        }
        selectedCommands.insert_or_assign(command.actor, command);
    };
    for (const MatchCommand& command : commands)
    {
        processCommand(command);
    }
    if (config.enableInternalBots
        && (tick - 1U) % config.botDecisionIntervalTicks == 0U)
    {
        const std::vector<MatchCommand> botCommands = BuildInternalBotCommands();
        for (const MatchCommand& command : botCommands)
        {
            processCommand(command);
        }
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

    const std::vector<ContenderRankInput> preCombatRanks = BuildContenderRanks(actors);
    std::vector<AttackIntent> attackIntents;
    attackIntents.reserve(selectedCommands.size());
    for (const auto& [actorId, command] : selectedCommands)
    {
        if (command.attackTarget.has_value())
        {
            attackIntents.push_back(AttackIntent{actorId, *command.attackTarget});
        }
    }
    CombatResolution combat = ResolveAttacks(actors, attackIntents);
    PreserveCombatWipe(actors, preCombatRanks, combat);
    AddCombatEvents(tick, combat, tickEvents);
    ApplySafeZoneDamage(actors, tick, config.tickRate, tickEvents);

    const std::uint32_t aliveContenders = CountAliveContenders(actors);
    if (aliveContenders == 0U)
    {
        throw std::logic_error{"offline match resolution left no contender alive"};
    }
    if (aliveContenders == 1U)
    {
        const ActorId winner = SoleAliveContender(actors);
        result = MatchResult{winner, MatchEndReason::LastSurvivor, tick};
        phase = MatchPhase::Finished;
        tickEvents.push_back(MatchEvent{
            tick,
            MatchEventType::MatchFinished,
            winner});
    }
    else if (tick >= config.hardTimeoutTick)
    {
        const ActorId winner = SelectSurvivalWinner(BuildContenderRanks(actors));
        for (CombatActor& actor : actors)
        {
            if (actor.role != ActorRole::Contender || !actor.alive)
            {
                continue;
            }
            if (actor.id == winner)
            {
                actor.health = 1;
                continue;
            }
            actor.health = 0;
            actor.alive = false;
            tickEvents.push_back(MatchEvent{
                tick,
                MatchEventType::ActorDied,
                actor.id});
        }
        result = MatchResult{winner, MatchEndReason::TimeLimit, tick};
        phase = MatchPhase::Finished;
        tickEvents.push_back(MatchEvent{
            tick,
            MatchEventType::MatchFinished,
            winner});
    }
    else
    {
        phase = tick >= config.suddenDeathTick
            ? MatchPhase::SuddenDeath
            : MatchPhase::Running;
    }
    RecordEvents(std::move(tickEvents));
}
} // namespace dxa::simulation
