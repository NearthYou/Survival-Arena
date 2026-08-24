#pragma once

#include <dxa/simulation/NavMesh.hpp>

#include <cstdint>
#include <vector>

namespace dxa::simulation
{
struct ArenaMapDefinition
{
    std::uint32_t mapId = 1U;
    std::vector<Vec2> vertices;
    std::vector<NavTriangleIndices> triangles;
    float gridCellSize = 4.0F;
};

[[nodiscard]] ArenaMapDefinition SurvivalArenaMapDefinition();
[[nodiscard]] NavMesh BuildSurvivalArenaNavMesh();
} // namespace dxa::simulation
