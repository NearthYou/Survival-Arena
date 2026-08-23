#pragma once

#include <dxa/simulation/MatchTypes.hpp>

#include <cstdint>
#include <span>

namespace dxa::simulation
{
struct ContenderRankInput
{
    ActorId id = 0;
    bool alive = false;
    std::int32_t health = 0;
    std::uint32_t eliminations = 0;

    [[nodiscard]] bool operator==(const ContenderRankInput&) const = default;
};

[[nodiscard]] ActorId SelectSurvivalWinner(
    std::span<const ContenderRankInput> contenders);
} // namespace dxa::simulation
