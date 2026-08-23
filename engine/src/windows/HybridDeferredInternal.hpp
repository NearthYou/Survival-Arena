#pragma once

#include <dxa/engine/assets/AssetFile.hpp>
#include <dxa/engine/benchmark/StressScene.hpp>

#include <DirectXMath.h>

#include <array>
#include <cstdint>

namespace dxa::engine::detail
{
using InstanceTransform = std::array<float, 16>;

struct HybridSceneConstants
{
    DirectX::XMFLOAT4X4 worldViewProjection;
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4 baseColor;
    std::uint32_t hasTexture = 0;
    float roughness = 0.65F;
    std::array<float, 2> padding{};
};

struct HybridSkinConstants
{
    std::array<DirectX::XMFLOAT4X4, asset::MaximumSkinJoints> boneMatrices;
};

struct HybridGpuPointLight
{
    DirectX::XMFLOAT4 positionAndRadius;
    DirectX::XMFLOAT4 colorAndIntensity;
};

struct HybridLightingConstants
{
    DirectX::XMFLOAT4X4 inverseViewProjection;
    DirectX::XMFLOAT4X4 lightViewProjection;
    std::array<HybridGpuPointLight, benchmark::DynamicLightCount> pointLights;
    std::uint32_t pointLightCount = 0;
    float shadowTexelSize = 0.0F;
    std::array<float, 2> padding{};
};

static_assert(sizeof(InstanceTransform) == sizeof(float) * 16);
static_assert(sizeof(HybridSceneConstants) % 16 == 0);
static_assert(sizeof(HybridSkinConstants) % 16 == 0);
static_assert(sizeof(HybridLightingConstants) % 16 == 0);
} // namespace dxa::engine::detail
