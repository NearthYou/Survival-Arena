#pragma once

#include <dxa/simulation/Math2.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace dxa::simulation
{
using ActorId = std::uint32_t;
using LootId = std::uint32_t;

enum class ActorRole
{
    Contender,
    Neutral
};

enum class NeutralArchetype
{
    None,
    Melee,
    Ranged
};

enum class WeaponType
{
    Blade,
    Rifle,
    ArcPulse
};

enum class LootType
{
    Rifle,
    ArcPulse,
    MedKit
};

enum class MatchPhase
{
    Waiting,
    Running,
    SuddenDeath,
    Finished
};

enum class SafeZoneStage
{
    Stage1,
    Stage2,
    Stage3,
    Stage4,
    SuddenDeath
};

enum class MatchEndReason
{
    LastSurvivor,
    TimeLimit
};

enum class MatchEventType
{
    CommandRejected,
    LootPickedUp,
    WeaponChanged,
    ActorHealed,
    DamageApplied,
    ActorDied,
    MatchFinished
};

struct MatchCommand
{
    ActorId actor = 0;
    std::optional<Vec2> moveDestination;
    std::optional<ActorId> attackTarget;

    [[nodiscard]] bool operator==(const MatchCommand&) const = default;
};

struct MatchEvent
{
    std::uint32_t tick = 0;
    MatchEventType type = MatchEventType::CommandRejected;
    ActorId actor = 0;
    std::optional<ActorId> subject;
    std::optional<LootId> loot;
    std::int32_t amount = 0;
    std::optional<WeaponType> weapon;

    [[nodiscard]] bool operator==(const MatchEvent&) const = default;
};

struct ActorSnapshot
{
    ActorId id = 0;
    ActorRole role = ActorRole::Contender;
    NeutralArchetype neutralArchetype = NeutralArchetype::None;
    Vec2 position;
    std::int32_t health = 100;
    bool alive = true;
    WeaponType weapon = WeaponType::Blade;
    std::uint32_t cooldownTicksRemaining = 0;
    std::uint32_t eliminations = 0;

    [[nodiscard]] bool operator==(const ActorSnapshot&) const = default;
};

struct LootSnapshot
{
    LootId id = 0;
    LootType type = LootType::Rifle;
    Vec2 position;
    bool active = true;

    [[nodiscard]] bool operator==(const LootSnapshot&) const = default;
};

struct MatchResult
{
    ActorId winner = 0;
    MatchEndReason reason = MatchEndReason::LastSurvivor;
    std::uint32_t finishedTick = 0;

    [[nodiscard]] bool operator==(const MatchResult&) const = default;
};

struct MatchSnapshot
{
    std::uint32_t tick = 0;
    double elapsedSeconds = 0.0;
    MatchPhase phase = MatchPhase::Waiting;
    SafeZoneStage safeZoneStage = SafeZoneStage::Stage1;
    Vec2 safeZoneCenter;
    float safeZoneRadius = 32.0F;
    std::uint32_t aliveContenders = 0;
    std::vector<ActorSnapshot> actors;
    std::vector<LootSnapshot> loot;
    std::optional<MatchResult> result;
    std::uint64_t eventChecksum = 0;

    [[nodiscard]] bool operator==(const MatchSnapshot&) const = default;
};
} // namespace dxa::simulation
