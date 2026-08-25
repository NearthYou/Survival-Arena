#pragma once

#include <dxa/protocol/GameUdpMessages.hpp>
#include <dxa/protocol/Ids.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dxa::protocol
{
inline constexpr std::size_t MaxSnapshotActors = 124U;
inline constexpr std::size_t MaxSnapshotLoot = 60U;

enum class NetworkActorRole : std::uint8_t
{
    Contender = 1,
    Neutral = 2
};

enum class NetworkNeutralArchetype : std::uint8_t
{
    None = 0,
    Melee = 1,
    Ranged = 2
};

enum class NetworkWeaponType : std::uint8_t
{
    Blade = 1,
    Rifle = 2,
    ArcPulse = 3
};

enum class NetworkLootType : std::uint8_t
{
    Rifle = 1,
    ArcPulse = 2,
    MedKit = 3
};

enum class NetworkMatchPhase : std::uint8_t
{
    Waiting = 1,
    Running = 2,
    SuddenDeath = 3,
    Finished = 4
};

enum class NetworkSafeZoneStage : std::uint8_t
{
    Stage1 = 1,
    Stage2 = 2,
    Stage3 = 3,
    Stage4 = 4,
    SuddenDeath = 5
};

enum class NetworkMatchEndReason : std::uint8_t
{
    LastSurvivor = 1,
    TimeLimit = 2
};

struct NetworkActorSnapshot
{
    EntityId id;
    NetworkActorRole role = NetworkActorRole::Contender;
    NetworkNeutralArchetype neutralArchetype = NetworkNeutralArchetype::None;
    NetworkVec2 position;
    std::int32_t health = 100;
    bool alive = true;
    NetworkWeaponType weapon = NetworkWeaponType::Blade;
    std::uint32_t cooldownTicksRemaining = 0U;
    std::uint32_t eliminations = 0U;

    [[nodiscard]] bool operator==(const NetworkActorSnapshot&) const = default;
};

struct NetworkLootSnapshot
{
    std::uint32_t id = 0U;
    NetworkLootType type = NetworkLootType::Rifle;
    NetworkVec2 position;
    bool active = true;

    [[nodiscard]] bool operator==(const NetworkLootSnapshot&) const = default;
};

struct NetworkMatchResult
{
    EntityId winner;
    NetworkMatchEndReason reason = NetworkMatchEndReason::LastSurvivor;
    std::uint32_t finishedTick = 0U;

    [[nodiscard]] bool operator==(const NetworkMatchResult&) const = default;
};

struct GameSnapshot
{
    NetworkMatchPhase phase = NetworkMatchPhase::Waiting;
    NetworkSafeZoneStage safeZoneStage = NetworkSafeZoneStage::Stage1;
    NetworkVec2 safeZoneCenter;
    float safeZoneRadius = 128.0F;
    std::uint32_t aliveContenders = 0U;
    std::vector<NetworkActorSnapshot> actors;
    std::vector<NetworkLootSnapshot> loot;
    NetworkMatchResult result;
    bool hasResult = false;
    std::uint64_t eventChecksum = 0U;

    [[nodiscard]] bool operator==(const GameSnapshot&) const = default;
};
} // namespace dxa::protocol
