#include <dxa/engine/AssetSceneRenderer.hpp>

#include <dxa/engine/assets/AnimationPlayback.hpp>

#include <DDSTextureLoader.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <span>
#include <stdexcept>

namespace dxa::engine
{
namespace
{
struct SceneConstants
{
    DirectX::XMFLOAT4X4 worldViewProjection;
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4 baseColor;
    std::uint32_t hasTexture = 0;
    std::array<float, 3> padding{};
};

struct SkinConstants
{
    std::array<DirectX::XMFLOAT4X4, asset::MaximumSkinJoints> boneMatrices;
};

struct GpuPointLight
{
    DirectX::XMFLOAT4 positionAndRadius;
    DirectX::XMFLOAT4 colorAndIntensity;
};

struct LightingConstants
{
    std::array<GpuPointLight, benchmark::DynamicLightCount> pointLights;
    std::uint32_t pointLightCount = 0;
    std::array<float, 3> padding{};
};

static_assert(sizeof(SceneConstants) % 16 == 0);
static_assert(sizeof(SkinConstants) % 16 == 0);
static_assert(sizeof(LightingConstants) % 16 == 0);
static_assert(sizeof(asset::Vertex) == 56);
static_assert(offsetof(asset::Vertex, jointIndices) == 32);
static_assert(offsetof(asset::Vertex, jointWeights) == 40);

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

[[nodiscard]] Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
    const std::filesystem::path& shaderPath,
    const char* entryPoint,
    const char* target)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> shader;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompileFromFile(
        shaderPath.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint,
        target,
        flags,
        0,
        shader.GetAddressOf(),
        errors.GetAddressOf());
    if (FAILED(result))
    {
        const std::string details = errors == nullptr
            ? std::string{"no compiler diagnostics"}
            : std::string{
                  static_cast<const char*>(errors->GetBufferPointer()),
                  errors->GetBufferSize()};
        throw std::runtime_error{"asset scene shader compilation failed: " + details};
    }
    return shader;
}

[[nodiscard]] DirectX::XMMATRIX LoadMatrix(const float* elements)
{
    return DirectX::XMLoadFloat4x4(
        reinterpret_cast<const DirectX::XMFLOAT4X4*>(elements));
}

[[nodiscard]] DirectX::XMMATRIX CharacterWorld(
    const asset::Float3 minimumBounds,
    const asset::Float3 maximumBounds)
{
    const float height = std::max(
        maximumBounds.y - minimumBounds.y,
        0.001F);
    const float scale = 2.2F / height;
    return DirectX::XMMatrixScaling(scale, scale, scale)
        * DirectX::XMMatrixTranslation(0.0F, -minimumBounds.y * scale, 0.0F);
}

[[nodiscard]] DirectX::XMMATRIX FloorWorld(
    const asset::Float3 minimumBounds,
    const asset::Float3 maximumBounds)
{
    const float width = std::max(
        {maximumBounds.x - minimumBounds.x,
         maximumBounds.z - minimumBounds.z,
         0.001F});
    const float scale = 8.0F / width;
    return DirectX::XMMatrixScaling(scale, scale, scale)
        * DirectX::XMMatrixTranslation(0.0F, -minimumBounds.y * scale - 0.02F, 0.0F);
}

[[nodiscard]] DirectX::XMMATRIX PlaceInstance(
    const DirectX::XMMATRIX baseWorld,
    const benchmark::SceneInstance& instance)
{
    return baseWorld
        * DirectX::XMMatrixScaling(
            instance.uniformScale,
            instance.uniformScale,
            instance.uniformScale)
        * DirectX::XMMatrixRotationY(instance.yawRadians)
        * DirectX::XMMatrixTranslation(
            instance.position.x,
            instance.position.y,
            instance.position.z);
}

void Accumulate(RenderStatistics& total, const RenderStatistics value)
{
    total.drawCalls += value.drawCalls;
    total.triangleCount += value.triangleCount;
    total.objectCount += value.objectCount;
}
} // namespace

