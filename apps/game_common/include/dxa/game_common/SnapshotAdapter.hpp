#pragma once

#include <dxa/protocol/GameSnapshot.hpp>
#include <dxa/simulation/MatchTypes.hpp>

namespace dxa::game_common
{
[[nodiscard]] dxa::protocol::GameSnapshot ToGameSnapshot(
    const dxa::simulation::MatchSnapshot& snapshot);
} // namespace dxa::game_common
