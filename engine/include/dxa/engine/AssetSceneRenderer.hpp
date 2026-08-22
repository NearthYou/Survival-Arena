#pragma once

#include <dxa/engine/assets/AssetFile.hpp>

#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <vector>

namespace dxa::engine
{
class AssetSceneRenderer
{
public:
    void Initialize(
        ID3D11Device* device,
        const std::filesystem::path& shaderPath,
        const std::filesystem::path& assetRoot);
    void Render(ID3D11DeviceContext* context, double totalSeconds, float aspectRatio) const;
    [[nodiscard]] bool AssetSceneReady() const noexcept;

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
    void RenderModel(
        ID3D11DeviceContext* context,
        const GpuModel& model,
        const float* worldMatrix,
        const float* viewProjectionMatrix,
        double totalSeconds) const;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> skinConstantBuffer_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    GpuModel character_;
    GpuModel floor_;
    bool ready_ = false;
};
} // namespace dxa::engine
