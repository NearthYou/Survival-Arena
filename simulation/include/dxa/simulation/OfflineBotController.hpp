#pragma once

#include <dxa/simulation/AiDecision.hpp>
#include <dxa/simulation/MatchConfig.hpp>
#include <dxa/simulation/MatchTypes.hpp>
#include <dxa/simulation/SafeZone.hpp>

#include <span>

namespace dxa::simulation
{
enum class BotDecisionReason
{
    ReturnToZone,
    UseMedKit,
    SeekWeapon,
    Attack,
    Chase,
    Retreat,
    Idle
};

struct BotPerception
{
    std::span<const ActorSnapshot> actors;
    std::span<const LootSnapshot> loot;
    SafeZoneState safeZone;
};

struct BotDecision
{
    MatchCommand command;
    BotDecisionReason reason = BotDecisionReason::Idle;

    [[nodiscard]] bool operator==(const BotDecision&) const = default;
};

[[nodiscard]] BotDecision DecideContender(
    const ActorSnapshot& self,
    const BotPerception& perception,
    const MatchConfig& config);

[[nodiscard]] BotDecision DecideNeutral(
    const ActorSnapshot& self,
    const BotPerception& perception,
    const MatchConfig& config,
    const BehaviorTreeAiController& controller);
} // namespace dxa::simulation
