#pragma once

#include <dxa/engine/assets/AssetFile.hpp>

#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <vector>

namespace dxa::engine::detail
{
struct GpuSceneMaterial
{
    asset::Float4 baseColor;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
};

struct GpuSceneModel
{
    asset::ModelAsset assetData;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    std::vector<GpuSceneMaterial> materials;
    asset::Float3 minimumBounds;
    asset::Float3 maximumBounds;
};

[[nodiscard]] GpuSceneModel LoadGpuSceneModel(
    ID3D11Device* device,
    const std::filesystem::path& modelPath);
} // namespace dxa::engine::detail
