#include <dxa/game_common/ArenaFingerprint.hpp>
#include <dxa/simulation/ArenaMap.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
using dxa::game_common::SurvivalArenaFingerprint;
using dxa::simulation::ArenaMapDefinition;
using dxa::simulation::BuildSurvivalArenaNavMesh;
using dxa::simulation::NavTriangleIndices;
using dxa::simulation::SurvivalArenaMapDefinition;
using dxa::simulation::Vec2;
} // namespace

TEST(ArenaMap, BuildsMapOneWithStableSourceAndCoverage)
{
    const ArenaMapDefinition definition = SurvivalArenaMapDefinition();

    EXPECT_EQ(1U, definition.mapId);
    EXPECT_EQ(
        (std::vector<Vec2>{
            {-128.0F, -128.0F},
            {128.0F, -128.0F},
            {-128.0F, 128.0F},
            {128.0F, 128.0F}}),
        definition.vertices);
    ASSERT_EQ(2U, definition.triangles.size());
    EXPECT_EQ(
        (std::array<std::uint32_t, 3U>{0U, 1U, 2U}),
        definition.triangles[0].vertices);
    EXPECT_EQ(
        (std::array<std::uint32_t, 3U>{1U, 3U, 2U}),
        definition.triangles[1].vertices);
    EXPECT_FLOAT_EQ(4.0F, definition.gridCellSize);

    const auto mesh = BuildSurvivalArenaNavMesh();
    EXPECT_TRUE(mesh.FindContainingTriangleGrid({0.0F, 0.0F})
                    .triangle.has_value());
    EXPECT_TRUE(mesh.FindContainingTriangleGrid({128.0F, 128.0F})
                    .triangle.has_value());
    EXPECT_FALSE(mesh.FindContainingTriangleGrid({129.0F, 0.0F})
                     .triangle.has_value());
}

TEST(ArenaMap, RepeatedBuildsReturnTheSameQueryResults)
{
    const auto first = BuildSurvivalArenaNavMesh();
    const auto second = BuildSurvivalArenaNavMesh();
    const std::array<Vec2, 7U> queries{
        Vec2{-128.0F, -128.0F},
        Vec2{-64.0F, 48.0F},
        Vec2{0.0F, 0.0F},
        Vec2{64.0F, -48.0F},
        Vec2{128.0F, 128.0F},
        Vec2{-129.0F, 0.0F},
        Vec2{0.0F, 129.0F}};

    for (const Vec2 query : queries)
    {
        EXPECT_EQ(
            first.FindContainingTriangleGrid(query).triangle,
            second.FindContainingTriangleGrid(query).triangle);
    }
}

TEST(ArenaFingerprint, RepeatsForTheCanonicalDefinition)
{
    EXPECT_EQ(
        SurvivalArenaFingerprint(SurvivalArenaMapDefinition()),
        SurvivalArenaFingerprint(SurvivalArenaMapDefinition()));
}

TEST(ArenaFingerprint, ChangesWhenAnyCanonicalSourceSectionChanges)
{
    const ArenaMapDefinition source = SurvivalArenaMapDefinition();
    const std::uint32_t canonical = SurvivalArenaFingerprint(source);

    ArenaMapDefinition changedMap = source;
    ++changedMap.mapId;
    EXPECT_NE(canonical, SurvivalArenaFingerprint(changedMap));

    ArenaMapDefinition changedVertex = source;
    changedVertex.vertices[0].x += 1.0F;
    EXPECT_NE(canonical, SurvivalArenaFingerprint(changedVertex));

    ArenaMapDefinition changedTriangle = source;
    changedTriangle.triangles[0] = NavTriangleIndices{{0U, 2U, 1U}};
    EXPECT_NE(canonical, SurvivalArenaFingerprint(changedTriangle));

    ArenaMapDefinition changedGrid = source;
    changedGrid.gridCellSize = 8.0F;
    EXPECT_NE(canonical, SurvivalArenaFingerprint(changedGrid));
}
