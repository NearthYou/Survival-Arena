#include <dxa/simulation/MatchResolution.hpp>

#include <algorithm>
#include <set>
#include <stdexcept>

namespace dxa::simulation
{
ActorId SelectSurvivalWinner(const std::span<const ContenderRankInput> contenders)
{
    if (contenders.empty())
    {
        throw std::invalid_argument{"survival ranking requires at least one contender"};
    }

    std::set<ActorId> ids;
    for (const ContenderRankInput& contender : contenders)
    {
        if (contender.health < 0)
        {
            throw std::invalid_argument{"survival ranking health cannot be negative"};
        }
        if (!ids.insert(contender.id).second)
        {
            throw std::invalid_argument{"survival ranking contender IDs must be unique"};
        }
    }

    const auto winner = std::min_element(
        contenders.begin(),
        contenders.end(),
        [](const ContenderRankInput& left, const ContenderRankInput& right) {
            if (left.alive != right.alive)
            {
                return left.alive > right.alive;
            }
            if (left.health != right.health)
            {
                return left.health > right.health;
            }
            if (left.eliminations != right.eliminations)
            {
                return left.eliminations > right.eliminations;
            }
            return left.id < right.id;
        });
    return winner->id;
}
} // namespace dxa::simulation
