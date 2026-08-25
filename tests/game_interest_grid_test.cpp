#include <dxa/game_server/InterestGrid.hpp>

#include <dxa/protocol/GameSnapshot.hpp>
#include <dxa/simulation/Math2.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace
{
using dxa::game_server::InterestGrid;
using dxa::game_server::VisibleSet;
using namespace dxa::protocol;
using dxa::simulation::Aabb2;

[[nodiscard]] Aabb2 ArenaBounds()
{
    return Aabb2::Create({-128.0F, -128.0F}, {128.0F, 128.0F});
}

[[nodiscard]] bool Contains(
    const std::vector<EntityId>& ids,
    const EntityId id)
{
    return std::binary_search(ids.begin(), ids.end(), id);
}

[[nodiscard]] bool Contains(
    const std::vector<std::uint32_t>& ids,
    const std::uint32_t id)
{
    return std::binary_search(ids.begin(), ids.end(), id);
}

[[nodiscard]] float DistanceSquared(
    const NetworkVec2 left,
    const NetworkVec2 right)
{
    const float x = left.x - right.x;
    const float z = left.z - right.z;
    return x * x + z * z;
}

[[nodiscard]] VisibleSet BruteForceUpdate(
    const GameSnapshot& world,
    const VisibleSet& previous,
    const NetworkVec2 center,
    const float enterRadius,
    const float leaveRadius)
{
    VisibleSet result;
    for (const NetworkActorSnapshot& actor : world.actors)
    {
        const float radius = Contains(previous.actors, actor.id)
            ? leaveRadius
            : enterRadius;
        if (DistanceSquared(actor.position, center) <= radius * radius)
        {
            result.actors.push_back(actor.id);
        }
    }
    for (const NetworkLootSnapshot& loot : world.loot)
    {
        if (!loot.active)
        {
            continue;
        }
        const float radius = Contains(previous.loot, loot.id)
            ? leaveRadius
            : enterRadius;
        if (DistanceSquared(loot.position, center) <= radius * radius)
        {
            result.loot.push_back(loot.id);
        }
    }
    std::sort(result.actors.begin(), result.actors.end());
    std::sort(result.loot.begin(), result.loot.end());
    return result;
}

[[nodiscard]] GameSnapshot SeededWorld(const std::uint32_t seed)
{
    std::mt19937 random{seed};
    std::uniform_real_distribution<float> coordinate{-128.0F, 128.0F};

    GameSnapshot world;
    world.actors.reserve(MaxSnapshotActors);
    for (std::uint32_t id = 0U;
         id < static_cast<std::uint32_t>(MaxSnapshotActors);
         ++id)
    {
        const bool contender = id < 24U;
        world.actors.push_back(NetworkActorSnapshot{
            EntityId{id},
            contender
                ? NetworkActorRole::Contender
                : NetworkActorRole::Neutral,
            contender
                ? NetworkNeutralArchetype::None
                : NetworkNeutralArchetype::Melee,
            {coordinate(random), coordinate(random)},
            100,
            true,
            NetworkWeaponType::Blade,
            0U,
            0U});
    }

    world.loot.reserve(MaxSnapshotLoot);
    for (std::uint32_t id = 0U;
         id < static_cast<std::uint32_t>(MaxSnapshotLoot);
         ++id)
    {
        world.loot.push_back(NetworkLootSnapshot{
            id,
            NetworkLootType::Rifle,
            {coordinate(random), coordinate(random)},
            id % 4U != 0U});
    }
    return world;
}

[[nodiscard]] NetworkActorSnapshot ActorAt(
    const std::uint32_t id,
    const float distance)
{
    return NetworkActorSnapshot{
        EntityId{id},
        NetworkActorRole::Contender,
        NetworkNeutralArchetype::None,
        {distance, 0.0F},
        100,
        true,
        NetworkWeaponType::Blade,
        0U,
        0U};
}

TEST(InterestGrid, MatchesBruteForceForSeededQueries)
{
    const GameSnapshot world = SeededWorld(20260825U);
    InterestGrid grid{ArenaBounds(), 32.0F};
    grid.Rebuild(world);
    const std::vector<NetworkVec2> recipients{
        {-127.0F, -127.0F},
        {-64.0F, 0.0F},
        {0.0F, 0.0F},
        {63.5F, -17.25F},
        {127.0F, 127.0F}};

    for (const NetworkVec2 recipient : recipients)
    {
        const VisibleSet actual =
            grid.UpdateVisibility({}, recipient, 80.0F, 88.0F);
        const VisibleSet expected = BruteForceUpdate(
            world,
            {},
            recipient,
            80.0F,
            88.0F);
        EXPECT_EQ(expected, actual);
    }

    const VisibleSet previous = grid.UpdateVisibility(
        {},
        {-40.0F, -40.0F},
        80.0F,
        88.0F);
    EXPECT_EQ(
        BruteForceUpdate(
            world,
            previous,
            {40.0F, 40.0F},
            80.0F,
            88.0F),
        grid.UpdateVisibility(
            previous,
            {40.0F, 40.0F},
            80.0F,
            88.0F));
}

TEST(InterestGrid, UsesEightyEnterAndEightyEightLeaveRadius)
{
    InterestGrid grid{ArenaBounds(), 32.0F};
    GameSnapshot enteredWorld;
    enteredWorld.actors = {
        ActorAt(1U, 79.99F),
        ActorAt(2U, 79.99F),
        ActorAt(3U, 80.01F)};
    grid.Rebuild(enteredWorld);

    const VisibleSet entered =
        grid.UpdateVisibility({}, {}, 80.0F, 88.0F);
    EXPECT_TRUE(Contains(entered.actors, EntityId{1U}));
    EXPECT_TRUE(Contains(entered.actors, EntityId{2U}));
    EXPECT_FALSE(Contains(entered.actors, EntityId{3U}));

    GameSnapshot movedWorld;
    movedWorld.actors = {
        ActorAt(1U, 87.99F),
        ActorAt(2U, 88.01F),
        ActorAt(3U, 79.99F)};
    grid.Rebuild(movedWorld);
    const VisibleSet retained =
        grid.UpdateVisibility(entered, {}, 80.0F, 88.0F);

    EXPECT_TRUE(Contains(retained.actors, EntityId{1U}));
    EXPECT_FALSE(Contains(retained.actors, EntityId{2U}));
    EXPECT_TRUE(Contains(retained.actors, EntityId{3U}));
}

TEST(InterestGrid, ExcludesInactiveLootAndReturnsSortedIds)
{
    InterestGrid grid{ArenaBounds(), 32.0F};
    GameSnapshot world;
    world.actors = {ActorAt(9U, 1.0F), ActorAt(2U, 2.0F)};
    world.loot = {
        NetworkLootSnapshot{8U, NetworkLootType::Rifle, {1.0F, 0.0F}, true},
        NetworkLootSnapshot{3U, NetworkLootType::MedKit, {2.0F, 0.0F}, true},
        NetworkLootSnapshot{1U, NetworkLootType::Rifle, {0.0F, 0.0F}, false}};
    grid.Rebuild(world);

    const VisibleSet visible =
        grid.UpdateVisibility({}, {}, 80.0F, 88.0F);
    EXPECT_EQ(
        (std::vector<EntityId>{EntityId{2U}, EntityId{9U}}),
        visible.actors);
    EXPECT_EQ((std::vector<std::uint32_t>{3U, 8U}), visible.loot);
}

TEST(InterestGrid, RejectsInvalidConfigurationWorldAndQuery)
{
    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(
        (void)InterestGrid(ArenaBounds(), 0.0F),
        std::invalid_argument);
    EXPECT_THROW(
        (void)InterestGrid(ArenaBounds(), notANumber),
        std::invalid_argument);

    InterestGrid grid{ArenaBounds(), 32.0F};
    GameSnapshot duplicateActors;
    duplicateActors.actors = {ActorAt(1U, 0.0F), ActorAt(1U, 1.0F)};
    EXPECT_THROW(grid.Rebuild(duplicateActors), std::invalid_argument);

    GameSnapshot duplicateLoot;
    duplicateLoot.loot = {
        NetworkLootSnapshot{1U, NetworkLootType::Rifle, {}, true},
        NetworkLootSnapshot{1U, NetworkLootType::MedKit, {}, true}};
    EXPECT_THROW(grid.Rebuild(duplicateLoot), std::invalid_argument);

    GameSnapshot outside;
    outside.actors = {ActorAt(1U, 128.01F)};
    EXPECT_THROW(grid.Rebuild(outside), std::out_of_range);

    GameSnapshot nonFinite;
    nonFinite.loot = {
        NetworkLootSnapshot{
            1U,
            NetworkLootType::Rifle,
            {notANumber, 0.0F},
            true}};
    EXPECT_THROW(grid.Rebuild(nonFinite), std::invalid_argument);

    EXPECT_THROW(
        (void)grid.UpdateVisibility({}, {}, 0.0F, 88.0F),
        std::invalid_argument);
    EXPECT_THROW(
        (void)grid.UpdateVisibility({}, {}, 80.0F, 79.0F),
        std::invalid_argument);
    EXPECT_THROW(
        (void)grid.UpdateVisibility(
            {},
            {notANumber, 0.0F},
            80.0F,
            88.0F),
        std::invalid_argument);
}
} // namespace
