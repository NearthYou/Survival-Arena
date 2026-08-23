#pragma once

#include <dxa/engine/assets/AssetFile.hpp>
#include <dxa/engine/benchmark/StressScene.hpp>
#include <dxa/engine/detail/GpuSceneModel.hpp>

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <filesystem>
#include <optional>

namespace dxa::engine
{
struct AssetSceneConfig
{
    std::optional<std::uint32_t> stressSceneSeed;
};

struct AssetSceneFrame
{
    std::uint64_t frameIndex = 0;
    double totalSeconds = 0.0;
    float aspectRatio = 1.0F;
};

struct RenderStatistics
{
    std::uint32_t drawCalls = 0;
    std::uint64_t triangleCount = 0;
    std::uint32_t objectCount = 0;
    std::uint32_t shadowDrawCalls = 0;
    std::uint32_t gBufferDrawCalls = 0;
    std::uint32_t lightingDrawCalls = 0;
    std::uint32_t transparentDrawCalls = 0;
    std::uint32_t visibleObjectCount = 0;
    std::uint32_t culledObjectCount = 0;
};

class AssetSceneRenderer
{
public:
    void Initialize(
        ID3D11Device* device,
        const std::filesystem::path& shaderPath,
        const std::filesystem::path& assetRoot,
        AssetSceneConfig config = {});
    [[nodiscard]] RenderStatistics Render(
        ID3D11DeviceContext* context,
        const AssetSceneFrame& frame) const;
    [[nodiscard]] bool AssetSceneReady() const noexcept;

private:
    [[nodiscard]] RenderStatistics RenderModel(
        ID3D11DeviceContext* context,
        const detail::GpuSceneModel& model,
        const float* worldMatrix,
        const float* viewProjectionMatrix,
        double totalSeconds) const;
    void UpdateLights(
        ID3D11DeviceContext* context,
        double sceneSeconds) const;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> skinConstantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> lightingConstantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    detail::GpuSceneModel character_;
    detail::GpuSceneModel floor_;
    std::optional<benchmark::StressScene> stressScene_;
    bool ready_ = false;
};
} // namespace dxa::engine
