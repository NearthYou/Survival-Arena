#include <dxa/simulation/SafeZone.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace
{
using dxa::simulation::EvaluateSafeZone;
using dxa::simulation::IsOutsideSafeZone;
using dxa::simulation::SafeZoneDamageForTick;
using dxa::simulation::SafeZoneStage;
using dxa::simulation::SafeZoneState;

TEST(SafeZone, InterpolatesEveryLockedBoundary)
{
    EXPECT_FLOAT_EQ(32.0F, EvaluateSafeZone(0U, 30U).radius);
    EXPECT_FLOAT_EQ(24.0F, EvaluateSafeZone(3600U, 30U).radius);
    EXPECT_FLOAT_EQ(16.0F, EvaluateSafeZone(7200U, 30U).radius);
    EXPECT_FLOAT_EQ(8.0F, EvaluateSafeZone(10800U, 30U).radius);
    EXPECT_FLOAT_EQ(2.0F, EvaluateSafeZone(14400U, 30U).radius);
    EXPECT_FLOAT_EQ(0.0F, EvaluateSafeZone(18000U, 30U).radius);
}

TEST(SafeZone, UsesLinearRadiusBetweenBoundaries)
{
    EXPECT_FLOAT_EQ(28.0F, EvaluateSafeZone(1800U, 30U).radius);
    EXPECT_FLOAT_EQ(20.0F, EvaluateSafeZone(5400U, 30U).radius);
    EXPECT_FLOAT_EQ(12.0F, EvaluateSafeZone(9000U, 30U).radius);
    EXPECT_FLOAT_EQ(5.0F, EvaluateSafeZone(12600U, 30U).radius);
    EXPECT_FLOAT_EQ(1.0F, EvaluateSafeZone(16200U, 30U).radius);
}

TEST(SafeZone, EntersTheNextStageAtEachBoundary)
{
    EXPECT_EQ(SafeZoneStage::Stage1, EvaluateSafeZone(3599U, 30U).stage);
    EXPECT_EQ(SafeZoneStage::Stage2, EvaluateSafeZone(3600U, 30U).stage);
    EXPECT_EQ(SafeZoneStage::Stage3, EvaluateSafeZone(7200U, 30U).stage);
    EXPECT_EQ(SafeZoneStage::Stage4, EvaluateSafeZone(10800U, 30U).stage);
    EXPECT_EQ(SafeZoneStage::SuddenDeath, EvaluateSafeZone(14400U, 30U).stage);
    EXPECT_EQ(SafeZoneStage::SuddenDeath, EvaluateSafeZone(20000U, 30U).stage);
}

TEST(SafeZone, ScalesTimeBoundariesWithTickRate)
{
    const SafeZoneState atTwoMinutes = EvaluateSafeZone(7200U, 60U);
    EXPECT_EQ(SafeZoneStage::Stage2, atTwoMinutes.stage);
    EXPECT_FLOAT_EQ(24.0F, atTwoMinutes.radius);
    EXPECT_EQ(4, atTwoMinutes.damagePerSecond);
}

TEST(SafeZone, AppliesIntegerDamageOnlyOnWholeSeconds)
{
    EXPECT_EQ(0, SafeZoneDamageForTick(0U, 30U));
    EXPECT_EQ(0, SafeZoneDamageForTick(29U, 30U));
    EXPECT_EQ(2, SafeZoneDamageForTick(30U, 30U));
    EXPECT_EQ(0, SafeZoneDamageForTick(3599U, 30U));
    EXPECT_EQ(4, SafeZoneDamageForTick(3600U, 30U));
    EXPECT_EQ(16, SafeZoneDamageForTick(10800U, 30U));
    EXPECT_EQ(32, SafeZoneDamageForTick(14400U, 30U));
}

TEST(SafeZone, IncludesCenterAndCircleBoundary)
{
    const SafeZoneState state{
        SafeZoneStage::Stage4,
        {2.0F, -3.0F},
        5.0F,
        16};

    EXPECT_FALSE(IsOutsideSafeZone({2.0F, -3.0F}, state));
    EXPECT_FALSE(IsOutsideSafeZone({7.0F, -3.0F}, state));
    EXPECT_TRUE(IsOutsideSafeZone({7.01F, -3.0F}, state));
}

TEST(SafeZone, ZeroRadiusKeepsOnlyTheCenterSafe)
{
    const SafeZoneState state{
        SafeZoneStage::SuddenDeath,
        {0.0F, 0.0F},
        0.0F,
        32};

    EXPECT_FALSE(IsOutsideSafeZone({0.0F, 0.0F}, state));
    EXPECT_TRUE(IsOutsideSafeZone({0.001F, 0.0F}, state));
}

TEST(SafeZone, RejectsZeroTickRate)
{
    EXPECT_THROW((void)EvaluateSafeZone(0U, 0U), std::invalid_argument);
    EXPECT_THROW((void)SafeZoneDamageForTick(30U, 0U), std::invalid_argument);
}

TEST(SafeZone, RejectsInvalidPositionAndState)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const SafeZoneState valid;
    EXPECT_THROW((void)IsOutsideSafeZone({nan, 0.0F}, valid), std::invalid_argument);

    SafeZoneState invalidCenter;
    invalidCenter.center = {0.0F, nan};
    EXPECT_THROW(
        (void)IsOutsideSafeZone({0.0F, 0.0F}, invalidCenter),
        std::invalid_argument);

    SafeZoneState invalidRadius;
    invalidRadius.radius = -1.0F;
    EXPECT_THROW(
        (void)IsOutsideSafeZone({0.0F, 0.0F}, invalidRadius),
        std::invalid_argument);

    SafeZoneState invalidDamage;
    invalidDamage.damagePerSecond = -1;
    EXPECT_THROW(
        (void)IsOutsideSafeZone({0.0F, 0.0F}, invalidDamage),
        std::invalid_argument);
}
} // namespace
