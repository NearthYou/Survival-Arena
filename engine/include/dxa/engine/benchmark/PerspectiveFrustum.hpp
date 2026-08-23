#pragma once

#include <dxa/engine/benchmark/StressScene.hpp>

#include <array>

namespace dxa::engine::benchmark
{
struct BoundingSphere
{
    SceneVector3 center;
    float radius = 0.0F;
};

struct FrustumPlane
{
    SceneVector3 normal;
    float distance = 0.0F;
};

class PerspectiveFrustum
{
public:
    explicit PerspectiveFrustum(std::array<FrustumPlane, 6> planes) noexcept;

    [[nodiscard]] bool IntersectsSphere(const BoundingSphere& sphere) const noexcept;

private:
    std::array<FrustumPlane, 6> planes_;
};

[[nodiscard]] PerspectiveFrustum BuildPerspectiveFrustum(
    const StressCamera& camera,
    float verticalFovRadians,
    float aspectRatio,
    float nearPlane,
    float farPlane);
} // namespace dxa::engine::benchmark
