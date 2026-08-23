#include <dxa/navigation_demo/GroundPicking.hpp>

#include <DirectXMath.h>

#include <cmath>
#include <stdexcept>

namespace dxa::navigation_demo
{
namespace
{
[[nodiscard]] bool IsFinite(
    const dxa::engine::benchmark::SceneVector3 value) noexcept
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}
} // namespace

std::optional<dxa::simulation::Vec2> PointerGroundDestination(
    const dxa::engine::PointerPosition pointer,
    const std::uint32_t width,
    const std::uint32_t height,
    const dxa::engine::benchmark::StressCamera& camera)
{
    using namespace DirectX;
    if (width == 0
        || height == 0
        || !IsFinite(camera.eye)
        || !IsFinite(camera.target))
    {
        throw std::invalid_argument{"ground picking requires a finite camera and viewport"};
    }

    const XMVECTOR eye = XMVectorSet(
        camera.eye.x,
        camera.eye.y,
        camera.eye.z,
        1.0F);
    const XMVECTOR target = XMVectorSet(
        camera.target.x,
        camera.target.y,
        camera.target.z,
        1.0F);
    const XMVECTOR up = XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F);
    const XMVECTOR lookDirection = XMVectorSubtract(target, eye);
    if (XMVectorGetX(XMVector3LengthSq(lookDirection)) <= 1.0e-12F
        || XMVectorGetX(XMVector3LengthSq(XMVector3Cross(lookDirection, up)))
            <= 1.0e-12F)
    {
        throw std::invalid_argument{"ground picking camera direction is degenerate"};
    }

    const XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XM_PIDIV4,
        static_cast<float>(width) / static_cast<float>(height),
        0.1F,
        200.0F);
    const XMMATRIX world = XMMatrixIdentity();
    const XMVECTOR screenNear = XMVectorSet(
        static_cast<float>(pointer.x),
        static_cast<float>(pointer.y),
        0.0F,
        1.0F);
    const XMVECTOR screenFar = XMVectorSet(
        static_cast<float>(pointer.x),
        static_cast<float>(pointer.y),
        1.0F,
        1.0F);
    const XMVECTOR worldNear = XMVector3Unproject(
        screenNear,
        0.0F,
        0.0F,
        static_cast<float>(width),
        static_cast<float>(height),
        0.0F,
        1.0F,
        projection,
        view,
        world);
    const XMVECTOR worldFar = XMVector3Unproject(
        screenFar,
        0.0F,
        0.0F,
        static_cast<float>(width),
        static_cast<float>(height),
        0.0F,
        1.0F,
        projection,
        view,
        world);

    DirectX::XMFLOAT3 nearPoint;
    DirectX::XMFLOAT3 direction;
    XMStoreFloat3(&nearPoint, worldNear);
    XMStoreFloat3(
        &direction,
        XMVector3Normalize(XMVectorSubtract(worldFar, worldNear)));
    if (!std::isfinite(nearPoint.y)
        || !std::isfinite(direction.y)
        || std::fabs(direction.y) <= 1.0e-6F)
    {
        return std::nullopt;
    }

    const float distance = -nearPoint.y / direction.y;
    if (!std::isfinite(distance) || distance < 0.0F)
    {
        return std::nullopt;
    }
    const dxa::simulation::Vec2 destination{
        nearPoint.x + direction.x * distance,
        nearPoint.z + direction.z * distance};
    return dxa::simulation::IsFinite(destination)
        ? std::optional<dxa::simulation::Vec2>{destination}
        : std::nullopt;
}
} // namespace dxa::navigation_demo
