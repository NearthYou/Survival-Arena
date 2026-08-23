#pragma once

#include <dxa/simulation/MatchTypes.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace dxa::simulation
{
struct WeaponDefinition
{
    float range = 0.0F;
    float effectRadius = 0.0F;
    std::int32_t damage = 0;
    std::uint32_t cooldownTicks = 0;

    [[nodiscard]] bool operator==(const WeaponDefinition&) const = default;
};

struct CombatActor
{
    ActorId id = 0;
    ActorRole role = ActorRole::Contender;
    Vec2 position;
    std::int32_t health = 100;
    bool alive = true;
    WeaponType weapon = WeaponType::Blade;
    std::uint32_t cooldownTicksRemaining = 0;
    std::uint32_t eliminations = 0;

    [[nodiscard]] bool operator==(const CombatActor&) const = default;
};

struct AttackIntent
{
    ActorId attacker = 0;
    ActorId target = 0;

    [[nodiscard]] bool operator==(const AttackIntent&) const = default;
};

struct DamageRecord
{
    ActorId target = 0;
    std::int32_t amount = 0;
    std::optional<ActorId> primarySource;

    [[nodiscard]] bool operator==(const DamageRecord&) const = default;
};

struct DeathRecord
{
    ActorId victim = 0;
    std::optional<ActorId> killer;

    [[nodiscard]] bool operator==(const DeathRecord&) const = default;
};

struct CombatResolution
{
    std::vector<AttackIntent> acceptedIntents;
    std::vector<DamageRecord> damage;
    std::vector<DeathRecord> deaths;

    [[nodiscard]] bool operator==(const CombatResolution&) const = default;
};

[[nodiscard]] WeaponDefinition WeaponDefinitionFor(WeaponType weapon);
void TickWeaponCooldowns(std::span<CombatActor> actors);
[[nodiscard]] CombatResolution ResolveAttacks(
    std::span<CombatActor> actors,
    std::span<const AttackIntent> intents);
} // namespace dxa::simulation
