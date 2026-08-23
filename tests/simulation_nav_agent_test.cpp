#include <dxa/simulation/NavAgent.hpp>
#include <dxa/simulation/NavMesh.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <vector>

namespace
{
using dxa::simulation::NavAgent;
using dxa::simulation::NavAgentState;
using dxa::simulation::NavMesh;
using dxa::simulation::NavTriangleIndices;
using dxa::simulation::TriangleId;
using dxa::simulation::Vec2;

[[nodiscard]] NavMesh MakeForkedNavMesh()
{
    return NavMesh::Build(
        {
            {0.0F, 0.0F},
            {2.0F, 0.0F},
            {1.0F, 1.0F},
            {2.0F, 2.0F},
            {0.0F, 2.0F}
        },
        {
            NavTriangleIndices{{0, 1, 2}},
            NavTriangleIndices{{1, 3, 2}},
            NavTriangleIndices{{0, 2, 4}},
            NavTriangleIndices{{2, 3, 4}}
        });
}

TEST(NavMeshPath, ChoosesLowerTriangleIdForEqualCostRoute)
{
    const NavMesh mesh = MakeForkedNavMesh();
    const auto path = mesh.FindPath({1.0F, 0.2F}, {1.0F, 1.8F});

    ASSERT_TRUE(path.has_value());
    EXPECT_EQ((std::vector<TriangleId>{0, 1, 3}), path->triangles);
    ASSERT_EQ(3U, path->waypoints.size());
    EXPECT_EQ((Vec2{1.0F, 0.2F}), path->waypoints.front());
    EXPECT_EQ(mesh.TriangleCenter(1), path->waypoints[1]);
    EXPECT_EQ((Vec2{1.0F, 1.8F}), path->waypoints.back());
    EXPECT_GT(path->expandedNodes, 0U);
}

TEST(NavMeshPath, HandlesSameTriangleAndRejectsUnreachableEndpoints)
{
    const NavMesh mesh = MakeForkedNavMesh();
    const auto sameTriangle = mesh.FindPath({0.5F, 0.2F}, {1.5F, 0.2F});

    ASSERT_TRUE(sameTriangle.has_value());
    EXPECT_EQ((std::vector<TriangleId>{0}), sameTriangle->triangles);
    EXPECT_EQ((std::vector<Vec2>{{0.5F, 0.2F}, {1.5F, 0.2F}}),
              sameTriangle->waypoints);
    EXPECT_FALSE(mesh.FindPath({-1.0F, -1.0F}, {1.0F, 1.8F}).has_value());

    const NavMesh disconnected = NavMesh::Build(
        {
            {0.0F, 0.0F},
            {1.0F, 0.0F},
            {0.0F, 1.0F},
            {10.0F, 0.0F},
            {11.0F, 0.0F},
            {10.0F, 1.0F}
        },
        {
            NavTriangleIndices{{0, 1, 2}},
            NavTriangleIndices{{3, 4, 5}}
        });
    EXPECT_FALSE(disconnected.FindPath({0.2F, 0.2F}, {10.2F, 0.2F}).has_value());
}

TEST(NavMeshPath, RejectsNonFiniteEndpoint)
{
    const NavMesh mesh = MakeForkedNavMesh();
    const float notANumber = std::numeric_limits<float>::quiet_NaN();

    EXPECT_THROW(
        (void)mesh.FindPath({1.0F, 0.2F}, {notANumber, 1.0F}),
        std::invalid_argument);
}

TEST(NavAgent, ConsumesLargeDeltaAcrossMultipleWaypointsWithoutOvershoot)
{
    const NavMesh mesh = MakeForkedNavMesh();
    NavAgent agent{mesh, {1.0F, 0.2F}, 4.0F, 0.01F};

    ASSERT_TRUE(agent.SetDestination({1.0F, 1.8F}));
    EXPECT_EQ(NavAgentState::Moving, agent.State());
    agent.Tick(1.0F);

    EXPECT_EQ(NavAgentState::Arrived, agent.State());
    EXPECT_NEAR(1.0F, agent.Position().x, 1.0e-4F);
    EXPECT_NEAR(1.8F, agent.Position().z, 1.0e-4F);
}

TEST(NavAgent, LeavesPositionUnchangedForZeroDeltaAndRejectsNegativeDelta)
{
    const NavMesh mesh = MakeForkedNavMesh();
    NavAgent agent{mesh, {1.0F, 0.2F}, 1.0F, 0.01F};
    ASSERT_TRUE(agent.SetDestination({1.0F, 1.8F}));
    const Vec2 before = agent.Position();

    agent.Tick(0.0F);
    EXPECT_EQ(before, agent.Position());
    EXPECT_EQ(NavAgentState::Moving, agent.State());
    EXPECT_THROW(agent.Tick(-0.01F), std::invalid_argument);
    EXPECT_THROW(
        agent.Tick(std::numeric_limits<float>::infinity()),
        std::invalid_argument);
}

TEST(NavAgent, ReportsInvalidDestinationWithoutMoving)
{
    const NavMesh mesh = MakeForkedNavMesh();
    NavAgent agent{mesh, {1.0F, 0.2F}, 1.0F, 0.01F};

    EXPECT_FALSE(agent.SetDestination({10.0F, 10.0F}));
    EXPECT_EQ(NavAgentState::InvalidDestination, agent.State());
    EXPECT_EQ((Vec2{1.0F, 0.2F}), agent.Position());
}
} // namespace
