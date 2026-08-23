#include <dxa/engine/HybridDeferredRenderer.hpp>

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
#include <utility>

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
    float roughness = 0.65F;
    std::array<float, 2> padding{};
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
    DirectX::XMFLOAT4X4 inverseViewProjection;
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
        throw std::runtime_error{
            "hybrid deferred shader compilation failed for "
            + shaderPath.string() + ": " + details};
    }
    return shader;
}

void CreateColorTarget(
    ID3D11Device* const device,
    const std::uint32_t width,
    const std::uint32_t height,
    const DXGI_FORMAT format,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& renderTarget,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& shaderResource)
{
    D3D11_TEXTURE2D_DESC description{};
    description.Width = width;
    description.Height = height;
    description.MipLevels = 1;
    description.ArraySize = 1;
    description.Format = format;
    description.SampleDesc.Count = 1;
    description.Usage = D3D11_USAGE_DEFAULT;
    description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    RequireSuccess(
        device->CreateTexture2D(&description, nullptr, texture.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateTexture2D(G-Buffer)");
    RequireSuccess(
        device->CreateRenderTargetView(
            texture.Get(), nullptr, renderTarget.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateRenderTargetView(G-Buffer)");
    RequireSuccess(
        device->CreateShaderResourceView(
            texture.Get(), nullptr, shaderResource.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateShaderResourceView(G-Buffer)");
}

void CreateDepthTarget(
    ID3D11Device* const device,
    const std::uint32_t width,
    const std::uint32_t height,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>& depthStencil,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& shaderResource)
{
    D3D11_TEXTURE2D_DESC textureDescription{};
    textureDescription.Width = width;
    textureDescription.Height = height;
    textureDescription.MipLevels = 1;
    textureDescription.ArraySize = 1;
    textureDescription.Format = DXGI_FORMAT_R24G8_TYPELESS;
    textureDescription.SampleDesc.Count = 1;
    textureDescription.Usage = D3D11_USAGE_DEFAULT;
    textureDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    RequireSuccess(
        device->CreateTexture2D(
            &textureDescription, nullptr, texture.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateTexture2D(sampleable depth)");

    D3D11_DEPTH_STENCIL_VIEW_DESC depthDescription{};
    depthDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDescription.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    RequireSuccess(
        device->CreateDepthStencilView(
            texture.Get(), &depthDescription, depthStencil.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateDepthStencilView(sampleable depth)");

    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceDescription{};
    shaderResourceDescription.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    shaderResourceDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shaderResourceDescription.Texture2D.MipLevels = 1;
    RequireSuccess(
        device->CreateShaderResourceView(
            texture.Get(),
            &shaderResourceDescription,
            shaderResource.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateShaderResourceView(sampleable depth)");
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
    const float height = std::max(maximumBounds.y - minimumBounds.y, 0.001F);
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
    total.shadowDrawCalls += value.shadowDrawCalls;
    total.gBufferDrawCalls += value.gBufferDrawCalls;
    total.lightingDrawCalls += value.lightingDrawCalls;
    total.transparentDrawCalls += value.transparentDrawCalls;
    total.visibleObjectCount += value.visibleObjectCount;
    total.culledObjectCount += value.culledObjectCount;
}
} // namespace

void HybridDeferredRenderer::Initialize(
    ID3D11Device* const device,
    const HybridDeferredConfig& config)
{
    if (device == nullptr
        || config.width == 0
        || config.height == 0
        || config.shadowMapSize == 0
        || config.shaderRoot.empty()
        || config.assetRoot.empty())
    {
        throw std::invalid_argument{
            "hybrid deferred renderer requires a device, dimensions, and asset paths"};
    }

    const auto geometryVertexBytecode = CompileShader(
        config.shaderRoot / L"hybrid_geometry.hlsl", "VSMain", "vs_5_0");
    const auto geometryPixelBytecode = CompileShader(
        config.shaderRoot / L"hybrid_geometry.hlsl", "PSMain", "ps_5_0");
    const auto lightingVertexBytecode = CompileShader(
        config.shaderRoot / L"hybrid_lighting.hlsl", "VSMain", "vs_5_0");
    const auto lightingPixelBytecode = CompileShader(
        config.shaderRoot / L"hybrid_lighting.hlsl", "PSMain", "ps_5_0");

    RequireSuccess(
        device->CreateVertexShader(
            geometryVertexBytecode->GetBufferPointer(),
            geometryVertexBytecode->GetBufferSize(),
            nullptr,
            geometryVertexShader_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateVertexShader(hybrid geometry)");
    RequireSuccess(
        device->CreatePixelShader(
            geometryPixelBytecode->GetBufferPointer(),
            geometryPixelBytecode->GetBufferSize(),
            nullptr,
            geometryPixelShader_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreatePixelShader(hybrid geometry)");
    RequireSuccess(
        device->CreateVertexShader(
            lightingVertexBytecode->GetBufferPointer(),
            lightingVertexBytecode->GetBufferSize(),
            nullptr,
            lightingVertexShader_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateVertexShader(deferred lighting)");
    RequireSuccess(
        device->CreatePixelShader(
            lightingPixelBytecode->GetBufferPointer(),
            lightingPixelBytecode->GetBufferSize(),
            nullptr,
            lightingPixelShader_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreatePixelShader(deferred lighting)");

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
            geometryVertexBytecode->GetBufferPointer(),
            geometryVertexBytecode->GetBufferSize(),
            geometryInputLayout_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateInputLayout(hybrid geometry)");

    const auto createConstantBuffer = [device](
                                          const UINT byteWidth,
                                          ID3D11Buffer** const buffer,
                                          const char* operation) {
        D3D11_BUFFER_DESC description{};
        description.ByteWidth = byteWidth;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        RequireSuccess(device->CreateBuffer(&description, nullptr, buffer), operation);
    };
    createConstantBuffer(
        sizeof(SceneConstants),
        sceneConstantBuffer_.ReleaseAndGetAddressOf(),
        "ID3D11Device::CreateBuffer(hybrid scene constants)");
    createConstantBuffer(
        sizeof(SkinConstants),
        skinConstantBuffer_.ReleaseAndGetAddressOf(),
        "ID3D11Device::CreateBuffer(hybrid skin constants)");
    createConstantBuffer(
        sizeof(LightingConstants),
        lightingConstantBuffer_.ReleaseAndGetAddressOf(),
        "ID3D11Device::CreateBuffer(deferred lighting constants)");

    D3D11_SAMPLER_DESC materialSamplerDescription{};
    materialSamplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    materialSamplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    materialSamplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    materialSamplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    materialSamplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
    RequireSuccess(
        device->CreateSamplerState(
            &materialSamplerDescription, materialSampler_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateSamplerState(hybrid material)");

    D3D11_SAMPLER_DESC gBufferSamplerDescription{};
    gBufferSamplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    gBufferSamplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    gBufferSamplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    gBufferSamplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    gBufferSamplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
    RequireSuccess(
        device->CreateSamplerState(
            &gBufferSamplerDescription, gBufferSampler_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateSamplerState(G-Buffer)");

    CreateColorTarget(
        device,
        config.width,
        config.height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        albedoRoughnessTexture_,
        albedoRoughnessRenderTarget_,
        albedoRoughnessShaderResource_);
    CreateColorTarget(
        device,
        config.width,
        config.height,
        DXGI_FORMAT_R16G16_FLOAT,
        normalTexture_,
        normalRenderTarget_,
        normalShaderResource_);
    CreateDepthTarget(
        device,
        config.width,
        config.height,
        depthTexture_,
        depthStencilView_,
        depthShaderResource_);

    viewport_.Width = static_cast<float>(config.width);
    viewport_.Height = static_cast<float>(config.height);
    viewport_.MinDepth = 0.0F;
    viewport_.MaxDepth = 1.0F;

    character_ = LoadModel(
        device, config.assetRoot / L"characters" / L"cyber-runner.dxam");
    floor_ = LoadModel(
        device, config.assetRoot / L"environment" / L"prototype-floor.dxam");
    stressScene_ = benchmark::GenerateStressScene(config.stressSceneSeed);
}

HybridDeferredRenderer::GpuModel HybridDeferredRenderer::LoadModel(
    ID3D11Device* const device,
    const std::filesystem::path& modelPath)
{
    GpuModel model;
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
            &vertexDescription, &vertexData, model.vertexBuffer.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer(hybrid vertices)");

    D3D11_BUFFER_DESC indexDescription{};
    indexDescription.ByteWidth = static_cast<UINT>(
        model.assetData.indices.size() * sizeof(std::uint32_t));
    indexDescription.Usage = D3D11_USAGE_IMMUTABLE;
    indexDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData{};
    indexData.pSysMem = model.assetData.indices.data();
    RequireSuccess(
        device->CreateBuffer(
            &indexDescription, &indexData, model.indexBuffer.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer(hybrid indices)");

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
                "DirectX::CreateDDSTextureFromFile(hybrid)");
        }
        model.materials.push_back(std::move(gpuMaterial));
    }
    return model;
}

RenderStatistics HybridDeferredRenderer::Render(
    ID3D11DeviceContext* const context,
    ID3D11RenderTargetView* const backBufferRenderTarget,
    const AssetSceneFrame& frame) const
{
    using namespace DirectX;

    if (context == nullptr
        || backBufferRenderTarget == nullptr
        || frame.frameIndex == 0
        || frame.aspectRatio <= 0.0F)
    {
        throw std::invalid_argument{
            "hybrid frame requires a context, back buffer, one-based frame, and aspect ratio"};
    }

    const double sceneSeconds = benchmark::StressSceneSeconds(frame.frameIndex);
    const benchmark::StressCamera camera = benchmark::SampleStressCamera(frame.frameIndex);
    const XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(camera.eye.x, camera.eye.y, camera.eye.z, 1.0F),
        XMVectorSet(camera.target.x, camera.target.y, camera.target.z, 1.0F),
        XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F));
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XM_PIDIV4,
        frame.aspectRatio,
        0.1F,
        200.0F);
    const XMMATRIX viewProjection = view * projection;
    XMFLOAT4X4 viewProjectionStorage;
    XMStoreFloat4x4(&viewProjectionStorage, viewProjection);

    ID3D11RenderTargetView* geometryTargets[]{
        albedoRoughnessRenderTarget_.Get(),
        normalRenderTarget_.Get()};
    context->OMSetRenderTargets(
        static_cast<UINT>(std::size(geometryTargets)),
        geometryTargets,
        depthStencilView_.Get());
    context->RSSetViewports(1, &viewport_);
    constexpr std::array AlbedoClear{0.0F, 0.0F, 0.0F, 1.0F};
    constexpr std::array NormalClear{0.0F, 0.0F, 0.0F, 0.0F};
    context->ClearRenderTargetView(albedoRoughnessRenderTarget_.Get(), AlbedoClear.data());
    context->ClearRenderTargetView(normalRenderTarget_.Get(), NormalClear.data());
    context->ClearDepthStencilView(
        depthStencilView_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0F, 0);

    const XMMATRIX floorBase = FloorWorld(floor_.minimumBounds, floor_.maximumBounds);
    const XMMATRIX characterBase =
        CharacterWorld(character_.minimumBounds, character_.maximumBounds);
    RenderStatistics statistics;

    for (const benchmark::SceneInstance& instance : stressScene_.staticInstances)
    {
        XMFLOAT4X4 worldStorage;
        XMStoreFloat4x4(&worldStorage, PlaceInstance(floorBase, instance));
        Accumulate(
            statistics,
            RenderGeometryModel(
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
                RenderGeometryModel(
                    context,
                    character_,
                    &worldStorage._11,
                    &viewProjectionStorage._11,
                    sceneSeconds + static_cast<double>(instance.animationPhaseSeconds)));
        }
    };
    renderCharacters(stressScene_.players);
    renderCharacters(stressScene_.ai);

    XMVECTOR determinant;
    const XMMATRIX inverseViewProjection = XMMatrixInverse(&determinant, viewProjection);
    XMFLOAT4X4 inverseViewProjectionStorage;
    XMStoreFloat4x4(&inverseViewProjectionStorage, inverseViewProjection);
    UpdateLightingConstants(
        context, &inverseViewProjectionStorage._11, sceneSeconds);

    context->OMSetRenderTargets(1, &backBufferRenderTarget, nullptr);
    context->RSSetViewports(1, &viewport_);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(lightingVertexShader_.Get(), nullptr, 0);
    context->PSSetShader(lightingPixelShader_.Get(), nullptr, 0);
    ID3D11Buffer* lightingBuffer = lightingConstantBuffer_.Get();
    context->PSSetConstantBuffers(0, 1, &lightingBuffer);
    ID3D11ShaderResourceView* gBufferResources[]{
        albedoRoughnessShaderResource_.Get(),
        normalShaderResource_.Get(),
        depthShaderResource_.Get()};
    context->PSSetShaderResources(
        0,
        static_cast<UINT>(std::size(gBufferResources)),
        gBufferResources);
    ID3D11SamplerState* gBufferSampler = gBufferSampler_.Get();
    context->PSSetSamplers(0, 1, &gBufferSampler);
    context->Draw(3, 0);
    ++statistics.drawCalls;
    ++statistics.lightingDrawCalls;
    ++statistics.triangleCount;

    constexpr std::array<ID3D11ShaderResourceView*, 3> NullResources{};
    context->PSSetShaderResources(
        0,
        static_cast<UINT>(NullResources.size()),
        NullResources.data());
    return statistics;
}

RenderStatistics HybridDeferredRenderer::RenderGeometryModel(
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
    ID3D11SamplerState* materialSampler = materialSampler_.Get();
    context->IASetInputLayout(geometryInputLayout_.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &Stride, &Offset);
    context->IASetIndexBuffer(model.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->VSSetShader(geometryVertexShader_.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &sceneBuffer);
    context->VSSetConstantBuffers(1, 1, &skinBuffer);
    context->PSSetShader(geometryPixelShader_.Get(), nullptr, 0);
    context->PSSetConstantBuffers(0, 1, &sceneBuffer);
    context->PSSetSamplers(0, 1, &materialSampler);

    RenderStatistics statistics;
    statistics.objectCount = 1;
    statistics.visibleObjectCount = 1;
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
        ++statistics.gBufferDrawCalls;
        statistics.triangleCount += part.indexCount / 3U;
    }
    return statistics;
}

void HybridDeferredRenderer::UpdateLightingConstants(
    ID3D11DeviceContext* const context,
    const float* const inverseViewProjectionMatrix,
    const double sceneSeconds) const
{
    LightingConstants constants{};
    std::memcpy(
        &constants.inverseViewProjection,
        inverseViewProjectionMatrix,
        sizeof(constants.inverseViewProjection));
    constants.pointLightCount = static_cast<std::uint32_t>(stressScene_.dynamicLights.size());
    for (std::size_t index = 0; index < stressScene_.dynamicLights.size(); ++index)
    {
        const benchmark::DynamicPointLight& light = stressScene_.dynamicLights[index];
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
    context->UpdateSubresource(lightingConstantBuffer_.Get(), 0, nullptr, &constants, 0, 0);
}
} // namespace dxa::engine
