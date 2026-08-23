#include <dxa/simulation/Combat.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace dxa::simulation
{
namespace
{
struct DamageContribution
{
    ActorId source = 0;
    std::int32_t amount = 0;
};

using ActorIndex = std::map<ActorId, std::size_t>;
using DamageContributions = std::map<ActorId, std::vector<DamageContribution>>;

void ValidateRole(const ActorRole role)
{
    switch (role)
    {
    case ActorRole::Contender:
    case ActorRole::Neutral:
        return;
    }
    throw std::invalid_argument{"combat actor role is invalid"};
}

[[nodiscard]] ActorIndex ValidateActors(const std::span<const CombatActor> actors)
{
    ActorIndex index;
    for (std::size_t actorIndex = 0; actorIndex < actors.size(); ++actorIndex)
    {
        const CombatActor& actor = actors[actorIndex];
        ValidateRole(actor.role);
        (void)WeaponDefinitionFor(actor.weapon);
        if (!IsFinite(actor.position)
            || actor.health < 0
            || actor.health > 100
            || actor.alive != (actor.health > 0))
        {
            throw std::invalid_argument{"combat actor state is invalid"};
        }
        if (!index.emplace(actor.id, actorIndex).second)
        {
            throw std::invalid_argument{"combat actor IDs must be unique"};
        }
    }
    return index;
}

[[nodiscard]] bool CanTarget(
    const CombatActor& attacker,
    const CombatActor& target) noexcept
{
    return attacker.id != target.id
        && target.alive
        && (attacker.role == ActorRole::Contender
            || target.role == ActorRole::Contender);
}

[[nodiscard]] CombatActor* FindActor(
    const ActorIndex& index,
    const std::span<CombatActor> actors,
    const ActorId id) noexcept
{
    const auto found = index.find(id);
    return found == index.end() ? nullptr : &actors[found->second];
}

[[nodiscard]] const CombatActor* FindActor(
    const ActorIndex& index,
    const std::span<const CombatActor> actors,
    const ActorId id) noexcept
{
    const auto found = index.find(id);
    return found == index.end() ? nullptr : &actors[found->second];
}

void ValidateUniqueAttackers(const std::span<const AttackIntent> intents)
{
    std::set<ActorId> attackers;
    for (const AttackIntent& intent : intents)
    {
        if (!attackers.insert(intent.attacker).second)
        {
            throw std::invalid_argument{"an actor may submit only one attack intent per tick"};
        }
    }
}

[[nodiscard]] ActorId PrimarySource(
    const std::vector<DamageContribution>& contributions)
{
    const auto primary = std::min_element(
        contributions.begin(),
        contributions.end(),
        [](const DamageContribution& left, const DamageContribution& right) {
            if (left.amount != right.amount)
            {
                return left.amount > right.amount;
            }
            return left.source < right.source;
        });
    if (primary == contributions.end())
    {
        throw std::logic_error{"damage contribution set is empty"};
    }
    return primary->source;
}
} // namespace

WeaponDefinition WeaponDefinitionFor(const WeaponType weapon)
{
    switch (weapon)
    {
    case WeaponType::Blade:
        return WeaponDefinition{2.2F, 0.0F, 24, 21U};
    case WeaponType::Rifle:
        return WeaponDefinition{18.0F, 0.0F, 12, 12U};
    case WeaponType::ArcPulse:
        return WeaponDefinition{10.0F, 5.0F, 18, 90U};
    }
    throw std::invalid_argument{"weapon type is invalid"};
}

void TickWeaponCooldowns(const std::span<CombatActor> actors)
{
    (void)ValidateActors(actors);
    for (CombatActor& actor : actors)
    {
        if (actor.cooldownTicksRemaining > 0U)
        {
            --actor.cooldownTicksRemaining;
        }
    }
}

CombatResolution ResolveAttacks(
    const std::span<CombatActor> actors,
    const std::span<const AttackIntent> intents)
{
    const ActorIndex actorIndex = ValidateActors(actors);
    ValidateUniqueAttackers(intents);

    std::vector<AttackIntent> sortedIntents{intents.begin(), intents.end()};
    std::sort(
        sortedIntents.begin(),
        sortedIntents.end(),
        [](const AttackIntent& left, const AttackIntent& right) {
            if (left.attacker != right.attacker)
            {
                return left.attacker < right.attacker;
            }
            return left.target < right.target;
        });

    CombatResolution resolution;
    DamageContributions contributionsByTarget;
    for (const AttackIntent& intent : sortedIntents)
    {
        const CombatActor* const attacker = FindActor(
            actorIndex,
            std::span<const CombatActor>{actors},
            intent.attacker);
        const CombatActor* const target = FindActor(
            actorIndex,
            std::span<const CombatActor>{actors},
            intent.target);
        if (attacker == nullptr
            || target == nullptr
            || !attacker->alive
            || attacker->cooldownTicksRemaining != 0U
            || !CanTarget(*attacker, *target))
        {
            continue;
        }

        const WeaponDefinition weapon = WeaponDefinitionFor(attacker->weapon);
        if (Distance(attacker->position, target->position) > weapon.range)
        {
            continue;
        }

        resolution.acceptedIntents.push_back(intent);
        if (weapon.effectRadius == 0.0F)
        {
            contributionsByTarget[target->id].push_back(
                DamageContribution{attacker->id, weapon.damage});
            continue;
        }

        for (const CombatActor& candidate : actors)
        {
            if (CanTarget(*attacker, candidate)
                && Distance(candidate.position, target->position) <= weapon.effectRadius)
            {
                contributionsByTarget[candidate.id].push_back(
                    DamageContribution{attacker->id, weapon.damage});
            }
        }
    }

    for (const AttackIntent& accepted : resolution.acceptedIntents)
    {
        CombatActor* const attacker = FindActor(actorIndex, actors, accepted.attacker);
        attacker->cooldownTicksRemaining =
            WeaponDefinitionFor(attacker->weapon).cooldownTicks;
    }

    for (const auto& [targetId, contributions] : contributionsByTarget)
    {
        CombatActor* const target = FindActor(actorIndex, actors, targetId);
        std::int64_t totalDamage = 0;
        for (const DamageContribution& contribution : contributions)
        {
            totalDamage += contribution.amount;
        }
        if (totalDamage > std::numeric_limits<std::int32_t>::max())
        {
            throw std::overflow_error{"combat damage exceeds supported range"};
        }

        const std::int32_t appliedDamage = std::min(
            target->health,
            static_cast<std::int32_t>(totalDamage));
        const ActorId primarySource = PrimarySource(contributions);
        target->health -= appliedDamage;
        resolution.damage.push_back(
            DamageRecord{targetId, appliedDamage, primarySource});
        if (target->health == 0)
        {
            target->alive = false;
            resolution.deaths.push_back(DeathRecord{targetId, primarySource});
        }
    }

    for (const DeathRecord& death : resolution.deaths)
    {
        if (!death.killer.has_value())
        {
            continue;
        }
        CombatActor* const killer = FindActor(actorIndex, actors, *death.killer);
        if (killer->eliminations == std::numeric_limits<std::uint32_t>::max())
        {
            throw std::overflow_error{"combat elimination count overflow"};
        }
        ++killer->eliminations;
    }
    return resolution;
}
} // namespace dxa::simulation
