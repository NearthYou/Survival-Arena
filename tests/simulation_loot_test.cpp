#include <dxa/simulation/Loot.hpp>

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <span>
#include <stdexcept>

namespace
{
using dxa::simulation::ActorRole;
using dxa::simulation::CombatActor;
using dxa::simulation::LootItem;
using dxa::simulation::LootType;
using dxa::simulation::ResolveNearestLootPickup;
using dxa::simulation::Vec2;
using dxa::simulation::WeaponType;

[[nodiscard]] CombatActor ContenderAt(
    const Vec2 position,
    const int health = 100,
    const WeaponType weapon = WeaponType::Blade)
{
    return CombatActor{
        0U,
        ActorRole::Contender,
        position,
        health,
        true,
        weapon};
}

TEST(Loot, ChoosesLowestIdInsidePickupRadiusRegardlessOfInputOrder)
{
    CombatActor actor = ContenderAt({0.0F, 0.0F});
    std::array loot{
        LootItem{8U, LootType::Rifle, {0.2F, 0.0F}, true},
        LootItem{3U, LootType::ArcPulse, {0.8F, 0.0F}, true}};

    const auto result = ResolveNearestLootPickup(actor, loot, 1.0F);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(3U, result->loot);
    EXPECT_EQ(LootType::ArcPulse, result->type);
    ASSERT_TRUE(result->equippedWeapon.has_value());
    EXPECT_EQ(WeaponType::ArcPulse, *result->equippedWeapon);
    EXPECT_EQ(WeaponType::ArcPulse, actor.weapon);
    EXPECT_TRUE(loot[0].active);
    EXPECT_FALSE(loot[1].active);
}

TEST(Loot, IncludesPickupRadiusBoundaryAndRejectsOutsideItem)
{
    CombatActor boundaryActor = ContenderAt({0.0F, 0.0F});
    std::array boundaryLoot{
        LootItem{1U, LootType::Rifle, {1.0F, 0.0F}, true}};
    EXPECT_TRUE(ResolveNearestLootPickup(boundaryActor, boundaryLoot, 1.0F)
                    .has_value());

    CombatActor outsideActor = ContenderAt({0.0F, 0.0F});
    std::array outsideLoot{
        LootItem{1U, LootType::Rifle, {1.01F, 0.0F}, true}};
    EXPECT_FALSE(ResolveNearestLootPickup(outsideActor, outsideLoot, 1.0F)
                     .has_value());
    EXPECT_TRUE(outsideLoot[0].active);
    EXPECT_EQ(WeaponType::Blade, outsideActor.weapon);
}

TEST(Loot, ConsumedItemCannotBePickedUpAgain)
{
    CombatActor first = ContenderAt({0.0F, 0.0F});
    CombatActor second = ContenderAt({0.0F, 0.0F});
    second.id = 1U;
    std::array loot{
        LootItem{4U, LootType::Rifle, {0.0F, 0.0F}, true}};

    EXPECT_TRUE(ResolveNearestLootPickup(first, loot, 1.0F).has_value());
    EXPECT_FALSE(ResolveNearestLootPickup(second, loot, 1.0F).has_value());
    EXPECT_FALSE(loot[0].active);
    EXPECT_EQ(WeaponType::Blade, second.weapon);
}

TEST(Loot, DeadAndNeutralActorsDoNotConsumeItems)
{
    CombatActor dead = ContenderAt({0.0F, 0.0F});
    dead.alive = false;
    dead.health = 0;
    std::array deadLoot{
        LootItem{1U, LootType::Rifle, {0.0F, 0.0F}, true}};
    EXPECT_FALSE(ResolveNearestLootPickup(dead, deadLoot, 1.0F).has_value());
    EXPECT_TRUE(deadLoot[0].active);

    CombatActor neutral = ContenderAt({0.0F, 0.0F});
    neutral.role = ActorRole::Neutral;
    std::array neutralLoot{
        LootItem{2U, LootType::ArcPulse, {0.0F, 0.0F}, true}};
    EXPECT_FALSE(ResolveNearestLootPickup(neutral, neutralLoot, 1.0F).has_value());
    EXPECT_TRUE(neutralLoot[0].active);
}

TEST(Loot, RifleAndArcPulseReplaceCurrentWeapon)
{
    CombatActor rifleActor = ContenderAt({0.0F, 0.0F}, 100, WeaponType::ArcPulse);
    std::array rifleLoot{
        LootItem{1U, LootType::Rifle, {0.0F, 0.0F}, true}};
    const auto rifleResult = ResolveNearestLootPickup(rifleActor, rifleLoot, 1.0F);
    ASSERT_TRUE(rifleResult.has_value());
    EXPECT_EQ(WeaponType::Rifle, rifleActor.weapon);
    EXPECT_EQ(WeaponType::Rifle, rifleResult->equippedWeapon);

    CombatActor pulseActor = ContenderAt({0.0F, 0.0F}, 100, WeaponType::Rifle);
    std::array pulseLoot{
        LootItem{2U, LootType::ArcPulse, {0.0F, 0.0F}, true}};
    const auto pulseResult = ResolveNearestLootPickup(pulseActor, pulseLoot, 1.0F);
    ASSERT_TRUE(pulseResult.has_value());
    EXPECT_EQ(WeaponType::ArcPulse, pulseActor.weapon);
    EXPECT_EQ(WeaponType::ArcPulse, pulseResult->equippedWeapon);
}

TEST(Loot, MedKitHealsThirtyFiveWithoutExceedingOneHundred)
{
    CombatActor damaged = ContenderAt({0.0F, 0.0F}, 40);
    std::array firstKit{
        LootItem{1U, LootType::MedKit, {0.0F, 0.0F}, true}};
    const auto firstResult = ResolveNearestLootPickup(damaged, firstKit, 1.0F);
    ASSERT_TRUE(firstResult.has_value());
    EXPECT_EQ(75, damaged.health);
    EXPECT_EQ(35, firstResult->healedAmount);
    EXPECT_FALSE(firstResult->equippedWeapon.has_value());

    CombatActor nearlyFull = ContenderAt({0.0F, 0.0F}, 90);
    std::array secondKit{
        LootItem{2U, LootType::MedKit, {0.0F, 0.0F}, true}};
    const auto secondResult = ResolveNearestLootPickup(nearlyFull, secondKit, 1.0F);
    ASSERT_TRUE(secondResult.has_value());
    EXPECT_EQ(100, nearlyFull.health);
    EXPECT_EQ(10, secondResult->healedAmount);
}

TEST(Loot, FullHealthActorConsumesMedKitWithoutOverhealing)
{
    CombatActor actor = ContenderAt({0.0F, 0.0F});
    std::array loot{
        LootItem{1U, LootType::MedKit, {0.0F, 0.0F}, true}};

    const auto result = ResolveNearestLootPickup(actor, loot, 1.0F);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(0, result->healedAmount);
    EXPECT_EQ(100, actor.health);
    EXPECT_FALSE(loot[0].active);
}

TEST(Loot, RejectsInvalidPickupRadius)
{
    CombatActor actor = ContenderAt({0.0F, 0.0F});
    std::array loot{
        LootItem{1U, LootType::Rifle, {0.0F, 0.0F}, true}};

    EXPECT_THROW(
        (void)ResolveNearestLootPickup(actor, loot, 0.0F),
        std::invalid_argument);
    EXPECT_THROW(
        (void)ResolveNearestLootPickup(
            actor,
            loot,
            std::numeric_limits<float>::quiet_NaN()),
        std::invalid_argument);
}

TEST(Loot, RejectsInvalidActorAndLootState)
{
    CombatActor invalidActor = ContenderAt(
        {std::numeric_limits<float>::infinity(), 0.0F});
    std::array validLoot{
        LootItem{1U, LootType::Rifle, {0.0F, 0.0F}, true}};
    EXPECT_THROW(
        (void)ResolveNearestLootPickup(invalidActor, validLoot, 1.0F),
        std::invalid_argument);

    CombatActor actor = ContenderAt({0.0F, 0.0F});
    std::array duplicateLoot{
        LootItem{2U, LootType::Rifle, {0.0F, 0.0F}, true},
        LootItem{2U, LootType::ArcPulse, {0.5F, 0.0F}, true}};
    EXPECT_THROW(
        (void)ResolveNearestLootPickup(actor, duplicateLoot, 1.0F),
        std::invalid_argument);

    std::array nonFiniteLoot{
        LootItem{3U, LootType::Rifle, {0.0F, std::numeric_limits<float>::infinity()}, true}};
    EXPECT_THROW(
        (void)ResolveNearestLootPickup(actor, nonFiniteLoot, 1.0F),
        std::invalid_argument);

    std::array unknownLoot{
        LootItem{4U, static_cast<LootType>(99), {0.0F, 0.0F}, true}};
    EXPECT_THROW(
        (void)ResolveNearestLootPickup(actor, unknownLoot, 1.0F),
        std::invalid_argument);
}
} // namespace
