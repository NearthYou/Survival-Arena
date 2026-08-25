#include <dxa/game_common/SnapshotAdapter.hpp>

#include <stdexcept>

namespace dxa::game_common
{
namespace
{
[[nodiscard]] dxa::protocol::NetworkActorRole ToNetwork(
    const dxa::simulation::ActorRole value)
{
    switch (value)
    {
    case dxa::simulation::ActorRole::Contender:
        return dxa::protocol::NetworkActorRole::Contender;
    case dxa::simulation::ActorRole::Neutral:
        return dxa::protocol::NetworkActorRole::Neutral;
    }
    throw std::invalid_argument{"simulation actor role is unknown"};
}

[[nodiscard]] dxa::protocol::NetworkNeutralArchetype ToNetwork(
    const dxa::simulation::NeutralArchetype value)
{
    switch (value)
    {
    case dxa::simulation::NeutralArchetype::None:
        return dxa::protocol::NetworkNeutralArchetype::None;
    case dxa::simulation::NeutralArchetype::Melee:
        return dxa::protocol::NetworkNeutralArchetype::Melee;
    case dxa::simulation::NeutralArchetype::Ranged:
        return dxa::protocol::NetworkNeutralArchetype::Ranged;
    }
    throw std::invalid_argument{"simulation neutral archetype is unknown"};
}

[[nodiscard]] dxa::protocol::NetworkWeaponType ToNetwork(
    const dxa::simulation::WeaponType value)
{
    switch (value)
    {
    case dxa::simulation::WeaponType::Blade:
        return dxa::protocol::NetworkWeaponType::Blade;
    case dxa::simulation::WeaponType::Rifle:
        return dxa::protocol::NetworkWeaponType::Rifle;
    case dxa::simulation::WeaponType::ArcPulse:
        return dxa::protocol::NetworkWeaponType::ArcPulse;
    }
    throw std::invalid_argument{"simulation weapon type is unknown"};
}

[[nodiscard]] dxa::protocol::NetworkLootType ToNetwork(
    const dxa::simulation::LootType value)
{
    switch (value)
    {
    case dxa::simulation::LootType::Rifle:
        return dxa::protocol::NetworkLootType::Rifle;
    case dxa::simulation::LootType::ArcPulse:
        return dxa::protocol::NetworkLootType::ArcPulse;
    case dxa::simulation::LootType::MedKit:
        return dxa::protocol::NetworkLootType::MedKit;
    }
    throw std::invalid_argument{"simulation loot type is unknown"};
}

[[nodiscard]] dxa::protocol::NetworkMatchPhase ToNetwork(
    const dxa::simulation::MatchPhase value)
{
    switch (value)
    {
    case dxa::simulation::MatchPhase::Waiting:
        return dxa::protocol::NetworkMatchPhase::Waiting;
    case dxa::simulation::MatchPhase::Running:
        return dxa::protocol::NetworkMatchPhase::Running;
    case dxa::simulation::MatchPhase::SuddenDeath:
        return dxa::protocol::NetworkMatchPhase::SuddenDeath;
    case dxa::simulation::MatchPhase::Finished:
        return dxa::protocol::NetworkMatchPhase::Finished;
    }
    throw std::invalid_argument{"simulation match phase is unknown"};
}

[[nodiscard]] dxa::protocol::NetworkSafeZoneStage ToNetwork(
    const dxa::simulation::SafeZoneStage value)
{
    switch (value)
    {
    case dxa::simulation::SafeZoneStage::Stage1:
        return dxa::protocol::NetworkSafeZoneStage::Stage1;
    case dxa::simulation::SafeZoneStage::Stage2:
        return dxa::protocol::NetworkSafeZoneStage::Stage2;
    case dxa::simulation::SafeZoneStage::Stage3:
        return dxa::protocol::NetworkSafeZoneStage::Stage3;
    case dxa::simulation::SafeZoneStage::Stage4:
        return dxa::protocol::NetworkSafeZoneStage::Stage4;
    case dxa::simulation::SafeZoneStage::SuddenDeath:
        return dxa::protocol::NetworkSafeZoneStage::SuddenDeath;
    }
    throw std::invalid_argument{"simulation safe zone stage is unknown"};
}

[[nodiscard]] dxa::protocol::NetworkMatchEndReason ToNetwork(
    const dxa::simulation::MatchEndReason value)
{
    switch (value)
    {
    case dxa::simulation::MatchEndReason::LastSurvivor:
        return dxa::protocol::NetworkMatchEndReason::LastSurvivor;
    case dxa::simulation::MatchEndReason::TimeLimit:
        return dxa::protocol::NetworkMatchEndReason::TimeLimit;
    }
    throw std::invalid_argument{"simulation match end reason is unknown"};
}
} // namespace

dxa::protocol::GameSnapshot ToGameSnapshot(
    const dxa::simulation::MatchSnapshot& snapshot)
{
    dxa::protocol::GameSnapshot converted;
    converted.phase = ToNetwork(snapshot.phase);
    converted.safeZoneStage = ToNetwork(snapshot.safeZoneStage);
    converted.safeZoneCenter = {
        snapshot.safeZoneCenter.x,
        snapshot.safeZoneCenter.z};
    converted.safeZoneRadius = snapshot.safeZoneRadius;
    converted.aliveContenders = snapshot.aliveContenders;
    converted.eventChecksum = snapshot.eventChecksum;
    converted.actors.reserve(snapshot.actors.size());
    converted.loot.reserve(snapshot.loot.size());

    for (const dxa::simulation::ActorSnapshot& actor : snapshot.actors)
    {
        converted.actors.push_back(dxa::protocol::NetworkActorSnapshot{
            dxa::protocol::EntityId{actor.id},
            ToNetwork(actor.role),
            ToNetwork(actor.neutralArchetype),
            {actor.position.x, actor.position.z},
            actor.health,
            actor.alive,
            ToNetwork(actor.weapon),
            actor.cooldownTicksRemaining,
            actor.eliminations});
    }
    for (const dxa::simulation::LootSnapshot& loot : snapshot.loot)
    {
        converted.loot.push_back(dxa::protocol::NetworkLootSnapshot{
            loot.id,
            ToNetwork(loot.type),
            {loot.position.x, loot.position.z},
            loot.active});
    }
    if (snapshot.result.has_value())
    {
        converted.hasResult = true;
        converted.result = dxa::protocol::NetworkMatchResult{
            dxa::protocol::EntityId{snapshot.result->winner},
            ToNetwork(snapshot.result->reason),
            snapshot.result->finishedTick};
    }
    return converted;
}
} // namespace dxa::game_common
