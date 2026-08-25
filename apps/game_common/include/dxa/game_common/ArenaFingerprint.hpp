#pragma once

#include <dxa/simulation/ArenaMap.hpp>

#include <cstdint>

namespace dxa::game_common
{
[[nodiscard]] std::uint32_t SurvivalArenaFingerprint(
    const dxa::simulation::ArenaMapDefinition& definition);
} // namespace dxa::game_common
