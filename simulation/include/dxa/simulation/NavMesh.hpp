#pragma once

#include <dxa/simulation/Math2.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
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
    [[nodiscard]] NavQueryResult FindContainingTriangleGrid(Vec2 point) const;
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

    struct GridCellKey
    {
        std::int32_t x = 0;
        std::int32_t z = 0;

        [[nodiscard]] bool operator==(const GridCellKey&) const = default;
    };

    struct GridCellKeyHash
    {
        [[nodiscard]] std::size_t operator()(GridCellKey key) const noexcept;
    };

    NavMesh(
        std::vector<Vec2> vertices,
        std::vector<TriangleData> triangles,
        float gridCellSize) noexcept;

    void BuildGridIndex();
    [[nodiscard]] bool TriangleContains(TriangleId triangle, Vec2 point) const noexcept;

    std::vector<Vec2> vertices_;
    std::vector<TriangleData> triangles_;
    float gridCellSize_ = 4.0F;
    std::unordered_map<GridCellKey, std::vector<TriangleId>, GridCellKeyHash>
        gridCells_;
};
} // namespace dxa::simulation
