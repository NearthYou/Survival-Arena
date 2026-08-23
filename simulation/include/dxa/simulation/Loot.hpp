#pragma once

#include <dxa/simulation/Combat.hpp>

#include <cstdint>
#include <optional>
#include <span>

namespace dxa::simulation
{
struct LootItem
{
    LootId id = 0;
    LootType type = LootType::Rifle;
    Vec2 position;
    bool active = true;

    [[nodiscard]] bool operator==(const LootItem&) const = default;
};

struct LootPickupResult
{
    ActorId actor = 0;
    LootId loot = 0;
    LootType type = LootType::Rifle;
    std::optional<WeaponType> equippedWeapon;
    std::int32_t healedAmount = 0;

    [[nodiscard]] bool operator==(const LootPickupResult&) const = default;
};

[[nodiscard]] std::optional<LootPickupResult> ResolveNearestLootPickup(
    CombatActor& actor,
    std::span<LootItem> loot,
    float pickupRadius);
} // namespace dxa::simulation
