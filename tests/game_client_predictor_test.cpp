#include <dxa/game_client/ClientPredictor.hpp>

#include <dxa/simulation/ArenaMap.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
using dxa::game_client::ClientPredictor;
using dxa::game_client::PredictionSynchronizationError;
using dxa::game_client::PredictedInput;
using dxa::simulation::BuildSurvivalArenaNavMesh;
using dxa::simulation::Vec2;
} // namespace

TEST(ClientPredictor, ReconcilesAndReplaysOnlyUnacknowledgedInputs)
{
    ClientPredictor predictor{
        BuildSurvivalArenaNavMesh(),
        {10.0F, 0.0F},
        6.0F,
        0.1F};
    ASSERT_TRUE(predictor.SetDestination({0.0F, 0.0F}));
    const PredictedInput first = predictor.AdvanceTick();
    const PredictedInput second = predictor.AdvanceTick();
    const Vec2 before = predictor.Position();

    predictor.Reconcile({9.7F, 0.0F}, first.sequence);

    EXPECT_NE(before, predictor.Position());
    EXPECT_NEAR(9.5F, predictor.Position().x, 0.0001F);
    EXPECT_EQ(1U, predictor.HistorySize());
    EXPECT_EQ(second.sequence, predictor.LastIssuedSequence());
}

TEST(ClientPredictor, EqualAckReplaysHistoryAndStaleAckDoesNothing)
{
    ClientPredictor predictor{
        BuildSurvivalArenaNavMesh(),
        {10.0F, 0.0F},
        6.0F,
        0.1F};
    ASSERT_TRUE(predictor.SetDestination({0.0F, 0.0F}));
    const PredictedInput first = predictor.AdvanceTick();
    static_cast<void>(predictor.AdvanceTick());
    predictor.Reconcile({9.8F, 0.0F}, first.sequence);
    const Vec2 reconciled = predictor.Position();

    predictor.Reconcile({100.0F, 0.0F}, first.sequence - 1U);

    EXPECT_EQ(reconciled, predictor.Position());
    EXPECT_EQ(1U, predictor.HistorySize());
}

TEST(ClientPredictor, RejectsAckAboveLastIssuedSequence)
{
    ClientPredictor predictor{
        BuildSurvivalArenaNavMesh(),
        {10.0F, 0.0F},
        6.0F,
        0.1F};
    static_cast<void>(predictor.AdvanceTick());

    EXPECT_THROW(
        predictor.Reconcile({9.8F, 0.0F}, 2U),
        PredictionSynchronizationError);
}

TEST(ClientPredictor, AcceptsTwoHundredFiftySixInputsAndRejectsNext)
{
    ClientPredictor predictor{
        BuildSurvivalArenaNavMesh(),
        {10.0F, 0.0F},
        6.0F,
        0.1F};
    ASSERT_TRUE(predictor.SetDestination({0.0F, 0.0F}));
    for (std::uint32_t index = 0U; index < 256U; ++index)
    {
        static_cast<void>(predictor.AdvanceTick());
    }

    EXPECT_EQ(256U, predictor.HistorySize());
    EXPECT_EQ(256U, predictor.LastIssuedSequence());
    EXPECT_THROW(
        (void)predictor.AdvanceTick(),
        PredictionSynchronizationError);
}

TEST(ClientPredictor, RejectsNonFiniteAndOffMeshDestinationWithoutMutation)
{
    ClientPredictor predictor{
        BuildSurvivalArenaNavMesh(),
        {10.0F, 0.0F},
        6.0F,
        0.1F};
    ASSERT_TRUE(predictor.SetDestination({0.0F, 0.0F}));
    const Vec2 before = predictor.Position();

    EXPECT_FALSE(predictor.SetDestination({
        std::numeric_limits<float>::quiet_NaN(),
        0.0F}));
    EXPECT_FALSE(predictor.SetDestination({999.0F, 999.0F}));
    static_cast<void>(predictor.AdvanceTick());

    EXPECT_LT(predictor.Position().x, before.x);
}

TEST(ClientPredictor, RejectsInvalidAuthoritativePosition)
{
    ClientPredictor predictor{
        BuildSurvivalArenaNavMesh(),
        {10.0F, 0.0F},
        6.0F,
        0.1F};

    EXPECT_THROW(
        predictor.Reconcile({
            std::numeric_limits<float>::infinity(),
            0.0F}, 0U),
        std::invalid_argument);
    EXPECT_THROW(
        predictor.Reconcile({999.0F, 999.0F}, 0U),
        std::invalid_argument);
}
