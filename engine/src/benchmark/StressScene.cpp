#include <dxa/engine/benchmark/StressScene.hpp>

#include <cmath>
#include <numbers>
#include <random>
#include <stdexcept>

namespace dxa::engine::benchmark
{
namespace
{
constexpr float SimulatedFramesPerSecond = 60.0F;
constexpr float CameraPeriodSeconds = 60.0F;

[[nodiscard]] float UnitFloat(std::mt19937& random)
{
    constexpr float InverseRange = 1.0F / 16777216.0F;
    return static_cast<float>(random() >> 8U) * InverseRange;
}

[[nodiscard]] float Range(
    std::mt19937& random,
    const float minimum,
    const float maximum)
{
    return minimum + (maximum - minimum) * UnitFloat(random);
}

[[nodiscard]] SceneInstance MakeCharacter(
    std::mt19937& random,
    const SceneVector3 position,
    const float yawRadians)
{
    return SceneInstance{
        position,
        yawRadians,
        Range(random, 0.85F, 1.15F),
        Range(random, 0.0F, 2.0F)};
}
} // namespace

StressScene GenerateStressScene(const std::uint32_t seed)
{
    std::mt19937 random{seed};
    StressScene scene;
    scene.players.reserve(PlayerCount);
    scene.ai.reserve(AiCount);
    scene.staticInstances.reserve(StaticInstanceCount);
    scene.dynamicLights.reserve(DynamicLightCount);

    constexpr float TwoPi = 2.0F * std::numbers::pi_v<float>;
    for (std::size_t index = 0; index < PlayerCount; ++index)
    {
        const float fraction = static_cast<float>(index) / static_cast<float>(PlayerCount);
        const float angle = fraction * TwoPi + Range(random, -0.05F, 0.05F);
        const float radius = Range(random, 9.0F, 18.0F);
        scene.players.push_back(MakeCharacter(
            random,
            SceneVector3{std::cos(angle) * radius, 0.0F, std::sin(angle) * radius},
            angle + std::numbers::pi_v<float>));
    }

    for (std::size_t index = 0; index < AiCount; ++index)
    {
        static_cast<void>(index);
        const SceneVector3 position{
            Range(random, -44.0F, 44.0F),
            0.0F,
            Range(random, -44.0F, 44.0F)};
        scene.ai.push_back(MakeCharacter(
            random,
            position,
            Range(random, 0.0F, TwoPi)));
    }

    constexpr std::size_t StaticColumns = 40;
    constexpr std::size_t StaticRows = StaticInstanceCount / StaticColumns;
    static_assert(StaticColumns * StaticRows == StaticInstanceCount);
    for (std::size_t row = 0; row < StaticRows; ++row)
    {
        for (std::size_t column = 0; column < StaticColumns; ++column)
        {
            const float x = (static_cast<float>(column) - 19.5F) * 2.2F
                + Range(random, -0.30F, 0.30F);
            const float z = (static_cast<float>(row) - 12.0F) * 2.2F
                + Range(random, -0.30F, 0.30F);
            scene.staticInstances.push_back(SceneInstance{
                SceneVector3{x, -0.03F, z},
                Range(random, 0.0F, TwoPi),
                Range(random, 0.18F, 0.32F),
                0.0F});
        }
    }

    for (std::size_t index = 0; index < DynamicLightCount; ++index)
    {
        static_cast<void>(index);
        scene.dynamicLights.push_back(DynamicPointLight{
            SceneVector3{
                Range(random, -42.0F, 42.0F),
                Range(random, 2.5F, 8.0F),
                Range(random, -42.0F, 42.0F)},
            SceneVector3{
                Range(random, 0.2F, 1.0F),
                Range(random, 0.2F, 1.0F),
                Range(random, 0.2F, 1.0F)},
            Range(random, 8.0F, 18.0F),
            Range(random, 1.5F, 4.0F),
            Range(random, 0.0F, TwoPi)});
    }

    return scene;
}

double StressSceneSeconds(const std::uint64_t frameIndex)
{
    if (frameIndex == 0)
    {
        throw std::invalid_argument{"stress scene frame indices are one-based"};
    }
    return static_cast<double>(frameIndex - 1) / static_cast<double>(SimulatedFramesPerSecond);
}

StressCamera SampleStressCamera(const std::uint64_t frameIndex)
{
    const float seconds = static_cast<float>(StressSceneSeconds(frameIndex));
    const float phase = std::fmod(seconds, CameraPeriodSeconds)
        / CameraPeriodSeconds
        * 2.0F
        * std::numbers::pi_v<float>;
    return StressCamera{
        SceneVector3{
            std::cos(phase) * 54.0F,
            32.0F + std::sin(phase * 2.0F) * 3.0F,
            std::sin(phase) * 54.0F},
        SceneVector3{
            std::sin(phase * 0.5F) * 5.0F,
            0.0F,
            std::cos(phase * 0.5F) * 5.0F}};
}
} // namespace dxa::engine::benchmark
