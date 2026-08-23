#include <dxa/simulation/MatchResolution.hpp>

#include <gtest/gtest.h>

#include <array>
#include <span>
#include <stdexcept>

namespace
{
using dxa::simulation::ContenderRankInput;
using dxa::simulation::SelectSurvivalWinner;

TEST(MatchResolution, RanksAliveBeforeDeadContender)
{
    const std::array contenders{
        ContenderRankInput{0U, false, 100, 99U},
        ContenderRankInput{8U, true, 1, 0U}};

    EXPECT_EQ(8U, SelectSurvivalWinner(contenders));
}

TEST(MatchResolution, RanksHealthBeforeEliminations)
{
    const std::array contenders{
        ContenderRankInput{4U, true, 80, 20U},
        ContenderRankInput{1U, true, 100, 1U}};

    EXPECT_EQ(1U, SelectSurvivalWinner(contenders));
}

TEST(MatchResolution, RanksEliminationsWhenHealthTies)
{
    const std::array contenders{
        ContenderRankInput{4U, true, 80, 3U},
        ContenderRankInput{1U, true, 80, 2U}};

    EXPECT_EQ(4U, SelectSurvivalWinner(contenders));
}

TEST(MatchResolution, RanksLowerIdWhenEveryScoreTies)
{
    const std::array contenders{
        ContenderRankInput{9U, true, 80, 3U},
        ContenderRankInput{4U, true, 80, 3U}};

    EXPECT_EQ(4U, SelectSurvivalWinner(contenders));
}

TEST(MatchResolution, RejectsEmptyDuplicateAndNegativeHealthInput)
{
    EXPECT_THROW(
        (void)SelectSurvivalWinner(std::span<const ContenderRankInput>{}),
        std::invalid_argument);

    const std::array duplicate{
        ContenderRankInput{1U, true, 80, 0U},
        ContenderRankInput{1U, true, 70, 0U}};
    EXPECT_THROW((void)SelectSurvivalWinner(duplicate), std::invalid_argument);

    const std::array negative{
        ContenderRankInput{1U, true, -1, 0U}};
    EXPECT_THROW((void)SelectSurvivalWinner(negative), std::invalid_argument);
}
} // namespace
