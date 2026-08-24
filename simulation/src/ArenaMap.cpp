#include <dxa/simulation/ArenaMap.hpp>

#include <utility>

namespace dxa::simulation
{
ArenaMapDefinition SurvivalArenaMapDefinition()
{
    return ArenaMapDefinition{
        1U,
        {
            {-128.0F, -128.0F},
            {128.0F, -128.0F},
            {-128.0F, 128.0F},
            {128.0F, 128.0F},
        },
        {
            NavTriangleIndices{{0U, 1U, 2U}},
            NavTriangleIndices{{1U, 3U, 2U}},
        },
        4.0F};
}

NavMesh BuildSurvivalArenaNavMesh()
{
    ArenaMapDefinition definition = SurvivalArenaMapDefinition();
    return NavMesh::Build(
        std::move(definition.vertices),
        std::move(definition.triangles),
        definition.gridCellSize);
}
} // namespace dxa::simulation
