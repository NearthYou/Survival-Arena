#include <dxa/simulation/Loot.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace dxa::simulation
{
namespace
{
void ValidateActor(const CombatActor& actor)
{
    switch (actor.role)
    {
    case ActorRole::Contender:
    case ActorRole::Neutral:
        break;
    default:
        throw std::invalid_argument{"loot actor role is invalid"};
    }
    (void)WeaponDefinitionFor(actor.weapon);
    if (!IsFinite(actor.position)
        || actor.health < 0
        || actor.health > 100
        || actor.alive != (actor.health > 0))
    {
        throw std::invalid_argument{"loot actor state is invalid"};
    }
}

void ValidateLootType(const LootType type)
{
    switch (type)
    {
    case LootType::Rifle:
    case LootType::ArcPulse:
    case LootType::MedKit:
        return;
    }
    throw std::invalid_argument{"loot type is invalid"};
}

void ValidateLoot(const std::span<const LootItem> loot)
{
    std::set<LootId> ids;
    for (const LootItem& item : loot)
    {
        ValidateLootType(item.type);
        if (!IsFinite(item.position))
        {
            throw std::invalid_argument{"loot position must be finite"};
        }
        if (!ids.insert(item.id).second)
        {
            throw std::invalid_argument{"loot IDs must be unique"};
        }
    }
}

[[nodiscard]] bool IsInsidePickupRadius(
    const Vec2 actorPosition,
    const Vec2 lootPosition,
    const float pickupRadius) noexcept
{
    const double deltaX = static_cast<double>(actorPosition.x)
        - static_cast<double>(lootPosition.x);
    const double deltaZ = static_cast<double>(actorPosition.z)
        - static_cast<double>(lootPosition.z);
    const double radius = pickupRadius;
    return deltaX * deltaX + deltaZ * deltaZ <= radius * radius;
}
} // namespace

std::optional<LootPickupResult> ResolveNearestLootPickup(
    CombatActor& actor,
    const std::span<LootItem> loot,
    const float pickupRadius)
{
    if (!std::isfinite(pickupRadius) || pickupRadius <= 0.0F)
    {
        throw std::invalid_argument{"loot pickup radius must be finite and positive"};
    }
    ValidateActor(actor);
    ValidateLoot(loot);
    if (!actor.alive || actor.role != ActorRole::Contender)
    {
        return std::nullopt;
    }

    LootItem* selected = nullptr;
    for (LootItem& item : loot)
    {
        if (item.active
            && IsInsidePickupRadius(actor.position, item.position, pickupRadius)
            && (selected == nullptr || item.id < selected->id))
        {
            selected = &item;
        }
    }
    if (selected == nullptr)
    {
        return std::nullopt;
    }

    LootPickupResult result;
    result.actor = actor.id;
    result.loot = selected->id;
    result.type = selected->type;
    selected->active = false;

    switch (selected->type)
    {
    case LootType::Rifle:
        actor.weapon = WeaponType::Rifle;
        result.equippedWeapon = actor.weapon;
        break;
    case LootType::ArcPulse:
        actor.weapon = WeaponType::ArcPulse;
        result.equippedWeapon = actor.weapon;
        break;
    case LootType::MedKit:
        result.healedAmount = std::min(35, 100 - actor.health);
        actor.health += result.healedAmount;
        break;
    }
    return result;
}
} // namespace dxa::simulation
