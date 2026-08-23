#include <dxa/simulation/NavMesh.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace dxa::simulation
{
namespace
{
constexpr float GeometryEpsilon = 1.0e-5F;

using EdgeKey = std::pair<std::uint32_t, std::uint32_t>;

[[nodiscard]] float Cross(const Vec2 left, const Vec2 right) noexcept
{
    return left.x * right.z - left.z * right.x;
}

[[nodiscard]] EdgeKey MakeEdgeKey(
    const std::uint32_t first,
    const std::uint32_t second) noexcept
{
    return std::minmax(first, second);
}

[[nodiscard]] bool ContainsPoint(
    const Vec2 point,
    const Vec2 first,
    const Vec2 second,
    const Vec2 third) noexcept
{
    const float firstSide = Cross(second - first, point - first);
    const float secondSide = Cross(third - second, point - second);
    const float thirdSide = Cross(first - third, point - third);
    const bool hasNegative = firstSide < -GeometryEpsilon
        || secondSide < -GeometryEpsilon
        || thirdSide < -GeometryEpsilon;
    const bool hasPositive = firstSide > GeometryEpsilon
        || secondSide > GeometryEpsilon
        || thirdSide > GeometryEpsilon;
    return !(hasNegative && hasPositive);
}
} // namespace

NavMesh NavMesh::Build(
    std::vector<Vec2> vertices,
    std::vector<NavTriangleIndices> triangles,
    const float gridCellSize)
{
    if (vertices.empty()
        || triangles.empty()
        || !std::isfinite(gridCellSize)
        || gridCellSize <= 0.0F
        || triangles.size() > std::numeric_limits<TriangleId>::max())
    {
        throw std::invalid_argument{"NavMesh requires geometry and a positive grid cell size"};
    }
    if (!std::ranges::all_of(vertices, IsFinite))
    {
        throw std::invalid_argument{"NavMesh vertices must be finite"};
    }

    std::vector<TriangleData> data;
    data.reserve(triangles.size());
    std::map<EdgeKey, std::vector<TriangleId>> edgeOwners;
    for (std::size_t index = 0; index < triangles.size(); ++index)
    {
        const NavTriangleIndices& triangle = triangles[index];
        const auto firstIndex = triangle.vertices[0];
        const auto secondIndex = triangle.vertices[1];
        const auto thirdIndex = triangle.vertices[2];
        if (firstIndex >= vertices.size()
            || secondIndex >= vertices.size()
            || thirdIndex >= vertices.size())
        {
            throw std::invalid_argument{"NavMesh triangle references a missing vertex"};
        }
        if (firstIndex == secondIndex
            || secondIndex == thirdIndex
            || thirdIndex == firstIndex)
        {
            throw std::invalid_argument{"NavMesh triangle repeats a vertex"};
        }

        const Vec2 first = vertices[firstIndex];
        const Vec2 second = vertices[secondIndex];
        const Vec2 third = vertices[thirdIndex];
        if (std::fabs(Cross(second - first, third - first)) <= GeometryEpsilon)
        {
            throw std::invalid_argument{"NavMesh triangle area is degenerate"};
        }

        const Vec2 minimum{
            std::min({first.x, second.x, third.x}),
            std::min({first.z, second.z, third.z})};
        const Vec2 maximum{
            std::max({first.x, second.x, third.x}),
            std::max({first.z, second.z, third.z})};
        data.push_back(TriangleData{
            triangle,
            (first + second + third) / 3.0F,
            Aabb2::Create(minimum, maximum),
            {}});

        const TriangleId triangleId = static_cast<TriangleId>(index);
        edgeOwners[MakeEdgeKey(firstIndex, secondIndex)].push_back(triangleId);
        edgeOwners[MakeEdgeKey(secondIndex, thirdIndex)].push_back(triangleId);
        edgeOwners[MakeEdgeKey(thirdIndex, firstIndex)].push_back(triangleId);
    }

    for (const auto& [edge, owners] : edgeOwners)
    {
        static_cast<void>(edge);
        if (owners.size() > 2)
        {
            throw std::invalid_argument{"NavMesh edge has more than two owners"};
        }
        if (owners.size() == 2)
        {
            data[owners[0]].neighbors.push_back(owners[1]);
            data[owners[1]].neighbors.push_back(owners[0]);
        }
    }
    for (TriangleData& triangle : data)
    {
        std::ranges::sort(triangle.neighbors);
    }

    return NavMesh{std::move(vertices), std::move(data), gridCellSize};
}

NavMesh::NavMesh(
    std::vector<Vec2> vertices,
    std::vector<TriangleData> triangles,
    const float gridCellSize) noexcept
    : vertices_{std::move(vertices)},
      triangles_{std::move(triangles)},
      gridCellSize_{gridCellSize}
{
}

NavQueryResult NavMesh::FindContainingTriangleLinear(const Vec2 point) const
{
    if (!IsFinite(point))
    {
        throw std::invalid_argument{"NavMesh query point must be finite"};
    }

    NavQueryResult result;
    for (std::size_t index = 0; index < triangles_.size(); ++index)
    {
        const TriangleData& triangle = triangles_[index];
        ++result.candidatesTested;
        if (!triangle.bounds.Contains(point))
        {
            continue;
        }
        const Vec2 first = vertices_[triangle.indices.vertices[0]];
        const Vec2 second = vertices_[triangle.indices.vertices[1]];
        const Vec2 third = vertices_[triangle.indices.vertices[2]];
        if (ContainsPoint(point, first, second, third) && !result.triangle.has_value())
        {
            result.triangle = static_cast<TriangleId>(index);
        }
    }
    return result;
}

std::span<const TriangleId> NavMesh::Neighbors(const TriangleId triangle) const
{
    return triangles_.at(triangle).neighbors;
}

Vec2 NavMesh::TriangleCenter(const TriangleId triangle) const
{
    return triangles_.at(triangle).center;
}

std::size_t NavMesh::TriangleCount() const noexcept
{
    return triangles_.size();
}
} // namespace dxa::simulation
