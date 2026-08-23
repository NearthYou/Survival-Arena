#include <dxa/simulation/NavMesh.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
using dxa::simulation::NavMesh;
using dxa::simulation::NavQueryResult;
using dxa::simulation::NavTriangleIndices;
using dxa::simulation::TriangleId;
using dxa::simulation::Vec2;

[[nodiscard]] NavMesh MakeTwoTriangleSquare()
{
    return NavMesh::Build(
        {{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}, {1.0F, 1.0F}},
        {NavTriangleIndices{{0, 1, 2}}, NavTriangleIndices{{1, 3, 2}}});
}

[[nodiscard]] NavMesh MakeGridNavMesh(
    const std::uint32_t columns,
    const std::uint32_t rows,
    const float quadSize,
    const float queryCellSize,
    const Vec2 origin = {})
{
    std::vector<Vec2> vertices;
    vertices.reserve(
        static_cast<std::size_t>(columns + 1U)
        * static_cast<std::size_t>(rows + 1U));
    for (std::uint32_t row = 0; row <= rows; ++row)
    {
        for (std::uint32_t column = 0; column <= columns; ++column)
        {
            vertices.push_back({
                origin.x + static_cast<float>(column) * quadSize,
                origin.z + static_cast<float>(row) * quadSize});
        }
    }

    const auto vertexId = [columns](
                              const std::uint32_t column,
                              const std::uint32_t row) {
        return row * (columns + 1U) + column;
    };

    std::vector<NavTriangleIndices> triangles;
    triangles.reserve(
        static_cast<std::size_t>(columns)
        * static_cast<std::size_t>(rows)
        * 2U);
    for (std::uint32_t row = 0; row < rows; ++row)
    {
        for (std::uint32_t column = 0; column < columns; ++column)
        {
            const std::uint32_t lowerLeft = vertexId(column, row);
            const std::uint32_t lowerRight = vertexId(column + 1U, row);
            const std::uint32_t upperLeft = vertexId(column, row + 1U);
            const std::uint32_t upperRight = vertexId(column + 1U, row + 1U);
            triangles.push_back(NavTriangleIndices{{
                lowerLeft,
                lowerRight,
                upperLeft}});
            triangles.push_back(NavTriangleIndices{{
                lowerRight,
                upperRight,
                upperLeft}});
        }
    }

    return NavMesh::Build(
        std::move(vertices),
        std::move(triangles),
        queryCellSize);
}

[[nodiscard]] std::vector<TriangleId> CopyNeighbors(
    const NavMesh& mesh,
    const TriangleId triangle)
{
    const auto neighbors = mesh.Neighbors(triangle);
    return std::vector<TriangleId>{neighbors.begin(), neighbors.end()};
}

TEST(NavMesh, BuildsStableAdjacencyFromSharedVertexIndices)
{
    const NavMesh mesh = MakeTwoTriangleSquare();

    EXPECT_EQ((std::vector<TriangleId>{1}), CopyNeighbors(mesh, 0));
    EXPECT_EQ((std::vector<TriangleId>{0}), CopyNeighbors(mesh, 1));
    EXPECT_EQ((Vec2{1.0F / 3.0F, 1.0F / 3.0F}), mesh.TriangleCenter(0));
}

TEST(NavMesh, LinearQueryChoosesLowestTriangleOnSharedEdge)
{
    const NavMesh mesh = MakeTwoTriangleSquare();

    const NavQueryResult boundary = mesh.FindContainingTriangleLinear({0.5F, 0.5F});
    ASSERT_TRUE(boundary.triangle.has_value());
    EXPECT_EQ(0U, *boundary.triangle);
    EXPECT_EQ(2U, boundary.candidatesTested);

    const NavQueryResult outside = mesh.FindContainingTriangleLinear({2.0F, 2.0F});
    EXPECT_FALSE(outside.triangle.has_value());
    EXPECT_EQ(2U, outside.candidatesTested);
}

