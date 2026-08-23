#include <dxa/simulation/SpatialIndex.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
using dxa::simulation::Aabb2;
using dxa::simulation::LinearSpatialIndex;
using dxa::simulation::LooseQuadtree;
using dxa::simulation::LooseQuadtreeConfig;
using dxa::simulation::SpatialEntity;
using dxa::simulation::Vec2;

[[nodiscard]] std::vector<SpatialEntity> GenerateSpatialEntities(
    const std::uint32_t count,
    const std::uint32_t seed)
{
    std::mt19937 random{seed};
    std::uniform_real_distribution<float> coordinate{-60.0F, 60.0F};
    std::uniform_real_distribution<float> halfExtent{0.05F, 1.5F};
    std::vector<SpatialEntity> entities;
    entities.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const Vec2 center{coordinate(random), coordinate(random)};
        const float halfX = halfExtent(random);
        const float halfZ = halfExtent(random);
        entities.push_back(SpatialEntity{
            index + 1U,
            Aabb2::Create(
                {center.x - halfX, center.z - halfZ},
                {center.x + halfX, center.z + halfZ})});
    }
    return entities;
}

[[nodiscard]] std::vector<Aabb2> GenerateAabbQueries(
    const std::uint32_t count,
    const std::uint32_t seed)
{
    std::mt19937 random{seed};
    std::uniform_real_distribution<float> coordinate{-70.0F, 70.0F};
    std::uniform_real_distribution<float> halfExtent{0.1F, 4.0F};
    std::vector<Aabb2> queries;
    queries.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const Vec2 center{coordinate(random), coordinate(random)};
        const float halfX = halfExtent(random);
        const float halfZ = halfExtent(random);
        queries.push_back(Aabb2::Create(
            {center.x - halfX, center.z - halfZ},
            {center.x + halfX, center.z + halfZ}));
    }
    return queries;
}

[[nodiscard]] std::vector<Vec2> GeneratePointQueries(
    const std::uint32_t count,
    const std::uint32_t seed)
{
    std::mt19937 random{seed};
    std::uniform_real_distribution<float> coordinate{-70.0F, 70.0F};
    std::vector<Vec2> queries;
    queries.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        queries.push_back({coordinate(random), coordinate(random)});
    }
    return queries;
}

TEST(SpatialIndex, LooseQuadtreeMatchesLinearForSeededQueries)
{
    const std::vector<SpatialEntity> entities = GenerateSpatialEntities(
        1124U,
        20260823U);
    const LinearSpatialIndex linear{entities};
    const LooseQuadtree tree{
        Aabb2::Create({-64.0F, -64.0F}, {64.0F, 64.0F}),
        entities,
        LooseQuadtreeConfig{1.5F, 8U, 6U}};
    std::uint64_t linearBoundsTested = 0;
    std::uint64_t treeBoundsTested = 0;

    const auto aabbQueries = GenerateAabbQueries(20000U, 20260823U);
    for (std::size_t index = 0; index < aabbQueries.size(); ++index)
    {
        const auto linearResult = linear.QueryAabb(aabbQueries[index]);
        const auto treeResult = tree.QueryAabb(aabbQueries[index]);
        ASSERT_EQ(linearResult.ids, treeResult.ids) << "AABB query " << index;
        linearBoundsTested += linearResult.boundsTested;
        treeBoundsTested += treeResult.boundsTested;
    }

    const auto pointQueries = GeneratePointQueries(20000U, 20260823U);
    for (std::size_t index = 0; index < pointQueries.size(); ++index)
    {
        const auto linearResult = linear.PickPoint(pointQueries[index]);
        const auto treeResult = tree.PickPoint(pointQueries[index]);
        ASSERT_EQ(linearResult.ids, treeResult.ids) << "point query " << index;
        linearBoundsTested += linearResult.boundsTested;
        treeBoundsTested += treeResult.boundsTested;
    }

    EXPECT_LT(treeBoundsTested, linearBoundsTested);
}

TEST(SpatialIndex, KeepsBoundaryAndOversizedEntitiesQueryable)
{
    const std::vector<SpatialEntity> entities{
        {10U, Aabb2::Create({-0.25F, -0.25F}, {0.25F, 0.25F})},
        {20U, Aabb2::Create({0.0F, 2.0F}, {0.25F, 2.25F})},
        {30U, Aabb2::Create({-5.0F, -5.0F}, {5.0F, 5.0F})},
        {40U, Aabb2::Create({6.0F, 6.0F}, {7.0F, 7.0F})}
    };
    const LinearSpatialIndex linear{entities};
    const LooseQuadtree tree{
        Aabb2::Create({-8.0F, -8.0F}, {8.0F, 8.0F}),
        entities,
        LooseQuadtreeConfig{1.5F, 1U, 4U}};
    const std::vector<Aabb2> queries{
        Aabb2::Create({-0.1F, -0.1F}, {0.1F, 0.1F}),
        Aabb2::Create({0.0F, 2.0F}, {0.0F, 2.0F}),
        Aabb2::Create({6.5F, 6.5F}, {6.5F, 6.5F}),
        Aabb2::Create({-8.0F, -8.0F}, {8.0F, 8.0F})
    };

    for (const Aabb2& query : queries)
    {
        EXPECT_EQ(linear.QueryAabb(query).ids, tree.QueryAabb(query).ids);
    }
    EXPECT_EQ(linear.PickPoint({0.0F, 0.0F}).ids, tree.PickPoint({0.0F, 0.0F}).ids);
}

TEST(SpatialIndex, RejectsInvalidConstructionAndPointQueries)
{
    const Aabb2 world = Aabb2::Create({-8.0F, -8.0F}, {8.0F, 8.0F});
    const std::vector<SpatialEntity> duplicateIds{
        {1U, Aabb2::Create({0.0F, 0.0F}, {1.0F, 1.0F})},
        {1U, Aabb2::Create({2.0F, 2.0F}, {3.0F, 3.0F})}
    };
    EXPECT_THROW((void)(LinearSpatialIndex{duplicateIds}), std::invalid_argument);
    EXPECT_THROW(
        (void)(LooseQuadtree{world, duplicateIds, {}}),
        std::invalid_argument);

    const std::vector<SpatialEntity> outsideWorld{
        {1U, Aabb2::Create({7.5F, 7.5F}, {8.5F, 8.5F})}
    };
    EXPECT_THROW(
        (void)(LooseQuadtree{world, outsideWorld, {}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)(LooseQuadtree{
            world,
            {},
            LooseQuadtreeConfig{0.99F, 8U, 6U}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)(LooseQuadtree{
            world,
            {},
            LooseQuadtreeConfig{1.5F, 0U, 6U}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)(LooseQuadtree{
            world,
            {},
            LooseQuadtreeConfig{1.5F, 8U, 0U}}),
        std::invalid_argument);

    const LinearSpatialIndex linear{std::vector<SpatialEntity>{}};
    const LooseQuadtree tree{world, {}, {}};
    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW((void)linear.PickPoint({notANumber, 0.0F}), std::invalid_argument);
    EXPECT_THROW((void)tree.PickPoint({0.0F, notANumber}), std::invalid_argument);
}
} // namespace
