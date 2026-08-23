#include <dxa/simulation/NavMesh.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
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
} // namespace
