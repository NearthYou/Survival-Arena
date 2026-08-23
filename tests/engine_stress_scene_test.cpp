#include <dxa/engine/benchmark/StressScene.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>

namespace
{
using dxa::engine::benchmark::GenerateStressScene;
using dxa::engine::benchmark::SampleStressCamera;
using dxa::engine::benchmark::StressSceneSeconds;

TEST(StressScene, GeneratesTheLockedForwardBaselinePopulation)
{
    const auto scene = GenerateStressScene(20260823);

    EXPECT_EQ(dxa::engine::benchmark::PlayerCount, scene.players.size());
    EXPECT_EQ(dxa::engine::benchmark::AiCount, scene.ai.size());
    EXPECT_EQ(dxa::engine::benchmark::StaticInstanceCount, scene.staticInstances.size());
    EXPECT_EQ(dxa::engine::benchmark::DynamicLightCount, scene.dynamicLights.size());
}

TEST(StressScene, RepeatsEveryGeneratedValueForTheSameSeed)
{
    const auto first = GenerateStressScene(20260823);
    const auto second = GenerateStressScene(20260823);

    EXPECT_EQ(first, second);
}

TEST(StressScene, ChangesLayoutWithoutChangingPopulationForAnotherSeed)
{
    const auto first = GenerateStressScene(20260823);
    const auto second = GenerateStressScene(7);

    ASSERT_EQ(first.players.size(), second.players.size());
    ASSERT_EQ(first.ai.size(), second.ai.size());
    ASSERT_EQ(first.staticInstances.size(), second.staticInstances.size());
    ASSERT_EQ(first.dynamicLights.size(), second.dynamicLights.size());
    EXPECT_NE(first, second);
    EXPECT_NE(first.players.front().position, second.players.front().position);
}

TEST(StressScene, KeepsGeneratedValuesInsideTheBenchmarkArenaContract)
{
    const auto scene = GenerateStressScene(20260823);

    for (const auto& instance : scene.players)
    {
        EXPECT_GE(instance.position.x, -48.0F);
        EXPECT_LE(instance.position.x, 48.0F);
        EXPECT_FLOAT_EQ(0.0F, instance.position.y);
        EXPECT_GE(instance.position.z, -48.0F);
        EXPECT_LE(instance.position.z, 48.0F);
        EXPECT_GE(instance.animationPhaseSeconds, 0.0F);
        EXPECT_LT(instance.animationPhaseSeconds, 2.0F);
    }
    for (const auto& instance : scene.ai)
    {
        EXPECT_GE(instance.position.x, -48.0F);
        EXPECT_LE(instance.position.x, 48.0F);
        EXPECT_FLOAT_EQ(0.0F, instance.position.y);
        EXPECT_GE(instance.position.z, -48.0F);
        EXPECT_LE(instance.position.z, 48.0F);
    }
    for (const auto& light : scene.dynamicLights)
    {
        EXPECT_GE(light.color.x, 0.2F);
        EXPECT_LE(light.color.x, 1.0F);
        EXPECT_GT(light.radius, 0.0F);
        EXPECT_GT(light.intensity, 0.0F);
    }
}

TEST(StressScene, DerivesAnimationAndCameraFromFrameIndex)
{
    EXPECT_DOUBLE_EQ(0.0, StressSceneSeconds(1));
    EXPECT_DOUBLE_EQ(1.0, StressSceneSeconds(61));

    const auto first = SampleStressCamera(1);
    const auto repeated = SampleStressCamera(3601);
    const auto advanced = SampleStressCamera(901);

    EXPECT_EQ(first, repeated);
    EXPECT_NE(first, advanced);
    EXPECT_FLOAT_EQ(0.0F, first.target.y);
    EXPECT_FLOAT_EQ(0.0F, advanced.target.y);
}

TEST(StressScene, RejectsFrameZeroBecauseRuntimeFramesAreOneBased)
{
    EXPECT_THROW((void)StressSceneSeconds(0), std::invalid_argument);
    EXPECT_THROW((void)SampleStressCamera(0), std::invalid_argument);
}
} // namespace
