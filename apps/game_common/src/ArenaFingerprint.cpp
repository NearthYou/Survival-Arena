#include <dxa/game_common/ArenaFingerprint.hpp>

#include <dxa/protocol/ByteCodec.hpp>
#include <dxa/protocol/Crc32.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dxa::game_common
{
namespace
{
[[nodiscard]] std::uint32_t CheckedCount(const std::size_t count)
{
    if (count > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::length_error{"arena map collection exceeds uint32 count"};
    }
    return static_cast<std::uint32_t>(count);
}
} // namespace

std::uint32_t SurvivalArenaFingerprint(
    const dxa::simulation::ArenaMapDefinition& definition)
{
    dxa::protocol::ByteWriter writer;
    writer.WriteU32(definition.mapId);
    writer.WriteU32(CheckedCount(definition.vertices.size()));
    for (const dxa::simulation::Vec2 vertex : definition.vertices)
    {
        writer.WriteF32(vertex.x);
        writer.WriteF32(vertex.z);
    }
    writer.WriteU32(CheckedCount(definition.triangles.size()));
    for (const dxa::simulation::NavTriangleIndices& triangle
         : definition.triangles)
    {
        for (const std::uint32_t vertex : triangle.vertices)
        {
            writer.WriteU32(vertex);
        }
    }
    writer.WriteF32(definition.gridCellSize);

    const std::vector<std::byte> bytes = std::move(writer).Finish();
    return dxa::protocol::Crc32(bytes);
}
} // namespace dxa::game_common
