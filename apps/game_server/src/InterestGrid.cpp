#include <dxa/game_server/InterestGrid.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dxa::game_server
{
namespace
{
using dxa::protocol::EntityId;
using dxa::protocol::GameSnapshot;
using dxa::protocol::NetworkVec2;
using dxa::simulation::Aabb2;
using dxa::simulation::Vec2;

[[nodiscard]] Vec2 ToSimulation(const NetworkVec2 value) noexcept
{
    return {value.x, value.z};
}

[[nodiscard]] bool IsFinite(const NetworkVec2 value) noexcept
{
    return dxa::simulation::IsFinite(ToSimulation(value));
}

template <typename Value, typename Key>
[[nodiscard]] bool HasDuplicateIds(
    const std::vector<Value>& values,
    Key key)
{
    std::vector<decltype(key(values.front()))> ids;
    ids.reserve(values.size());
    for (const Value& value : values)
    {
        ids.push_back(key(value));
    }
    std::sort(ids.begin(), ids.end());
    return std::adjacent_find(ids.begin(), ids.end()) != ids.end();
}

template <typename Id>
void ValidatePreviousIds(const std::vector<Id>& ids)
{
    if (!std::is_sorted(ids.begin(), ids.end())
        || std::adjacent_find(ids.begin(), ids.end()) != ids.end())
    {
        throw std::invalid_argument{"previous visibility IDs are not canonical"};
    }
}

[[nodiscard]] float DistanceSquared(
    const NetworkVec2 left,
    const NetworkVec2 right) noexcept
{
    return dxa::simulation::LengthSquared(
        ToSimulation(left) - ToSimulation(right));
}

template <typename Id>
[[nodiscard]] bool WasVisible(
    const std::vector<Id>& previous,
    const Id id)
{
    return std::binary_search(previous.begin(), previous.end(), id);
}

template <typename Id>
void SortAndDeduplicate(std::vector<Id>& ids)
{
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}
} // namespace

InterestGrid::InterestGrid(Aabb2 bounds, const float cellSize)
    : bounds_{bounds},
      cellSize_{cellSize}
{
    const Vec2 minimum = bounds_.Minimum();
    const Vec2 maximum = bounds_.Maximum();
    const Vec2 extent = maximum - minimum;
    if (!std::isfinite(cellSize_)
        || cellSize_ <= 0.0F
        || extent.x <= 0.0F
        || extent.z <= 0.0F)
    {
        throw std::invalid_argument{"interest grid configuration is invalid"};
    }

    const double columns = std::ceil(
        static_cast<double>(extent.x) / static_cast<double>(cellSize_));
    const double rows = std::ceil(
        static_cast<double>(extent.z) / static_cast<double>(cellSize_));
    const double maximumSize = static_cast<double>(
        std::numeric_limits<std::size_t>::max());
    if (columns < 1.0
        || rows < 1.0
        || columns > maximumSize
        || rows > maximumSize)
    {
        throw std::length_error{"interest grid dimensions are invalid"};
    }

    columns_ = static_cast<std::size_t>(columns);
    rows_ = static_cast<std::size_t>(rows);
    if (columns_ > std::numeric_limits<std::size_t>::max() / rows_)
    {
        throw std::length_error{"interest grid cell count overflows"};
    }
    cells_.resize(columns_ * rows_);
}

void InterestGrid::Rebuild(const GameSnapshot& world)
{
    if (world.actors.size() > dxa::protocol::MaxSnapshotActors
        || world.loot.size() > dxa::protocol::MaxSnapshotLoot)
    {
        throw std::length_error{"interest grid world exceeds snapshot bounds"};
    }
    if ((!world.actors.empty()
         && HasDuplicateIds(
             world.actors,
             [](const dxa::protocol::NetworkActorSnapshot& actor) {
                 return actor.id;
             }))
        || (!world.loot.empty()
            && HasDuplicateIds(
                world.loot,
                [](const dxa::protocol::NetworkLootSnapshot& loot) {
                    return loot.id;
                })))
    {
        throw std::invalid_argument{"interest grid IDs must be unique"};
    }

    for (const dxa::protocol::NetworkActorSnapshot& actor : world.actors)
    {
        if (!IsFinite(actor.position))
        {
            throw std::invalid_argument{"actor position is not finite"};
        }
        if (!bounds_.Contains(ToSimulation(actor.position)))
        {
            throw std::out_of_range{"actor position is outside grid bounds"};
        }
    }
    for (const dxa::protocol::NetworkLootSnapshot& loot : world.loot)
    {
        if (!IsFinite(loot.position))
        {
            throw std::invalid_argument{"loot position is not finite"};
        }
        if (!bounds_.Contains(ToSimulation(loot.position)))
        {
            throw std::out_of_range{"loot position is outside grid bounds"};
        }
    }

    std::vector<Cell> rebuilt(cells_.size());
    for (const dxa::protocol::NetworkActorSnapshot& actor : world.actors)
    {
        rebuilt[CellIndex(actor.position)].actors.push_back(
            ActorEntry{actor.id, actor.position});
    }
    for (const dxa::protocol::NetworkLootSnapshot& loot : world.loot)
    {
        if (loot.active)
        {
            rebuilt[CellIndex(loot.position)].loot.push_back(
                LootEntry{loot.id, loot.position});
        }
    }
    for (Cell& cell : rebuilt)
    {
        std::sort(
            cell.actors.begin(),
            cell.actors.end(),
            [](const ActorEntry& left, const ActorEntry& right) {
                return left.id < right.id;
            });
        std::sort(
            cell.loot.begin(),
            cell.loot.end(),
            [](const LootEntry& left, const LootEntry& right) {
                return left.id < right.id;
            });
    }
    cells_.swap(rebuilt);
}

VisibleSet InterestGrid::UpdateVisibility(
    const VisibleSet& previous,
    const NetworkVec2 center,
    const float enterRadius,
    const float leaveRadius) const
{
    ValidatePreviousIds(previous.actors);
    ValidatePreviousIds(previous.loot);
    if (!IsFinite(center)
        || !std::isfinite(enterRadius)
        || !std::isfinite(leaveRadius)
        || enterRadius <= 0.0F
        || leaveRadius < enterRadius)
    {
        throw std::invalid_argument{"interest visibility query is invalid"};
    }
    if (!bounds_.Contains(ToSimulation(center)))
    {
        throw std::out_of_range{"interest center is outside grid bounds"};
    }

    const Vec2 boundsMinimum = bounds_.Minimum();
    const Vec2 boundsMaximum = bounds_.Maximum();
    const NetworkVec2 queryMinimum{
        std::max(boundsMinimum.x, center.x - leaveRadius),
        std::max(boundsMinimum.z, center.z - leaveRadius)};
    const NetworkVec2 queryMaximum{
        std::min(boundsMaximum.x, center.x + leaveRadius),
        std::min(boundsMaximum.z, center.z + leaveRadius)};
    const std::size_t minimumColumn = Column(queryMinimum.x);
    const std::size_t maximumColumn = Column(queryMaximum.x);
    const std::size_t minimumRow = Row(queryMinimum.z);
    const std::size_t maximumRow = Row(queryMaximum.z);
    const float enterRadiusSquared = enterRadius * enterRadius;
    const float leaveRadiusSquared = leaveRadius * leaveRadius;

    VisibleSet visible;
    for (std::size_t row = minimumRow; row <= maximumRow; ++row)
    {
        for (std::size_t column = minimumColumn;
             column <= maximumColumn;
             ++column)
        {
            const Cell& cell = cells_[row * columns_ + column];
            for (const ActorEntry& actor : cell.actors)
            {
                const float maximumDistance = WasVisible(
                    previous.actors,
                    actor.id)
                    ? leaveRadiusSquared
                    : enterRadiusSquared;
                if (DistanceSquared(actor.position, center) <= maximumDistance)
                {
                    visible.actors.push_back(actor.id);
                }
            }
            for (const LootEntry& loot : cell.loot)
            {
                const float maximumDistance = WasVisible(
                    previous.loot,
                    loot.id)
                    ? leaveRadiusSquared
                    : enterRadiusSquared;
                if (DistanceSquared(loot.position, center) <= maximumDistance)
                {
                    visible.loot.push_back(loot.id);
                }
            }
        }
    }
    SortAndDeduplicate(visible.actors);
    SortAndDeduplicate(visible.loot);
    return visible;
}

std::size_t InterestGrid::Column(const float x) const noexcept
{
    const float relative = (x - bounds_.Minimum().x) / cellSize_;
    const std::size_t column = static_cast<std::size_t>(
        std::max(0.0F, std::floor(relative)));
    return std::min(column, columns_ - 1U);
}

std::size_t InterestGrid::Row(const float z) const noexcept
{
    const float relative = (z - bounds_.Minimum().z) / cellSize_;
    const std::size_t row = static_cast<std::size_t>(
        std::max(0.0F, std::floor(relative)));
    return std::min(row, rows_ - 1U);
}

std::size_t InterestGrid::CellIndex(const NetworkVec2 position) const noexcept
{
    return Row(position.z) * columns_ + Column(position.x);
}
} // namespace dxa::game_server
