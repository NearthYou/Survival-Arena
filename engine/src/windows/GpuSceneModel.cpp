#include <dxa/engine/detail/GpuSceneModel.hpp>

#include <DDSTextureLoader.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace dxa::engine::detail
{
namespace
{
void RequireSuccess(const HRESULT result, const char* operation)
{
    if (FAILED(result))
    {
        std::ostringstream message;
        message << operation << " failed with HRESULT 0x" << std::hex << std::uppercase
                << static_cast<std::uint32_t>(result);
        throw std::runtime_error{message.str()};
    }
}
} // namespace

GpuSceneModel LoadGpuSceneModel(
    ID3D11Device* const device,
    const std::filesystem::path& modelPath)
{
    if (device == nullptr)
    {
        throw std::invalid_argument{"GPU scene model requires a Direct3D device"};
    }

    GpuSceneModel model;
    model.assetData = asset::LoadModelAsset(modelPath);
    if (model.assetData.vertices.empty() || model.assetData.indices.empty())
    {
        throw std::runtime_error{"runtime model contains no geometry"};
    }

    D3D11_BUFFER_DESC vertexDescription{};
    vertexDescription.ByteWidth = static_cast<UINT>(
        model.assetData.vertices.size() * sizeof(asset::Vertex));
    vertexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    vertexDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexData{};
    vertexData.pSysMem = model.assetData.vertices.data();
    RequireSuccess(
        device->CreateBuffer(
            &vertexDescription,
            &vertexData,
            model.vertexBuffer.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer(scene vertices)");

    D3D11_BUFFER_DESC indexDescription{};
    indexDescription.ByteWidth = static_cast<UINT>(
        model.assetData.indices.size() * sizeof(std::uint32_t));
    indexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    indexDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData{};
    indexData.pSysMem = model.assetData.indices.data();
    RequireSuccess(
        device->CreateBuffer(
            &indexDescription,
            &indexData,
            model.indexBuffer.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer(scene indices)");

    model.minimumBounds = asset::Float3{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    model.maximumBounds = asset::Float3{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};
    for (const asset::Vertex& vertex : model.assetData.vertices)
    {
        model.minimumBounds.x = std::min(model.minimumBounds.x, vertex.position.x);
        model.minimumBounds.y = std::min(model.minimumBounds.y, vertex.position.y);
        model.minimumBounds.z = std::min(model.minimumBounds.z, vertex.position.z);
        model.maximumBounds.x = std::max(model.maximumBounds.x, vertex.position.x);
        model.maximumBounds.y = std::max(model.maximumBounds.y, vertex.position.y);
        model.maximumBounds.z = std::max(model.maximumBounds.z, vertex.position.z);
    }

    model.materials.reserve(model.assetData.materials.size());
    for (const asset::Material& material : model.assetData.materials)
    {
        GpuSceneMaterial gpuMaterial;
        gpuMaterial.baseColor = material.baseColor;
        if (!material.baseColorTexture.empty())
        {
            const std::filesystem::path texturePath =
                modelPath.parent_path() / std::filesystem::path{material.baseColorTexture};
            RequireSuccess(
                DirectX::CreateDDSTextureFromFile(
                    device,
                    texturePath.c_str(),
                    nullptr,
                    gpuMaterial.texture.ReleaseAndGetAddressOf()),
                "DirectX::CreateDDSTextureFromFile(scene material)");
        }
        model.materials.push_back(std::move(gpuMaterial));
    }
    return model;
}
} // namespace dxa::engine::detail
