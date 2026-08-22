#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dxa::engine::benchmark
{
inline constexpr std::size_t PlayerCount = 24;
inline constexpr std::size_t AiCount = 100;
inline constexpr std::size_t StaticInstanceCount = 1000;
inline constexpr std::size_t DynamicLightCount = 32;

struct SceneVector3
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    [[nodiscard]] bool operator==(const SceneVector3&) const = default;
};

struct SceneInstance
{
    SceneVector3 position;
    float yawRadians = 0.0F;
    float uniformScale = 1.0F;
    float animationPhaseSeconds = 0.0F;

    [[nodiscard]] bool operator==(const SceneInstance&) const = default;
};

struct DynamicPointLight
{
    SceneVector3 position;
    SceneVector3 color;
    float radius = 1.0F;
    float intensity = 1.0F;
    float phaseRadians = 0.0F;

    [[nodiscard]] bool operator==(const DynamicPointLight&) const = default;
};

struct StressScene
{
    std::vector<SceneInstance> players;
    std::vector<SceneInstance> ai;
    std::vector<SceneInstance> staticInstances;
    std::vector<DynamicPointLight> dynamicLights;

    [[nodiscard]] bool operator==(const StressScene&) const = default;
};

struct StressCamera
{
    SceneVector3 eye;
    SceneVector3 target;

    [[nodiscard]] bool operator==(const StressCamera&) const = default;
};

[[nodiscard]] StressScene GenerateStressScene(std::uint32_t seed);
[[nodiscard]] double StressSceneSeconds(std::uint64_t frameIndex);
[[nodiscard]] StressCamera SampleStressCamera(std::uint64_t frameIndex);
} // namespace dxa::engine::benchmark
