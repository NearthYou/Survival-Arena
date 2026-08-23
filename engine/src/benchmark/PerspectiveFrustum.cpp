#include <dxa/engine/benchmark/PerspectiveFrustum.hpp>

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace dxa::engine::benchmark
{
namespace
{
[[nodiscard]] SceneVector3 Add(const SceneVector3 left, const SceneVector3 right) noexcept
{
    return SceneVector3{left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] SceneVector3 Subtract(
    const SceneVector3 left,
    const SceneVector3 right) noexcept
{
    return SceneVector3{left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] SceneVector3 Scale(const SceneVector3 value, const float scale) noexcept
{
    return SceneVector3{value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] float Dot(const SceneVector3 left, const SceneVector3 right) noexcept
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] SceneVector3 Cross(const SceneVector3 left, const SceneVector3 right) noexcept
{
    return SceneVector3{
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x};
}

[[nodiscard]] SceneVector3 Normalize(const SceneVector3 value)
{
    const float lengthSquared = Dot(value, value);
    if (!std::isfinite(lengthSquared) || lengthSquared <= 0.000001F)
    {
        throw std::invalid_argument{"frustum camera basis is degenerate"};
    }
    return Scale(value, 1.0F / std::sqrt(lengthSquared));
}

[[nodiscard]] FrustumPlane PlaneThroughPoint(
    const SceneVector3 normal,
    const SceneVector3 point) noexcept
{
    return FrustumPlane{normal, -Dot(normal, point)};
}
} // namespace

PerspectiveFrustum::PerspectiveFrustum(
    std::array<FrustumPlane, 6> planes) noexcept
    : planes_{planes}
{
}

bool PerspectiveFrustum::IntersectsSphere(const BoundingSphere& sphere) const noexcept
{
    if (!std::isfinite(sphere.radius) || sphere.radius < 0.0F)
    {
        return false;
    }
    for (const FrustumPlane& plane : planes_)
    {
        if (Dot(plane.normal, sphere.center) + plane.distance < -sphere.radius)
        {
            return false;
        }
    }
    return true;
}

PerspectiveFrustum BuildPerspectiveFrustum(
    const StressCamera& camera,
    const float verticalFovRadians,
    const float aspectRatio,
    const float nearPlane,
    const float farPlane)
{
    if (!std::isfinite(verticalFovRadians)
        || verticalFovRadians <= 0.0F
        || verticalFovRadians >= std::numbers::pi_v<float>
        || !std::isfinite(aspectRatio)
        || aspectRatio <= 0.0F
        || !std::isfinite(nearPlane)
        || nearPlane <= 0.0F
        || !std::isfinite(farPlane)
        || farPlane <= nearPlane)
    {
        throw std::invalid_argument{"invalid perspective frustum projection"};
    }

    constexpr SceneVector3 WorldUp{0.0F, 1.0F, 0.0F};
    const SceneVector3 forward = Normalize(Subtract(camera.target, camera.eye));
    const SceneVector3 right = Normalize(Cross(WorldUp, forward));
    const SceneVector3 up = Cross(forward, right);
    const float tangentY = std::tan(verticalFovRadians * 0.5F);
    const float tangentX = tangentY * aspectRatio;

    const SceneVector3 nearPoint = Add(camera.eye, Scale(forward, nearPlane));
    const SceneVector3 farPoint = Add(camera.eye, Scale(forward, farPlane));
    const SceneVector3 nearNormal = forward;
    const SceneVector3 farNormal = Scale(forward, -1.0F);
    const SceneVector3 leftNormal = Normalize(Add(Scale(forward, tangentX), right));
    const SceneVector3 rightNormal = Normalize(Subtract(Scale(forward, tangentX), right));
    const SceneVector3 bottomNormal = Normalize(Add(Scale(forward, tangentY), up));
    const SceneVector3 topNormal = Normalize(Subtract(Scale(forward, tangentY), up));

    return PerspectiveFrustum{std::array{
        PlaneThroughPoint(nearNormal, nearPoint),
        PlaneThroughPoint(farNormal, farPoint),
        PlaneThroughPoint(leftNormal, camera.eye),
        PlaneThroughPoint(rightNormal, camera.eye),
        PlaneThroughPoint(bottomNormal, camera.eye),
        PlaneThroughPoint(topNormal, camera.eye)}};
}
} // namespace dxa::engine::benchmark
