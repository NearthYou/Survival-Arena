#include <dxa/simulation/OfflineBotController.hpp>

#include "OfflineMatchInternal.hpp"

#include <dxa/simulation/Combat.hpp>
#include <dxa/simulation/SpatialIndex.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dxa::simulation
{
namespace
{
[[nodiscard]] double DistanceSquaredDouble(const Vec2 left, const Vec2 right) noexcept
{
    const double deltaX = static_cast<double>(left.x) - static_cast<double>(right.x);
    const double deltaZ = static_cast<double>(left.z) - static_cast<double>(right.z);
    return deltaX * deltaX + deltaZ * deltaZ;
}

void ValidateActor(const ActorSnapshot& actor)
{
    switch (actor.role)
    {
    case ActorRole::Contender:
    case ActorRole::Neutral:
        break;
    default:
        throw std::invalid_argument{"bot actor role is invalid"};
    }
    switch (actor.neutralArchetype)
    {
    case NeutralArchetype::None:
    case NeutralArchetype::Melee:
    case NeutralArchetype::Ranged:
        break;
    default:
        throw std::invalid_argument{"bot neutral archetype is invalid"};
    }
    (void)WeaponDefinitionFor(actor.weapon);
    if (!IsFinite(actor.position)
        || actor.health < 0
        || actor.health > 100
        || actor.alive != (actor.health > 0))
    {
        throw std::invalid_argument{"bot actor state is invalid"};
    }
}

void ValidateLoot(const LootSnapshot& loot)
{
    switch (loot.type)
    {
    case LootType::Rifle:
    case LootType::ArcPulse:
    case LootType::MedKit:
        break;
    default:
        throw std::invalid_argument{"bot loot type is invalid"};
    }
    if (!IsFinite(loot.position))
    {
        throw std::invalid_argument{"bot loot position must be finite"};
    }
}

void ValidatePerception(const ActorSnapshot& self, const BotPerception& perception)
{
    ValidateActor(self);
    std::set<ActorId> actorIds;
    for (const ActorSnapshot& actor : perception.actors)
    {
        ValidateActor(actor);
        if (!actorIds.insert(actor.id).second)
        {
            throw std::invalid_argument{"bot perception actor IDs must be unique"};
        }
    }
    std::set<LootId> lootIds;
    for (const LootSnapshot& loot : perception.loot)
    {
        ValidateLoot(loot);
        if (!lootIds.insert(loot.id).second)
        {
            throw std::invalid_argument{"bot perception loot IDs must be unique"};
        }
    }
    (void)IsOutsideSafeZone(self.position, perception.safeZone);
}

template <typename Predicate>
[[nodiscard]] const ActorSnapshot* ClosestActor(
    const ActorSnapshot& self,
    const std::span<const ActorSnapshot> actors,
    const float radius,
    Predicate predicate)
{
    const double radiusSquared = static_cast<double>(radius) * radius;
    const ActorSnapshot* closest = nullptr;
    double closestDistance = std::numeric_limits<double>::infinity();
    for (const ActorSnapshot& actor : actors)
    {
        if (actor.id == self.id || !actor.alive || !predicate(actor))
        {
            continue;
        }
        const double distance = DistanceSquaredDouble(self.position, actor.position);
        if (distance > radiusSquared)
        {
            continue;
        }
        if (closest == nullptr
            || distance < closestDistance
            || (distance == closestDistance && actor.id < closest->id))
        {
            closest = &actor;
            closestDistance = distance;
        }
    }
    return closest;
}

template <typename Predicate>
[[nodiscard]] const LootSnapshot* ClosestLoot(
    const ActorSnapshot& self,
    const std::span<const LootSnapshot> loot,
    const float radius,
    Predicate predicate)
{
    const double radiusSquared = static_cast<double>(radius) * radius;
    const LootSnapshot* closest = nullptr;
    double closestDistance = std::numeric_limits<double>::infinity();
    for (const LootSnapshot& item : loot)
    {
        if (!item.active || !predicate(item))
        {
            continue;
        }
        const double distance = DistanceSquaredDouble(self.position, item.position);
        if (distance > radiusSquared)
        {
            continue;
        }
        if (closest == nullptr
            || distance < closestDistance
            || (distance == closestDistance && item.id < closest->id))
        {
            closest = &item;
            closestDistance = distance;
        }
    }
    return closest;
}

[[nodiscard]] MatchCommand MoveDecision(const ActorId actor, const Vec2 destination)
{
    MatchCommand command;
    command.actor = actor;
    command.moveDestination = destination;
    return command;
}

[[nodiscard]] MatchCommand AttackDecision(const ActorId actor, const ActorId target)
{
    MatchCommand command;
    command.actor = actor;
    command.attackTarget = target;
    return command;
}

[[nodiscard]] ActorSnapshot SnapshotActor(
    const CombatActor& actor,
    const NeutralArchetype archetype)
{
    return ActorSnapshot{
        actor.id,
        actor.role,
        archetype,
        actor.position,
        actor.health,
        actor.alive,
        actor.weapon,
        actor.cooldownTicksRemaining,
        actor.eliminations};
}
} // namespace

BotDecision DecideContender(
    const ActorSnapshot& self,
    const BotPerception& perception,
    const MatchConfig& config)
{
    ValidateMatchConfig(config);
    ValidatePerception(self, perception);
    if (self.role != ActorRole::Contender)
    {
        throw std::invalid_argument{"contender decision requires contender role"};
    }
    if (!self.alive)
    {
        return BotDecision{MatchCommand{self.id}, BotDecisionReason::Idle};
    }
    if (IsOutsideSafeZone(self.position, perception.safeZone))
    {
        return BotDecision{
            MoveDecision(self.id, perception.safeZone.center),
            BotDecisionReason::ReturnToZone};
    }

    if (self.health <= 45)
    {
        const LootSnapshot* medKit = ClosestLoot(
            self,
            perception.loot,
            config.contenderPerceptionRadius,
            [](const LootSnapshot& item) { return item.type == LootType::MedKit; });
        if (medKit != nullptr)
        {
            return BotDecision{
                MoveDecision(self.id, medKit->position),
                BotDecisionReason::UseMedKit};
        }
    }

    if (self.weapon == WeaponType::Blade)
    {
        const LootSnapshot* weapon = ClosestLoot(
            self,
            perception.loot,
            config.contenderPerceptionRadius,
            [](const LootSnapshot& item) {
                return item.type == LootType::Rifle
                    || item.type == LootType::ArcPulse;
            });
        if (weapon != nullptr)
        {
            return BotDecision{
                MoveDecision(self.id, weapon->position),
                BotDecisionReason::SeekWeapon};
        }
    }

    const ActorSnapshot* target = ClosestActor(
        self,
        perception.actors,
        config.contenderPerceptionRadius,
        [](const ActorSnapshot&) { return true; });
    if (target == nullptr)
    {
        return BotDecision{MatchCommand{self.id}, BotDecisionReason::Idle};
    }

    const WeaponDefinition weapon = WeaponDefinitionFor(self.weapon);
    if (self.cooldownTicksRemaining == 0U
        && DistanceSquaredDouble(self.position, target->position)
            <= static_cast<double>(weapon.range) * weapon.range)
    {
        return BotDecision{
            AttackDecision(self.id, target->id),
            BotDecisionReason::Attack};
    }
    return BotDecision{
        MoveDecision(self.id, target->position),
        BotDecisionReason::Chase};
}

BotDecision DecideNeutral(
    const ActorSnapshot& self,
    const BotPerception& perception,
    const MatchConfig& config,
    const BehaviorTreeAiController& controller)
{
    ValidateMatchConfig(config);
    ValidatePerception(self, perception);
    if (self.role != ActorRole::Neutral
        || self.neutralArchetype == NeutralArchetype::None)
    {
        throw std::invalid_argument{"neutral decision requires a neutral archetype"};
    }
    if (!self.alive)
    {
        return BotDecision{MatchCommand{self.id}, BotDecisionReason::Idle};
    }

    const ActorSnapshot* target = ClosestActor(
        self,
        perception.actors,
        config.neutralPerceptionRadius,
        [](const ActorSnapshot& actor) {
            return actor.role == ActorRole::Contender;
        });
    AiBlackboard blackboard;
    blackboard.selfPosition = self.position;
    blackboard.cooldownReady = self.cooldownTicksRemaining == 0U;
    blackboard.attackRange = WeaponDefinitionFor(self.weapon).range;
    blackboard.preferredRange = 8.0F;
    blackboard.retreatRange = 3.0F;
    if (target != nullptr)
    {
        blackboard.hasTarget = true;
        blackboard.targetPosition = target->position;
    }

    switch (controller.Tick(blackboard))
    {
    case AiCommandType::Idle:
        return BotDecision{MatchCommand{self.id}, BotDecisionReason::Idle};
    case AiCommandType::MoveToTarget:
        return BotDecision{
            MoveDecision(self.id, target->position),
            BotDecisionReason::Chase};
    case AiCommandType::MoveAwayFromTarget:
    {
        Vec2 away = self.position - target->position;
        if (LengthSquared(away) == 0.0F)
        {
            away = self.id < target->id ? Vec2{-1.0F, 0.0F} : Vec2{1.0F, 0.0F};
        }
        else
        {
            away = Normalize(away);
        }
        return BotDecision{
            MoveDecision(self.id, self.position + away * 4.0F),
            BotDecisionReason::Retreat};
    }
    case AiCommandType::Attack:
        return BotDecision{
            AttackDecision(self.id, target->id),
            BotDecisionReason::Attack};
    }
    throw std::logic_error{"behavior tree returned an unknown command"};
}

std::vector<MatchCommand> OfflineMatch::Impl::BuildInternalBotCommands() const
{
    if (actors.size() != neutralArchetypes.size()
        || actors.size() != neutralControllers.size())
    {
        throw std::logic_error{"offline bot metadata is inconsistent"};
    }

    std::vector<ActorSnapshot> snapshots;
    std::vector<LootSnapshot> lootSnapshots;
    std::vector<SpatialEntity> entities;
    snapshots.reserve(actors.size());
    lootSnapshots.reserve(loot.size());
    entities.reserve(actors.size());

    float minimumX = std::numeric_limits<float>::infinity();
    float minimumZ = std::numeric_limits<float>::infinity();
    float maximumX = -std::numeric_limits<float>::infinity();
    float maximumZ = -std::numeric_limits<float>::infinity();
    for (std::size_t index = 0; index < actors.size(); ++index)
    {
        snapshots.push_back(SnapshotActor(actors[index], neutralArchetypes[index]));
        if (!actors[index].alive)
        {
            continue;
        }
        minimumX = std::min(minimumX, actors[index].position.x);
        minimumZ = std::min(minimumZ, actors[index].position.z);
        maximumX = std::max(maximumX, actors[index].position.x);
        maximumZ = std::max(maximumZ, actors[index].position.z);
        entities.push_back(SpatialEntity{
            actors[index].id,
            Aabb2::Create(actors[index].position, actors[index].position)});
    }
    if (entities.empty())
    {
        return {};
    }
    for (const LootItem& item : loot)
    {
        lootSnapshots.push_back(
            LootSnapshot{item.id, item.type, item.position, item.active});
    }

    const Aabb2 worldBounds = Aabb2::Create(
        {minimumX - 1.0F, minimumZ - 1.0F},
        {maximumX + 1.0F, maximumZ + 1.0F});
    const LooseQuadtree spatialIndex{worldBounds, std::move(entities)};
    const SafeZoneState zone = EvaluateSafeZone(tick, config.tickRate);
    std::vector<MatchCommand> commands;
    commands.reserve(actors.size());

    for (std::size_t index = 0; index < actors.size(); ++index)
    {
        const ActorSnapshot& self = snapshots[index];
        if (!self.alive || (self.role == ActorRole::Contender && self.id == 0U))
        {
            continue;
        }
        const float perceptionRadius = self.role == ActorRole::Contender
            ? config.contenderPerceptionRadius
            : config.neutralPerceptionRadius;
        const SpatialQueryResult nearby = spatialIndex.QueryAabb(Aabb2::Create(
            {self.position.x - perceptionRadius, self.position.z - perceptionRadius},
            {self.position.x + perceptionRadius, self.position.z + perceptionRadius}));
        std::vector<ActorSnapshot> visibleActors;
        visibleActors.reserve(nearby.ids.size());
        for (const SpatialEntityId id : nearby.ids)
        {
            const auto visible = std::lower_bound(
                snapshots.begin(), snapshots.end(), id, [](const ActorSnapshot& actor, const ActorId value) {
                    return actor.id < value;
                });
            if (visible == snapshots.end() || visible->id != id)
            {
                throw std::logic_error{"spatial index returned an unknown actor"};
            }
            visibleActors.push_back(*visible);
        }

        BotDecision decision;
        if (self.role == ActorRole::Contender)
        {
            decision = DecideContender(
                self,
                BotPerception{visibleActors, lootSnapshots, zone},
                config);
        }
        else
        {
            if (neutralControllers[index] == nullptr)
            {
                throw std::logic_error{"neutral actor has no behavior-tree controller"};
            }
            decision = DecideNeutral(
                self,
                BotPerception{visibleActors, lootSnapshots, zone},
                config,
                *neutralControllers[index]);
        }
        if (decision.command.moveDestination.has_value()
            || decision.command.attackTarget.has_value())
        {
            commands.push_back(std::move(decision.command));
        }
    }
    return commands;
}
} // namespace dxa::simulation
