#pragma once

#include <dxa/engine/AssetSceneRenderer.hpp>
#include <dxa/engine/assets/AssetFile.hpp>
#include <dxa/engine/benchmark/StressScene.hpp>

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace dxa::engine
{
struct HybridDeferredConfig
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t shadowMapSize = 0;
    std::uint32_t stressSceneSeed = 0;
    std::filesystem::path shaderRoot;
    std::filesystem::path assetRoot;
};

class HybridDeferredRenderer
{
public:
    void Initialize(ID3D11Device* device, const HybridDeferredConfig& config);
    [[nodiscard]] RenderStatistics Render(
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* backBufferRenderTarget,
        const AssetSceneFrame& frame) const;
    [[nodiscard]] bool ShadowMapReady() const noexcept;

private:
    struct GpuMaterial
    {
        asset::Float4 baseColor;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
    };

    struct GpuModel
    {
        asset::ModelAsset assetData;
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
        std::vector<GpuMaterial> materials;
        asset::Float3 minimumBounds;
        asset::Float3 maximumBounds;
    };

    [[nodiscard]] static GpuModel LoadModel(
        ID3D11Device* device,
        const std::filesystem::path& modelPath);
    [[nodiscard]] RenderStatistics RenderGeometryModel(
        ID3D11DeviceContext* context,
        const GpuModel& model,
        const float* worldMatrix,
        const float* viewProjectionMatrix,
        double totalSeconds) const;
    [[nodiscard]] RenderStatistics RenderGeometryInstances(
        ID3D11DeviceContext* context,
        const GpuModel& model,
        ID3D11Buffer* instanceBuffer,
        std::uint32_t instanceCount,
        const float* viewProjectionMatrix) const;
    [[nodiscard]] RenderStatistics RenderShadowModel(
        ID3D11DeviceContext* context,
        const GpuModel& model,
        const float* worldMatrix,
        const float* lightViewProjectionMatrix,
        double totalSeconds) const;
    [[nodiscard]] RenderStatistics RenderShadowInstances(
        ID3D11DeviceContext* context,
        const GpuModel& model,
        ID3D11Buffer* instanceBuffer,
        std::uint32_t instanceCount,
        const float* lightViewProjectionMatrix) const;
    [[nodiscard]] RenderStatistics RenderTransparentMarkers(
        ID3D11DeviceContext* context,
        ID3D11RenderTargetView* backBufferRenderTarget,
        ID3D11Buffer* instanceBuffer,
        std::uint32_t instanceCount,
        const float* viewProjectionMatrix) const;
    void UpdateLightingConstants(
        ID3D11DeviceContext* context,
        const float* inverseViewProjectionMatrix,
        const float* lightViewProjectionMatrix,
        double sceneSeconds) const;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> geometryVertexShader_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> geometryInstancedVertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> geometryPixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> geometryInputLayout_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> instancedInputLayout_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> lightingVertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> lightingPixelShader_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> shadowVertexShader_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> shadowInstancedVertexShader_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> transparentVertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> transparentPixelShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> skinConstantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> lightingConstantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> materialSampler_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> gBufferSampler_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> shadowSampler_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> shadowRasterizerState_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> transparentBlendState_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> transparentDepthState_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> staticInstanceBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> markerInstanceBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> albedoRoughnessTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> albedoRoughnessRenderTarget_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> albedoRoughnessShaderResource_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> normalTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> normalRenderTarget_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalShaderResource_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthShaderResource_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> shadowTexture_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> shadowDepthStencilView_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadowShaderResource_;
    GpuModel character_;
    GpuModel floor_;
    benchmark::StressScene stressScene_;
    D3D11_VIEWPORT viewport_{};
    D3D11_VIEWPORT shadowViewport_{};
};
} // namespace dxa::engine
