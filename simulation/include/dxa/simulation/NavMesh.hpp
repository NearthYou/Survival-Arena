#pragma once

#include <dxa/simulation/Math2.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace dxa::simulation
{
using TriangleId = std::uint32_t;

struct NavTriangleIndices
{
    std::array<std::uint32_t, 3> vertices{};
};

struct NavQueryResult
{
    std::optional<TriangleId> triangle;
    std::uint32_t candidatesTested = 0;
};

class NavMesh
{
public:
    [[nodiscard]] static NavMesh Build(
        std::vector<Vec2> vertices,
        std::vector<NavTriangleIndices> triangles,
        float gridCellSize = 4.0F);

    [[nodiscard]] NavQueryResult FindContainingTriangleLinear(Vec2 point) const;
    [[nodiscard]] std::span<const TriangleId> Neighbors(TriangleId triangle) const;
    [[nodiscard]] Vec2 TriangleCenter(TriangleId triangle) const;
    [[nodiscard]] std::size_t TriangleCount() const noexcept;

private:
    struct TriangleData
    {
        NavTriangleIndices indices;
        Vec2 center;
        Aabb2 bounds;
        std::vector<TriangleId> neighbors;
    };

    NavMesh(
        std::vector<Vec2> vertices,
        std::vector<TriangleData> triangles,
        float gridCellSize) noexcept;

    std::vector<Vec2> vertices_;
    std::vector<TriangleData> triangles_;
    float gridCellSize_ = 4.0F;
};
} // namespace dxa::simulation
