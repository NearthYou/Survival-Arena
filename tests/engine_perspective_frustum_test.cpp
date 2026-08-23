#include <dxa/engine/benchmark/PerspectiveFrustum.hpp>
#include <dxa/engine/benchmark/StressScene.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace
{
using dxa::engine::benchmark::BoundingSphere;
using dxa::engine::benchmark::BuildPerspectiveFrustum;
using dxa::engine::benchmark::GenerateStressScene;
using dxa::engine::benchmark::SampleStressCamera;
using dxa::engine::benchmark::SceneVector3;
using dxa::engine::benchmark::StressCamera;

TEST(PerspectiveFrustum, IncludesSpheresTouchingNearAndFarPlanes)
{
    const StressCamera camera{
        SceneVector3{0.0F, 0.0F, 0.0F},
        SceneVector3{0.0F, 0.0F, 1.0F}};
    const auto frustum = BuildPerspectiveFrustum(
        camera,
        std::numbers::pi_v<float> / 2.0F,
        1.0F,
        1.0F,
        10.0F);

    EXPECT_TRUE(frustum.IntersectsSphere(
        BoundingSphere{SceneVector3{0.0F, 0.0F, 0.6F}, 0.5F}));
    EXPECT_FALSE(frustum.IntersectsSphere(
        BoundingSphere{SceneVector3{0.0F, 0.0F, 0.4F}, 0.5F}));
    EXPECT_TRUE(frustum.IntersectsSphere(
        BoundingSphere{SceneVector3{0.0F, 0.0F, 10.4F}, 0.5F}));
    EXPECT_FALSE(frustum.IntersectsSphere(
        BoundingSphere{SceneVector3{0.0F, 0.0F, 10.6F}, 0.5F}));
}

TEST(PerspectiveFrustum, IncludesSpheresTouchingSidePlanes)
{
    const StressCamera camera{
        SceneVector3{0.0F, 0.0F, 0.0F},
        SceneVector3{0.0F, 0.0F, 1.0F}};
    const auto frustum = BuildPerspectiveFrustum(
        camera,
        std::numbers::pi_v<float> / 2.0F,
        1.0F,
        1.0F,
        10.0F);

    EXPECT_TRUE(frustum.IntersectsSphere(
        BoundingSphere{SceneVector3{5.4F, 0.0F, 5.0F}, 0.5F}));
    EXPECT_FALSE(frustum.IntersectsSphere(
        BoundingSphere{SceneVector3{6.0F, 0.0F, 5.0F}, 0.5F}));
    EXPECT_TRUE(frustum.IntersectsSphere(
        BoundingSphere{SceneVector3{0.0F, -5.4F, 5.0F}, 0.5F}));
    EXPECT_FALSE(frustum.IntersectsSphere(
        BoundingSphere{SceneVector3{0.0F, -6.0F, 5.0F}, 0.5F}));
}

TEST(PerspectiveFrustum, ProducesTheSameVisibilityMaskForTheSameStressFrame)
{
    const auto scene = GenerateStressScene(20260823);
    const auto camera = SampleStressCamera(1);
    const auto first = BuildPerspectiveFrustum(
        camera,
        std::numbers::pi_v<float> / 4.0F,
        16.0F / 9.0F,
        0.1F,
        200.0F);
    const auto second = BuildPerspectiveFrustum(
        camera,
        std::numbers::pi_v<float> / 4.0F,
        16.0F / 9.0F,
        0.1F,
        200.0F);

    std::vector<bool> firstMask;
    std::vector<bool> secondMask;
    firstMask.reserve(scene.staticInstances.size() + scene.players.size() + scene.ai.size());
    secondMask.reserve(firstMask.capacity());
    const auto appendInstances = [&](const auto& instances, const float radius) {
        for (const auto& instance : instances)
        {
            const BoundingSphere sphere{instance.position, radius * instance.uniformScale};
            firstMask.push_back(first.IntersectsSphere(sphere));
            secondMask.push_back(second.IntersectsSphere(sphere));
        }
    };
    appendInstances(scene.staticInstances, 4.0F);
    appendInstances(scene.players, 1.2F);
    appendInstances(scene.ai, 1.2F);

    EXPECT_EQ(firstMask, secondMask);
    EXPECT_NE(firstMask.end(), std::ranges::find(firstMask, true));
    EXPECT_NE(firstMask.end(), std::ranges::find(firstMask, false));
}

TEST(PerspectiveFrustum, RejectsInvalidProjectionAndCameraInputs)
{
    const StressCamera camera{
        SceneVector3{0.0F, 0.0F, 0.0F},
        SceneVector3{0.0F, 0.0F, 1.0F}};
    const StressCamera degenerate{camera.eye, camera.eye};

    EXPECT_THROW(
        (void)BuildPerspectiveFrustum(camera, 0.0F, 1.0F, 0.1F, 10.0F),
        std::invalid_argument);
    EXPECT_THROW(
        (void)BuildPerspectiveFrustum(
            camera, std::numbers::pi_v<float>, 1.0F, 0.1F, 10.0F),
        std::invalid_argument);
    EXPECT_THROW(
        (void)BuildPerspectiveFrustum(
            camera, std::numbers::pi_v<float> / 4.0F, 0.0F, 0.1F, 10.0F),
        std::invalid_argument);
    EXPECT_THROW(
        (void)BuildPerspectiveFrustum(
            camera, std::numbers::pi_v<float> / 4.0F, 1.0F, 10.0F, 10.0F),
        std::invalid_argument);
    EXPECT_THROW(
        (void)BuildPerspectiveFrustum(
            degenerate, std::numbers::pi_v<float> / 4.0F, 1.0F, 0.1F, 10.0F),
        std::invalid_argument);
}

TEST(PerspectiveFrustum, RejectsNonFiniteBoundingSphereValues)
{
    const auto frustum = BuildPerspectiveFrustum(
        StressCamera{
            SceneVector3{0.0F, 0.0F, 0.0F},
            SceneVector3{0.0F, 0.0F, 1.0F}},
        std::numbers::pi_v<float> / 4.0F,
        16.0F / 9.0F,
        0.1F,
        100.0F);
    const float notANumber = std::numeric_limits<float>::quiet_NaN();

    EXPECT_FALSE(frustum.IntersectsSphere(
        BoundingSphere{SceneVector3{notANumber, 0.0F, 5.0F}, 1.0F}));
    EXPECT_FALSE(frustum.IntersectsSphere(
        BoundingSphere{SceneVector3{0.0F, 0.0F, 5.0F}, notANumber}));
}
} // namespace
