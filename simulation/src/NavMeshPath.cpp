#include <dxa/simulation/NavMesh.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dxa::simulation
{
namespace
{
struct OpenNode
{
    TriangleId triangle = 0;
    float gCost = 0.0F;
    float hCost = 0.0F;
};

struct OpenNodeGreater
{
    [[nodiscard]] bool operator()(
        const OpenNode& left,
        const OpenNode& right) const noexcept
    {
        const float leftFCost = left.gCost + left.hCost;
        const float rightFCost = right.gCost + right.hCost;
        if (leftFCost != rightFCost)
        {
            return leftFCost > rightFCost;
        }
        if (left.hCost != right.hCost)
        {
            return left.hCost > right.hCost;
        }
        return left.triangle > right.triangle;
    }
};
} // namespace

std::optional<NavPath> NavMesh::FindPath(
    const Vec2 start,
    const Vec2 destination) const
{
    if (!IsFinite(start) || !IsFinite(destination))
    {
        throw std::invalid_argument{"NavMesh path endpoints must be finite"};
    }

    const NavQueryResult startQuery = FindContainingTriangleGrid(start);
    const NavQueryResult destinationQuery = FindContainingTriangleGrid(destination);
    if (!startQuery.triangle.has_value()
        || !destinationQuery.triangle.has_value())
    {
        return std::nullopt;
    }

    const TriangleId startTriangle = *startQuery.triangle;
    const TriangleId destinationTriangle = *destinationQuery.triangle;
    const Vec2 destinationCenter = TriangleCenter(destinationTriangle);
    std::vector<float> bestCost(
        triangles_.size(),
        std::numeric_limits<float>::infinity());
    std::vector<std::optional<TriangleId>> parents(triangles_.size());
    std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeGreater> open;

    bestCost[startTriangle] = 0.0F;
    open.push(OpenNode{
        startTriangle,
        0.0F,
        Distance(TriangleCenter(startTriangle), destinationCenter)});

    std::uint32_t expandedNodes = 0;
    while (!open.empty())
    {
        const OpenNode current = open.top();
        open.pop();
        if (current.gCost != bestCost[current.triangle])
        {
            continue;
        }

        ++expandedNodes;
        if (current.triangle == destinationTriangle)
        {
            std::vector<TriangleId> trianglePath;
            TriangleId cursor = destinationTriangle;
            while (true)
            {
                trianglePath.push_back(cursor);
                if (cursor == startTriangle)
                {
                    break;
                }
                cursor = *parents[cursor];
            }
            std::ranges::reverse(trianglePath);

            std::vector<Vec2> waypoints;
            waypoints.reserve(trianglePath.size() + 1U);
            waypoints.push_back(start);
            for (std::size_t index = 1; index + 1U < trianglePath.size(); ++index)
            {
                waypoints.push_back(TriangleCenter(trianglePath[index]));
            }
            waypoints.push_back(destination);
            return NavPath{
                std::move(trianglePath),
                std::move(waypoints),
                expandedNodes};
        }

        for (const TriangleId neighbor : Neighbors(current.triangle))
        {
            const float candidateCost = current.gCost + Distance(
                TriangleCenter(current.triangle),
                TriangleCenter(neighbor));
            if (candidateCost >= bestCost[neighbor])
            {
                continue;
            }

            bestCost[neighbor] = candidateCost;
            parents[neighbor] = current.triangle;
            open.push(OpenNode{
                neighbor,
                candidateCost,
                Distance(TriangleCenter(neighbor), destinationCenter)});
        }
    }

    return std::nullopt;
}
} // namespace dxa::simulation
