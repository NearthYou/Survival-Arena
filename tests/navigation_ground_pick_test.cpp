#include <dxa/navigation_demo/GroundPicking.hpp>

#include <dxa/engine/benchmark/StressScene.hpp>

#include <gtest/gtest.h>

#include <stdexcept>

namespace
{
TEST(NavigationGroundPick, CenterPointerIntersectsTheStressCameraTarget)
{
    const auto camera = dxa::engine::benchmark::SampleStressCamera(1U);

    const auto destination = dxa::navigation_demo::PointerGroundDestination(
        {160, 90},
        320U,
        180U,
        camera);

    ASSERT_TRUE(destination.has_value());
    EXPECT_NEAR(camera.target.x, destination->x, 2.0e-3F);
    EXPECT_NEAR(camera.target.z, destination->z, 2.0e-3F);
}

TEST(NavigationGroundPick, ParallelRayHasNoGroundDestination)
{
    const dxa::engine::benchmark::StressCamera camera{
        {0.0F, 1.0F, 0.0F},
        {1.0F, 1.0F, 0.0F}};

    EXPECT_FALSE(dxa::navigation_demo::PointerGroundDestination(
                     {160, 90},
                     320U,
                     180U,
                     camera)
                     .has_value());
}

TEST(NavigationGroundPick, RejectsZeroViewportDimensions)
{
    const auto camera = dxa::engine::benchmark::SampleStressCamera(1U);

    EXPECT_THROW(
        (void)dxa::navigation_demo::PointerGroundDestination(
            {0, 0},
            0U,
            180U,
            camera),
        std::invalid_argument);
}
} // namespace