TEST(NavMesh, RejectsInvalidTrianglePayloads)
{
    const std::vector<Vec2> vertices{
        {0.0F, 0.0F},
        {1.0F, 0.0F},
        {0.0F, 1.0F},
        {1.0F, 1.0F},
        {0.5F, -1.0F}};

    EXPECT_THROW((void)NavMesh::Build({}, {}), std::invalid_argument);

    EXPECT_THROW(
        (void)NavMesh::Build(vertices, {NavTriangleIndices{{0, 1, 99}}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)NavMesh::Build(vertices, {NavTriangleIndices{{0, 0, 1}}}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)NavMesh::Build(
            {{0.0F, 0.0F}, {1.0F, 0.0F}, {2.0F, 0.0F}},
            {NavTriangleIndices{{0, 1, 2}}}),
        std::invalid_argument);

    auto nonFiniteVertices = vertices;
    nonFiniteVertices[0].x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(
        (void)NavMesh::Build(nonFiniteVertices, {NavTriangleIndices{{0, 1, 2}}}),
        std::invalid_argument);

    EXPECT_THROW(
        (void)NavMesh::Build(
            vertices,
            {
                NavTriangleIndices{{0, 1, 2}},
                NavTriangleIndices{{1, 0, 3}},
                NavTriangleIndices{{0, 1, 4}}
            }),
        std::invalid_argument);
    EXPECT_THROW(
        (void)NavMesh::Build(vertices, {NavTriangleIndices{{0, 1, 2}}}, 0.0F),
        std::invalid_argument);
    EXPECT_THROW(
        (void)NavMesh::Build(
            vertices,
            {NavTriangleIndices{{0, 1, 2}}},
            std::numeric_limits<float>::infinity()),
        std::invalid_argument);
}

TEST(NavMesh, RejectsNonFiniteLinearQuery)
{
    const NavMesh mesh = MakeTwoTriangleSquare();
    const float notANumber = std::numeric_limits<float>::quiet_NaN();

    EXPECT_THROW(
        (void)mesh.FindContainingTriangleLinear({notANumber, 0.0F}),
        std::invalid_argument);
}

TEST(NavMesh, GridMatchesLinearAtNegativeAndBoundaryCoordinates)
{
    const NavMesh mesh = MakeGridNavMesh(4U, 4U, 1.0F, 2.0F, {-4.0F, -4.0F});
    const std::vector<Vec2> points{
        {-3.75F, -3.75F},
        {-3.5F, -3.5F},
        {-2.0F, -2.5F},
        {-2.0F, -2.0F},
        {-0.001F, -0.001F},
        {-4.1F, -4.1F},
        {0.1F, 0.1F}};

    for (const Vec2 point : points)
    {
        const NavQueryResult linear = mesh.FindContainingTriangleLinear(point);
        const NavQueryResult grid = mesh.FindContainingTriangleGrid(point);
        EXPECT_EQ(linear.triangle, grid.triangle)
            << "point=(" << point.x << ", " << point.z << ')';
    }

    const NavQueryResult sharedEdge = mesh.FindContainingTriangleGrid(
        {-3.5F, -3.5F});
    ASSERT_TRUE(sharedEdge.triangle.has_value());
    EXPECT_EQ(0U, *sharedEdge.triangle);
}

TEST(NavMesh, GridMatchesLinearForSeededQueriesAndReducesCandidates)
{
    const NavMesh mesh = MakeGridNavMesh(16U, 16U, 1.0F, 4.0F);
    std::mt19937 random{20260823U};
    std::uniform_real_distribution<float> coordinate{-2.0F, 18.0F};
    std::vector<std::uint32_t> gridCandidates;
    gridCandidates.reserve(100000U);

    for (std::uint32_t index = 0; index < 100000U; ++index)
    {
        const Vec2 point{coordinate(random), coordinate(random)};
        const NavQueryResult linear = mesh.FindContainingTriangleLinear(point);
        const NavQueryResult grid = mesh.FindContainingTriangleGrid(point);
        if (linear.triangle != grid.triangle)
        {
            FAIL() << "query " << index << " differs at point=("
                   << point.x << ", " << point.z << ')';
        }
        gridCandidates.push_back(grid.candidatesTested);
    }

    const auto median = gridCandidates.begin()
        + static_cast<std::ptrdiff_t>(gridCandidates.size() / 2U);
    std::nth_element(gridCandidates.begin(), median, gridCandidates.end());
    EXPECT_LT(*median, mesh.TriangleCount());
}

TEST(NavMesh, RejectsNonFiniteGridQuery)
{
    const NavMesh mesh = MakeTwoTriangleSquare();
    const float infinity = std::numeric_limits<float>::infinity();

    EXPECT_THROW(
        (void)mesh.FindContainingTriangleGrid({0.0F, infinity}),
        std::invalid_argument);
}

TEST(NavMesh, GridCountsEveryCellCandidateItInspects)
{
    const NavMesh mesh = NavMesh::Build(
        {
            {0.0F, 0.0F},
            {0.5F, 0.0F},
            {0.0F, 0.5F},
            {2.0F, 2.0F},
            {2.5F, 2.0F},
            {2.0F, 2.5F}
        },
        {
            NavTriangleIndices{{0, 1, 2}},
            NavTriangleIndices{{3, 4, 5}}
        },
        4.0F);

    const NavQueryResult linear = mesh.FindContainingTriangleLinear({2.1F, 2.1F});
    const NavQueryResult grid = mesh.FindContainingTriangleGrid({2.1F, 2.1F});

    EXPECT_EQ(linear.triangle, grid.triangle);
    EXPECT_EQ(2U, linear.candidatesTested);
    EXPECT_EQ(2U, grid.candidatesTested);
}
} // namespace
