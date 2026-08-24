#include <dxa/engine/GroundPlanePicking.hpp>

#include <DirectXMath.h>

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace dxa::engine
{
namespace
{
[[nodiscard]] bool IsFinite(
    const benchmark::SceneVector3 value) noexcept
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}
} // namespace

std::optional<benchmark::SceneVector3> PointerGroundDestination(
    const PointerPosition pointer,
    const std::uint32_t width,
    const std::uint32_t height,
    const benchmark::StressCamera& camera)
{
    using namespace DirectX;
    if (width == 0U
        || height == 0U
        || !IsFinite(camera.eye)
        || !IsFinite(camera.target))
    {
        throw std::invalid_argument{
            "ground picking requires a finite camera and viewport"};
    }
    if (pointer.x < 0
        || pointer.y < 0
        || static_cast<std::uint32_t>(pointer.x) >= width
        || static_cast<std::uint32_t>(pointer.y) >= height)
    {
        throw std::invalid_argument{
            "ground picking pointer must be inside the viewport"};
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
        || XMVectorGetX(XMVector3LengthSq(
               XMVector3Cross(lookDirection, up))) <= 1.0e-12F)
    {
        throw std::invalid_argument{
            "ground picking camera direction is degenerate"};
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
    const benchmark::SceneVector3 destination{
        nearPoint.x + direction.x * distance,
        0.0F,
        nearPoint.z + direction.z * distance};
    return IsFinite(destination)
        ? std::optional<benchmark::SceneVector3>{destination}
        : std::nullopt;
}
} // namespace dxa::engine
