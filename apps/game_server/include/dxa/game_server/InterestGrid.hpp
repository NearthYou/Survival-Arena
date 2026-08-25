#pragma once

#include <dxa/protocol/GameSnapshot.hpp>
#include <dxa/simulation/Math2.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dxa::game_server
{
struct VisibleSet
{
    std::vector<dxa::protocol::EntityId> actors;
    std::vector<std::uint32_t> loot;

    [[nodiscard]] bool operator==(const VisibleSet&) const = default;
};

class InterestGrid
{
public:
    InterestGrid(dxa::simulation::Aabb2 bounds, float cellSize);

    void Rebuild(const dxa::protocol::GameSnapshot& world);

    [[nodiscard]] VisibleSet UpdateVisibility(
        const VisibleSet& previous,
        dxa::protocol::NetworkVec2 center,
        float enterRadius,
        float leaveRadius) const;

private:
    struct ActorEntry
    {
        dxa::protocol::EntityId id;
        dxa::protocol::NetworkVec2 position;
    };

    struct LootEntry
    {
        std::uint32_t id = 0U;
        dxa::protocol::NetworkVec2 position;
    };

    struct Cell
    {
        std::vector<ActorEntry> actors;
        std::vector<LootEntry> loot;
    };

    [[nodiscard]] std::size_t Column(float x) const noexcept;
    [[nodiscard]] std::size_t Row(float z) const noexcept;
    [[nodiscard]] std::size_t CellIndex(
        dxa::protocol::NetworkVec2 position) const noexcept;

    dxa::simulation::Aabb2 bounds_;
    float cellSize_ = 0.0F;
    std::size_t columns_ = 0U;
    std::size_t rows_ = 0U;
    std::vector<Cell> cells_;
};
} // namespace dxa::game_server
