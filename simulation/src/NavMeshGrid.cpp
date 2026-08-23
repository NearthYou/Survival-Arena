#include <dxa/simulation/NavMesh.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>

namespace dxa::simulation
{
namespace
{
[[nodiscard]] std::optional<std::int32_t> ToCellCoordinate(
    const float value,
    const float cellSize) noexcept
{
    const double coordinate = std::floor(
        static_cast<double>(value) / static_cast<double>(cellSize));
    constexpr double minimum = static_cast<double>(
        std::numeric_limits<std::int32_t>::min());
    constexpr double maximum = static_cast<double>(
        std::numeric_limits<std::int32_t>::max());
    if (coordinate < minimum || coordinate > maximum)
    {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(coordinate);
}
} // namespace

std::size_t NavMesh::GridCellKeyHash::operator()(const GridCellKey key) const noexcept
{
    const std::uint64_t packed =
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(key.x)) << 32U)
        | static_cast<std::uint32_t>(key.z);
    return std::hash<std::uint64_t>{}(packed);
}

void NavMesh::BuildGridIndex()
{
    for (std::size_t index = 0; index < triangles_.size(); ++index)
    {
        const Vec2 minimum = triangles_[index].bounds.Minimum();
        const Vec2 maximum = triangles_[index].bounds.Maximum();
        const auto minimumX = ToCellCoordinate(minimum.x, gridCellSize_);
        const auto minimumZ = ToCellCoordinate(minimum.z, gridCellSize_);
        const auto maximumX = ToCellCoordinate(maximum.x, gridCellSize_);
        const auto maximumZ = ToCellCoordinate(maximum.z, gridCellSize_);
        if (!minimumX.has_value()
            || !minimumZ.has_value()
            || !maximumX.has_value()
            || !maximumZ.has_value())
        {
            throw std::invalid_argument{"NavMesh grid coordinate is out of range"};
        }

        for (std::int32_t cellZ = *minimumZ;; ++cellZ)
        {
            for (std::int32_t cellX = *minimumX;; ++cellX)
            {
                gridCells_[GridCellKey{cellX, cellZ}].push_back(
                    static_cast<TriangleId>(index));
                if (cellX == *maximumX)
                {
                    break;
                }
            }
            if (cellZ == *maximumZ)
            {
                break;
            }
        }
    }

    for (auto& [cell, triangles] : gridCells_)
    {
        static_cast<void>(cell);
        std::ranges::sort(triangles);
        triangles.erase(
            std::unique(triangles.begin(), triangles.end()),
            triangles.end());
    }
}

NavQueryResult NavMesh::FindContainingTriangleGrid(const Vec2 point) const
{
    if (!IsFinite(point))
    {
        throw std::invalid_argument{"NavMesh query point must be finite"};
    }

    const auto cellX = ToCellCoordinate(point.x, gridCellSize_);
    const auto cellZ = ToCellCoordinate(point.z, gridCellSize_);
    if (!cellX.has_value() || !cellZ.has_value())
    {
        return {};
    }

    const auto cell = gridCells_.find(GridCellKey{*cellX, *cellZ});
    if (cell == gridCells_.end())
    {
        return {};
    }

    NavQueryResult result;
    for (const TriangleId triangle : cell->second)
    {
        if (!triangles_[triangle].bounds.Contains(point))
        {
            continue;
        }
        ++result.candidatesTested;
        if (TriangleContains(triangle, point))
        {
            result.triangle = triangle;
            break;
        }
    }
    return result;
}
} // namespace dxa::simulation
