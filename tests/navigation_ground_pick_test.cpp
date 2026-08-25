#include <dxa/engine/GroundPlanePicking.hpp>

#include <dxa/engine/benchmark/StressScene.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace
{
TEST(NavigationGroundPick, CenterPointerIntersectsTheStressCameraTarget)
{
    const auto camera = dxa::engine::benchmark::SampleStressCamera(1U);

    const auto destination = dxa::engine::PointerGroundDestination(
        {160, 90},
        320U,
        180U,
        camera);

    ASSERT_TRUE(destination.has_value());
    EXPECT_NEAR(camera.target.x, destination->x, 2.0e-3F);
    EXPECT_FLOAT_EQ(0.0F, destination->y);
    EXPECT_NEAR(camera.target.z, destination->z, 2.0e-3F);
}

TEST(NavigationGroundPick, ParallelRayHasNoGroundDestination)
{
    const dxa::engine::benchmark::StressCamera camera{
        {0.0F, 1.0F, 0.0F},
        {1.0F, 1.0F, 0.0F}};

    EXPECT_FALSE(dxa::engine::PointerGroundDestination(
                     {160, 90},
                     320U,
                     180U,
                     camera)
                     .has_value());
}

TEST(NavigationGroundPick, BehindCameraIntersectionIsRejected)
{
    const dxa::engine::benchmark::StressCamera camera{
        {0.0F, -1.0F, 0.0F},
        {0.0F, -2.0F, 1.0F}};

    EXPECT_FALSE(dxa::engine::PointerGroundDestination(
                     {160, 90},
                     320U,
                     180U,
                     camera)
                     .has_value());
}

TEST(NavigationGroundPick, RejectsNonFiniteCamera)
{
    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    const dxa::engine::benchmark::StressCamera camera{
        {notANumber, 1.0F, 0.0F},
        {0.0F, 0.0F, 0.0F}};

    EXPECT_THROW(
        (void)dxa::engine::PointerGroundDestination(
            {160, 90},
            320U,
            180U,
            camera),
        std::invalid_argument);
}

TEST(NavigationGroundPick, RejectsZeroViewportDimensions)
{
    const auto camera = dxa::engine::benchmark::SampleStressCamera(1U);

    EXPECT_THROW(
        (void)dxa::engine::PointerGroundDestination(
            {0, 0},
            0U,
            180U,
            camera),
        std::invalid_argument);
}

TEST(NavigationGroundPick, RejectsPointerOutsideViewport)
{
    const auto camera = dxa::engine::benchmark::SampleStressCamera(1U);

    EXPECT_THROW(
        (void)dxa::engine::PointerGroundDestination(
            {320, 90},
            320U,
            180U,
            camera),
        std::invalid_argument);
    EXPECT_THROW(
        (void)dxa::engine::PointerGroundDestination(
            {-1, 0},
            320U,
            180U,
            camera),
        std::invalid_argument);
}
} // namespace