void AssetSceneRenderer::Initialize(
    ID3D11Device* const device,
    const std::filesystem::path& shaderPath,
    const std::filesystem::path& assetRoot,
    const AssetSceneConfig config)
{
    if (device == nullptr)
    {
        throw std::invalid_argument{"asset scene renderer requires a Direct3D device"};
    }

    const auto vertexShaderBytecode = CompileShader(shaderPath, "VSMain", "vs_5_0");
    const auto pixelShaderBytecode = CompileShader(shaderPath, "PSMain", "ps_5_0");
    RequireSuccess(
        device->CreateVertexShader(
            vertexShaderBytecode->GetBufferPointer(),
            vertexShaderBytecode->GetBufferSize(),
            nullptr,
            vertexShader_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateVertexShader(asset scene)");
    RequireSuccess(
        device->CreatePixelShader(
            pixelShaderBytecode->GetBufferPointer(),
            pixelShaderBytecode->GetBufferSize(),
            nullptr,
            pixelShader_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreatePixelShader(asset scene)");

    constexpr D3D11_INPUT_ELEMENT_DESC InputElements[]{
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(asset::Vertex, position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(asset::Vertex, normal)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(asset::Vertex, texcoord)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0, static_cast<UINT>(offsetof(asset::Vertex, jointIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(asset::Vertex, jointWeights)), D3D11_INPUT_PER_VERTEX_DATA, 0}};
    RequireSuccess(
        device->CreateInputLayout(
            InputElements,
            static_cast<UINT>(std::size(InputElements)),
            vertexShaderBytecode->GetBufferPointer(),
            vertexShaderBytecode->GetBufferSize(),
            inputLayout_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateInputLayout(asset scene)");

    D3D11_BUFFER_DESC sceneBufferDescription{};
    sceneBufferDescription.ByteWidth = sizeof(SceneConstants);
    sceneBufferDescription.Usage = D3D11_USAGE_DEFAULT;
    sceneBufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    RequireSuccess(
        device->CreateBuffer(
            &sceneBufferDescription,
            nullptr,
            sceneConstantBuffer_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer(asset scene constants)");

    D3D11_BUFFER_DESC skinBufferDescription{};
    skinBufferDescription.ByteWidth = sizeof(SkinConstants);
    skinBufferDescription.Usage = D3D11_USAGE_DEFAULT;
    skinBufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    RequireSuccess(
        device->CreateBuffer(
            &skinBufferDescription,
            nullptr,
            skinConstantBuffer_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer(skin constants)");

    D3D11_BUFFER_DESC lightingBufferDescription{};
    lightingBufferDescription.ByteWidth = sizeof(LightingConstants);
    lightingBufferDescription.Usage = D3D11_USAGE_DEFAULT;
    lightingBufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    RequireSuccess(
        device->CreateBuffer(
            &lightingBufferDescription,
            nullptr,
            lightingConstantBuffer_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer(lighting constants)");

    D3D11_SAMPLER_DESC samplerDescription{};
    samplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
    RequireSuccess(
        device->CreateSamplerState(&samplerDescription, sampler_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateSamplerState(asset scene)");

    character_ = LoadModel(device, assetRoot / L"characters" / L"cyber-runner.dxam");
    floor_ = LoadModel(device, assetRoot / L"environment" / L"prototype-floor.dxam");
    if (config.stressSceneSeed.has_value())
    {
        stressScene_ = benchmark::GenerateStressScene(*config.stressSceneSeed);
    }
    else
    {
        stressScene_.reset();
    }
    ready_ = !character_.assetData.joints.empty()
        && !character_.assetData.animations.empty()
        && std::ranges::any_of(floor_.materials, [](const GpuMaterial& material) {
            return material.texture != nullptr;
        });
}

AssetSceneRenderer::GpuModel AssetSceneRenderer::LoadModel(
    ID3D11Device* const device,
    const std::filesystem::path& modelPath)
{
    GpuModel model;
    model.assetData = asset::LoadModelAsset(modelPath);
    if (model.assetData.vertices.empty() || model.assetData.indices.empty())
    {
        throw std::runtime_error{"runtime model contains no geometry"};
    }

    D3D11_BUFFER_DESC vertexBufferDescription{};
    vertexBufferDescription.ByteWidth = static_cast<UINT>(
        model.assetData.vertices.size() * sizeof(asset::Vertex));
    vertexBufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
    vertexBufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexData{};
    vertexData.pSysMem = model.assetData.vertices.data();
    RequireSuccess(
        device->CreateBuffer(
            &vertexBufferDescription,
            &vertexData,
            model.vertexBuffer.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer(runtime vertices)");

    D3D11_BUFFER_DESC indexBufferDescription{};
    indexBufferDescription.ByteWidth = static_cast<UINT>(
        model.assetData.indices.size() * sizeof(std::uint32_t));
    indexBufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
    indexBufferDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData{};
    indexData.pSysMem = model.assetData.indices.data();
    RequireSuccess(
        device->CreateBuffer(
            &indexBufferDescription,
            &indexData,
            model.indexBuffer.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer(runtime indices)");

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
        GpuMaterial gpuMaterial;
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
                "DirectX::CreateDDSTextureFromFile");
        }
        model.materials.push_back(std::move(gpuMaterial));
    }
    return model;
}

RenderStatistics AssetSceneRenderer::Render(
    ID3D11DeviceContext* const context,
    const AssetSceneFrame& frame) const
{
    using namespace DirectX;

    if (context == nullptr || frame.frameIndex == 0 || frame.aspectRatio <= 0.0F)
    {
        throw std::invalid_argument{
            "asset scene frame requires a context, one-based frame index, and positive aspect ratio"};
    }

    const bool stressMode = stressScene_.has_value();
    const double sceneSeconds = stressMode
        ? benchmark::StressSceneSeconds(frame.frameIndex)
        : frame.totalSeconds;
    XMMATRIX view;
    if (stressMode)
    {
        const benchmark::StressCamera camera =
            benchmark::SampleStressCamera(frame.frameIndex);
        view = XMMatrixLookAtLH(
            XMVectorSet(camera.eye.x, camera.eye.y, camera.eye.z, 1.0F),
            XMVectorSet(camera.target.x, camera.target.y, camera.target.z, 1.0F),
            XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F));
    }
    else
    {
        view = XMMatrixLookAtLH(
            XMVectorSet(5.5F, 5.0F, -7.0F, 1.0F),
            XMVectorSet(0.0F, 1.0F, 0.0F, 1.0F),
            XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F));
    }
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XM_PIDIV4,
        frame.aspectRatio,
        0.1F,
        stressMode ? 200.0F : 100.0F);
    const XMMATRIX viewProjection = view * projection;
    XMFLOAT4X4 viewProjectionStorage;
    XMStoreFloat4x4(&viewProjectionStorage, viewProjection);
    UpdateLights(context, sceneSeconds);

    const XMMATRIX floorBase = FloorWorld(floor_.minimumBounds, floor_.maximumBounds);
    const XMMATRIX characterBase =
        CharacterWorld(character_.minimumBounds, character_.maximumBounds);
    RenderStatistics statistics;

    if (!stressMode)
    {
        XMFLOAT4X4 floorWorldStorage;
        XMStoreFloat4x4(&floorWorldStorage, floorBase);
        Accumulate(
            statistics,
            RenderModel(
                context,
                floor_,
                &floorWorldStorage._11,
                &viewProjectionStorage._11,
                sceneSeconds));

        XMFLOAT4X4 characterWorldStorage;
        XMStoreFloat4x4(&characterWorldStorage, characterBase);
        Accumulate(
            statistics,
            RenderModel(
                context,
                character_,
                &characterWorldStorage._11,
                &viewProjectionStorage._11,
                sceneSeconds));
        return statistics;
    }

    for (const benchmark::SceneInstance& instance : stressScene_->staticInstances)
    {
        XMFLOAT4X4 worldStorage;
        XMStoreFloat4x4(&worldStorage, PlaceInstance(floorBase, instance));
        Accumulate(
            statistics,
            RenderModel(
                context,
                floor_,
                &worldStorage._11,
                &viewProjectionStorage._11,
                sceneSeconds));
    }

    const auto renderCharacters = [&](const std::span<const benchmark::SceneInstance> instances) {
        for (const benchmark::SceneInstance& instance : instances)
        {
            XMFLOAT4X4 worldStorage;
            XMStoreFloat4x4(&worldStorage, PlaceInstance(characterBase, instance));
            Accumulate(
                statistics,
                RenderModel(
                    context,
                    character_,
                    &worldStorage._11,
                    &viewProjectionStorage._11,
                    sceneSeconds + static_cast<double>(instance.animationPhaseSeconds)));
        }
    };
    renderCharacters(stressScene_->players);
    renderCharacters(stressScene_->ai);
    return statistics;
}

RenderStatistics AssetSceneRenderer::RenderModel(
    ID3D11DeviceContext* const context,
    const GpuModel& model,
    const float* const worldMatrix,
    const float* const viewProjectionMatrix,
    const double totalSeconds) const
{
    using namespace DirectX;

    const XMMATRIX world = LoadMatrix(worldMatrix);
    const XMMATRIX viewProjection = LoadMatrix(viewProjectionMatrix);

    SkinConstants skinConstants{};
    for (XMFLOAT4X4& matrix : skinConstants.boneMatrices)
    {
        XMStoreFloat4x4(&matrix, XMMatrixIdentity());
    }
    const std::span<const asset::Matrix4> palette =
        asset::SampleAnimationPalette(model.assetData, 0, totalSeconds);
    for (std::size_t jointIndex = 0; jointIndex < palette.size(); ++jointIndex)
    {
        std::memcpy(
            &skinConstants.boneMatrices[jointIndex],
            palette[jointIndex].elements.data(),
            sizeof(XMFLOAT4X4));
    }
    context->UpdateSubresource(skinConstantBuffer_.Get(), 0, nullptr, &skinConstants, 0, 0);

    constexpr UINT Stride = sizeof(asset::Vertex);
    constexpr UINT Offset = 0;
    ID3D11Buffer* vertexBuffer = model.vertexBuffer.Get();
    ID3D11Buffer* sceneBuffer = sceneConstantBuffer_.Get();
    ID3D11Buffer* skinBuffer = skinConstantBuffer_.Get();
    ID3D11SamplerState* sampler = sampler_.Get();
    context->IASetInputLayout(inputLayout_.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &Stride, &Offset);
    context->IASetIndexBuffer(model.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &sceneBuffer);
    context->VSSetConstantBuffers(1, 1, &skinBuffer);
    context->PSSetShader(pixelShader_.Get(), nullptr, 0);
    context->PSSetConstantBuffers(0, 1, &sceneBuffer);
    context->PSSetSamplers(0, 1, &sampler);

    RenderStatistics statistics;
    statistics.objectCount = 1;
    for (const asset::MeshPart& part : model.assetData.meshParts)
    {
        const GpuMaterial& material = model.materials.at(part.materialIndex);
        SceneConstants constants{};
        XMStoreFloat4x4(&constants.world, world);
        XMStoreFloat4x4(&constants.worldViewProjection, world * viewProjection);
        constants.baseColor = XMFLOAT4{
            material.baseColor.x,
            material.baseColor.y,
            material.baseColor.z,
            material.baseColor.w};
        constants.hasTexture = material.texture != nullptr ? 1U : 0U;
        context->UpdateSubresource(sceneConstantBuffer_.Get(), 0, nullptr, &constants, 0, 0);
        ID3D11ShaderResourceView* texture = material.texture.Get();
        context->PSSetShaderResources(0, 1, &texture);
        context->DrawIndexed(part.indexCount, part.firstIndex, 0);
        ++statistics.drawCalls;
        statistics.triangleCount += part.indexCount / 3U;
    }
    return statistics;
}

void AssetSceneRenderer::UpdateLights(
    ID3D11DeviceContext* const context,
    const double sceneSeconds) const
{
    LightingConstants constants{};
    if (stressScene_.has_value())
    {
        constants.pointLightCount = static_cast<std::uint32_t>(stressScene_->dynamicLights.size());
        for (std::size_t index = 0; index < stressScene_->dynamicLights.size(); ++index)
        {
            const benchmark::DynamicPointLight& light = stressScene_->dynamicLights[index];
            const float phase = light.phaseRadians + static_cast<float>(sceneSeconds) * 0.75F;
            constants.pointLights[index].positionAndRadius = DirectX::XMFLOAT4{
                light.position.x + std::cos(phase) * 1.5F,
                light.position.y + std::sin(phase * 0.5F) * 0.75F,
                light.position.z + std::sin(phase) * 1.5F,
                light.radius};
            constants.pointLights[index].colorAndIntensity = DirectX::XMFLOAT4{
                light.color.x,
                light.color.y,
                light.color.z,
                light.intensity};
        }
    }

    context->UpdateSubresource(lightingConstantBuffer_.Get(), 0, nullptr, &constants, 0, 0);
    ID3D11Buffer* lightingBuffer = lightingConstantBuffer_.Get();
    context->PSSetConstantBuffers(2, 1, &lightingBuffer);
}

bool AssetSceneRenderer::AssetSceneReady() const noexcept
{
    return ready_;
}
} // namespace dxa::engine
