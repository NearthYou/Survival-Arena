#include <dxa/engine/HybridDeferredRenderer.hpp>

#include <dxa/engine/assets/AnimationPlayback.hpp>
#include <dxa/engine/benchmark/PerspectiveFrustum.hpp>

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
#include <numbers>
#include <sstream>
#include <span>
#include <stdexcept>
#include <utility>

namespace dxa::engine
{
namespace
{
inline constexpr std::uint32_t StaticInstanceCapacity =
    static_cast<std::uint32_t>(benchmark::StaticInstanceCount);
inline constexpr std::uint32_t MarkerInstanceCount = 64;

struct InstanceTransform
{
    DirectX::XMFLOAT4X4 world;
};

static_assert(sizeof(InstanceTransform) == sizeof(float) * 16);

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
    DirectX::XMFLOAT4X4 lightViewProjection;
    std::array<GpuPointLight, benchmark::DynamicLightCount> pointLights;
    std::uint32_t pointLightCount = 0;
    float shadowTexelSize = 0.0F;
    std::array<float, 2> padding{};
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

void CreateShadowTarget(
    ID3D11Device* const device,
    const std::uint32_t size,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>& depthStencil,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& shaderResource)
{
    D3D11_TEXTURE2D_DESC textureDescription{};
    textureDescription.Width = size;
    textureDescription.Height = size;
    textureDescription.MipLevels = 1;
    textureDescription.ArraySize = 1;
    textureDescription.Format = DXGI_FORMAT_R32_TYPELESS;
    textureDescription.SampleDesc.Count = 1;
    textureDescription.Usage = D3D11_USAGE_DEFAULT;
    textureDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    RequireSuccess(
        device->CreateTexture2D(
            &textureDescription, nullptr, texture.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateTexture2D(shadow map)");

    D3D11_DEPTH_STENCIL_VIEW_DESC depthDescription{};
    depthDescription.Format = DXGI_FORMAT_D32_FLOAT;
    depthDescription.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    RequireSuccess(
        device->CreateDepthStencilView(
            texture.Get(), &depthDescription, depthStencil.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateDepthStencilView(shadow map)");

    D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceDescription{};
    shaderResourceDescription.Format = DXGI_FORMAT_R32_FLOAT;
    shaderResourceDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shaderResourceDescription.Texture2D.MipLevels = 1;
    RequireSuccess(
        device->CreateShaderResourceView(
            texture.Get(),
            &shaderResourceDescription,
            shaderResource.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateShaderResourceView(shadow map)");
}

void CreateInstanceBuffer(
    ID3D11Device* const device,
    const std::uint32_t capacity,
    Microsoft::WRL::ComPtr<ID3D11Buffer>& buffer,
    const char* operation)
{
    D3D11_BUFFER_DESC description{};
    description.ByteWidth = static_cast<UINT>(sizeof(InstanceTransform) * capacity);
    description.Usage = D3D11_USAGE_DYNAMIC;
    description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    RequireSuccess(
        device->CreateBuffer(&description, nullptr, buffer.ReleaseAndGetAddressOf()),
        operation);
}

void UploadInstanceTransforms(
    ID3D11DeviceContext* const context,
    ID3D11Buffer* const buffer,
    const std::span<const InstanceTransform> transforms,
    const std::uint32_t capacity)
{
    if (context == nullptr || buffer == nullptr || transforms.size() > capacity)
    {
        throw std::invalid_argument{"instance upload exceeds its reusable buffer capacity"};
    }
    if (transforms.empty())
    {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    RequireSuccess(
        context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
        "ID3D11DeviceContext::Map(instance transforms)");
    std::memcpy(
        mapped.pData,
        transforms.data(),
        transforms.size_bytes());
    context->Unmap(buffer, 0);
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
    const auto geometryInstancedVertexBytecode = CompileShader(
        config.shaderRoot / L"hybrid_geometry.hlsl", "VSInstanced", "vs_5_0");
    const auto geometryPixelBytecode = CompileShader(
        config.shaderRoot / L"hybrid_geometry.hlsl", "PSMain", "ps_5_0");
    const auto lightingVertexBytecode = CompileShader(
        config.shaderRoot / L"hybrid_lighting.hlsl", "VSMain", "vs_5_0");
    const auto lightingPixelBytecode = CompileShader(
        config.shaderRoot / L"hybrid_lighting.hlsl", "PSMain", "ps_5_0");
    const auto shadowVertexBytecode = CompileShader(
        config.shaderRoot / L"hybrid_shadow.hlsl", "VSMain", "vs_5_0");
    const auto shadowInstancedVertexBytecode = CompileShader(
        config.shaderRoot / L"hybrid_shadow.hlsl", "VSInstanced", "vs_5_0");
    const auto transparentVertexBytecode = CompileShader(
        config.shaderRoot / L"hybrid_transparent.hlsl", "VSMain", "vs_5_0");
    const auto transparentPixelBytecode = CompileShader(
        config.shaderRoot / L"hybrid_transparent.hlsl", "PSMain", "ps_5_0");

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
            geometryInstancedVertexBytecode->GetBufferPointer(),
            geometryInstancedVertexBytecode->GetBufferSize(),
            nullptr,
            geometryInstancedVertexShader_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateVertexShader(instanced geometry)");
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
    RequireSuccess(
        device->CreateVertexShader(
            shadowVertexBytecode->GetBufferPointer(),
            shadowVertexBytecode->GetBufferSize(),
            nullptr,
            shadowVertexShader_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateVertexShader(shadow)");
    RequireSuccess(
        device->CreateVertexShader(
            shadowInstancedVertexBytecode->GetBufferPointer(),
            shadowInstancedVertexBytecode->GetBufferSize(),
            nullptr,
            shadowInstancedVertexShader_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateVertexShader(instanced shadow)");
    RequireSuccess(
        device->CreateVertexShader(
            transparentVertexBytecode->GetBufferPointer(),
            transparentVertexBytecode->GetBufferSize(),
            nullptr,
            transparentVertexShader_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateVertexShader(transparent markers)");
    RequireSuccess(
        device->CreatePixelShader(
            transparentPixelBytecode->GetBufferPointer(),
            transparentPixelBytecode->GetBufferSize(),
            nullptr,
            transparentPixelShader_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreatePixelShader(transparent markers)");

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

    constexpr D3D11_INPUT_ELEMENT_DESC InstancedInputElements[]{
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(asset::Vertex, position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(asset::Vertex, normal)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(asset::Vertex, texcoord)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0, static_cast<UINT>(offsetof(asset::Vertex, jointIndices)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(asset::Vertex, jointWeights)), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"INSTANCEWORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCEWORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCEWORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"INSTANCEWORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1}};
    RequireSuccess(
        device->CreateInputLayout(
            InstancedInputElements,
            static_cast<UINT>(std::size(InstancedInputElements)),
            geometryInstancedVertexBytecode->GetBufferPointer(),
            geometryInstancedVertexBytecode->GetBufferSize(),
            instancedInputLayout_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateInputLayout(instanced geometry)");

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

    D3D11_SAMPLER_DESC shadowSamplerDescription{};
    shadowSamplerDescription.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    shadowSamplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDescription.BorderColor[0] = 1.0F;
    shadowSamplerDescription.BorderColor[1] = 1.0F;
    shadowSamplerDescription.BorderColor[2] = 1.0F;
    shadowSamplerDescription.BorderColor[3] = 1.0F;
    shadowSamplerDescription.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    shadowSamplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
    RequireSuccess(
        device->CreateSamplerState(
            &shadowSamplerDescription, shadowSampler_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateSamplerState(shadow comparison)");

    D3D11_RASTERIZER_DESC shadowRasterizerDescription{};
    shadowRasterizerDescription.FillMode = D3D11_FILL_SOLID;
    shadowRasterizerDescription.CullMode = D3D11_CULL_BACK;
    shadowRasterizerDescription.DepthClipEnable = TRUE;
    shadowRasterizerDescription.DepthBias = 1200;
    shadowRasterizerDescription.SlopeScaledDepthBias = 1.5F;
    shadowRasterizerDescription.DepthBiasClamp = 0.01F;
    RequireSuccess(
        device->CreateRasterizerState(
            &shadowRasterizerDescription,
            shadowRasterizerState_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateRasterizerState(shadow bias)");

    D3D11_BLEND_DESC transparentBlendDescription{};
    transparentBlendDescription.RenderTarget[0].BlendEnable = TRUE;
    transparentBlendDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    transparentBlendDescription.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    transparentBlendDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    transparentBlendDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    transparentBlendDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    transparentBlendDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    transparentBlendDescription.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALL;
    RequireSuccess(
        device->CreateBlendState(
            &transparentBlendDescription,
            transparentBlendState_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBlendState(transparent markers)");

    D3D11_DEPTH_STENCIL_DESC transparentDepthDescription{};
    transparentDepthDescription.DepthEnable = TRUE;
    transparentDepthDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    transparentDepthDescription.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    RequireSuccess(
        device->CreateDepthStencilState(
            &transparentDepthDescription,
            transparentDepthState_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateDepthStencilState(transparent markers)");

    CreateInstanceBuffer(
        device,
        StaticInstanceCapacity,
        staticInstanceBuffer_,
        "ID3D11Device::CreateBuffer(static instances)");
    CreateInstanceBuffer(
        device,
        MarkerInstanceCount,
        markerInstanceBuffer_,
        "ID3D11Device::CreateBuffer(marker instances)");

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
    CreateShadowTarget(
        device,
        config.shadowMapSize,
        shadowTexture_,
        shadowDepthStencilView_,
        shadowShaderResource_);

    viewport_.Width = static_cast<float>(config.width);
    viewport_.Height = static_cast<float>(config.height);
    viewport_.MinDepth = 0.0F;
    viewport_.MaxDepth = 1.0F;

    shadowViewport_.Width = static_cast<float>(config.shadowMapSize);
    shadowViewport_.Height = static_cast<float>(config.shadowMapSize);
    shadowViewport_.MinDepth = 0.0F;
    shadowViewport_.MaxDepth = 1.0F;

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

    const XMVECTOR sunDirection = XMVector3Normalize(
        XMVectorSet(-0.45F, 0.8F, -0.35F, 0.0F));
    const XMMATRIX lightView = XMMatrixLookAtLH(
        XMVectorScale(sunDirection, 80.0F),
        XMVectorZero(),
        XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F));
    const XMMATRIX lightProjection = XMMatrixOrthographicLH(
        110.0F,
        110.0F,
        0.1F,
        200.0F);
    const XMMATRIX lightViewProjection = lightView * lightProjection;
    XMFLOAT4X4 lightViewProjectionStorage;
    XMStoreFloat4x4(&lightViewProjectionStorage, lightViewProjection);

    const XMMATRIX floorBase = FloorWorld(floor_.minimumBounds, floor_.maximumBounds);
    const XMMATRIX characterBase =
        CharacterWorld(character_.minimumBounds, character_.maximumBounds);
    RenderStatistics statistics;

    const benchmark::PerspectiveFrustum frustum = benchmark::BuildPerspectiveFrustum(
        camera,
        std::numbers::pi_v<float> / 4.0F,
        frame.aspectRatio,
        0.1F,
        200.0F);
    std::vector<InstanceTransform> allStaticTransforms;
    std::vector<InstanceTransform> visibleStaticTransforms;
    allStaticTransforms.reserve(benchmark::StaticInstanceCount);
    visibleStaticTransforms.reserve(benchmark::StaticInstanceCount);
    for (const benchmark::SceneInstance& instance : stressScene_.staticInstances)
    {
        InstanceTransform transform;
        XMStoreFloat4x4(&transform.world, PlaceInstance(floorBase, instance));
        allStaticTransforms.push_back(transform);
        if (frustum.IntersectsSphere(benchmark::BoundingSphere{
                benchmark::SceneVector3{
                    instance.position.x,
                    instance.position.y + 0.5F,
                    instance.position.z},
                2.5F * instance.uniformScale / 0.25F}))
        {
            visibleStaticTransforms.push_back(transform);
        }
    }
    const std::uint32_t visibleStaticCount =
        static_cast<std::uint32_t>(visibleStaticTransforms.size());
    statistics.objectCount += StaticInstanceCapacity;
    statistics.visibleObjectCount += visibleStaticCount;
    statistics.culledObjectCount += StaticInstanceCapacity - visibleStaticCount;

    UploadInstanceTransforms(
        context,
        staticInstanceBuffer_.Get(),
        allStaticTransforms,
        StaticInstanceCapacity);

    constexpr std::array<ID3D11ShaderResourceView*, 1> NullShadowResource{};
    context->PSSetShaderResources(3, 1, NullShadowResource.data());
    context->OMSetRenderTargets(0, nullptr, shadowDepthStencilView_.Get());
    context->RSSetViewports(1, &shadowViewport_);
    context->RSSetState(shadowRasterizerState_.Get());
    context->ClearDepthStencilView(
        shadowDepthStencilView_.Get(), D3D11_CLEAR_DEPTH, 1.0F, 0);
    Accumulate(
        statistics,
        RenderShadowInstances(
            context,
            floor_,
            staticInstanceBuffer_.Get(),
            StaticInstanceCapacity,
            &lightViewProjectionStorage._11));
    const auto renderShadowCharacters = [&](
        const std::span<const benchmark::SceneInstance> instances) {
        for (const benchmark::SceneInstance& instance : instances)
        {
            XMFLOAT4X4 worldStorage;
            XMStoreFloat4x4(&worldStorage, PlaceInstance(characterBase, instance));
            Accumulate(
                statistics,
                RenderShadowModel(
                    context,
                    character_,
                    &worldStorage._11,
                    &lightViewProjectionStorage._11,
                    sceneSeconds + static_cast<double>(instance.animationPhaseSeconds)));
        }
    };
    renderShadowCharacters(stressScene_.players);
    renderShadowCharacters(stressScene_.ai);
    context->RSSetState(nullptr);

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

    UploadInstanceTransforms(
        context,
        staticInstanceBuffer_.Get(),
        visibleStaticTransforms,
        StaticInstanceCapacity);
    if (visibleStaticCount > 0)
    {
        Accumulate(
            statistics,
            RenderGeometryInstances(
                context,
                floor_,
                staticInstanceBuffer_.Get(),
                visibleStaticCount,
                &viewProjectionStorage._11));
    }

    const auto renderCharacters = [&](const std::span<const benchmark::SceneInstance> instances) {
        for (const benchmark::SceneInstance& instance : instances)
        {
            if (!frustum.IntersectsSphere(benchmark::BoundingSphere{
                    benchmark::SceneVector3{
                        instance.position.x,
                        instance.position.y + 1.1F,
                        instance.position.z},
                    1.6F * instance.uniformScale}))
            {
                ++statistics.objectCount;
                ++statistics.culledObjectCount;
                continue;
            }
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
        context,
        &inverseViewProjectionStorage._11,
        &lightViewProjectionStorage._11,
        sceneSeconds);

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
        depthShaderResource_.Get(),
        shadowShaderResource_.Get()};
    context->PSSetShaderResources(
        0,
        static_cast<UINT>(std::size(gBufferResources)),
        gBufferResources);
    ID3D11SamplerState* gBufferSampler = gBufferSampler_.Get();
    context->PSSetSamplers(0, 1, &gBufferSampler);
    ID3D11SamplerState* shadowSampler = shadowSampler_.Get();
    context->PSSetSamplers(1, 1, &shadowSampler);
    context->Draw(3, 0);
    ++statistics.drawCalls;
    ++statistics.lightingDrawCalls;
    ++statistics.triangleCount;

    constexpr std::array<ID3D11ShaderResourceView*, 4> NullResources{};
    context->PSSetShaderResources(
        0,
        static_cast<UINT>(NullResources.size()),
        NullResources.data());

    std::vector<InstanceTransform> markerTransforms;
    markerTransforms.reserve(MarkerInstanceCount);
    const float zoneRadius = 42.0F
        - std::fmod(static_cast<float>(sceneSeconds) * 2.0F, 28.0F);
    constexpr float TwoPi = std::numbers::pi_v<float> * 2.0F;
    for (std::uint32_t index = 0; index < MarkerInstanceCount; ++index)
    {
        const float angle = static_cast<float>(index)
            / static_cast<float>(MarkerInstanceCount)
            * TwoPi;
        InstanceTransform transform;
        XMStoreFloat4x4(
            &transform.world,
            floorBase
                * XMMatrixScaling(0.07F, 0.20F, 0.07F)
                * XMMatrixRotationY(-angle)
                * XMMatrixTranslation(
                    std::cos(angle) * zoneRadius,
                    0.08F,
                    std::sin(angle) * zoneRadius));
        markerTransforms.push_back(transform);
    }
    UploadInstanceTransforms(
        context,
        markerInstanceBuffer_.Get(),
        markerTransforms,
        MarkerInstanceCount);
    Accumulate(
        statistics,
        RenderTransparentMarkers(
            context,
            backBufferRenderTarget,
            markerInstanceBuffer_.Get(),
            MarkerInstanceCount,
            &viewProjectionStorage._11));
    return statistics;
}

RenderStatistics HybridDeferredRenderer::RenderShadowModel(
    ID3D11DeviceContext* const context,
    const GpuModel& model,
    const float* const worldMatrix,
    const float* const lightViewProjectionMatrix,
    const double totalSeconds) const
{
    using namespace DirectX;

    const XMMATRIX world = LoadMatrix(worldMatrix);
    const XMMATRIX lightViewProjection = LoadMatrix(lightViewProjectionMatrix);
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

    SceneConstants constants{};
    XMStoreFloat4x4(
        &constants.worldViewProjection,
        world * lightViewProjection);
    context->UpdateSubresource(sceneConstantBuffer_.Get(), 0, nullptr, &constants, 0, 0);

    constexpr UINT Stride = sizeof(asset::Vertex);
    constexpr UINT Offset = 0;
    ID3D11Buffer* vertexBuffer = model.vertexBuffer.Get();
    ID3D11Buffer* sceneBuffer = sceneConstantBuffer_.Get();
    ID3D11Buffer* skinBuffer = skinConstantBuffer_.Get();
    context->IASetInputLayout(geometryInputLayout_.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &Stride, &Offset);
    context->IASetIndexBuffer(model.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->VSSetShader(shadowVertexShader_.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &sceneBuffer);
    context->VSSetConstantBuffers(1, 1, &skinBuffer);
    context->PSSetShader(nullptr, nullptr, 0);

    const UINT indexCount = static_cast<UINT>(model.assetData.indices.size());
    context->DrawIndexed(indexCount, 0, 0);
    RenderStatistics statistics;
    statistics.drawCalls = 1;
    statistics.shadowDrawCalls = 1;
    statistics.triangleCount = indexCount / 3U;
    return statistics;
}

RenderStatistics HybridDeferredRenderer::RenderShadowInstances(
    ID3D11DeviceContext* const context,
    const GpuModel& model,
    ID3D11Buffer* const instanceBuffer,
    const std::uint32_t instanceCount,
    const float* const lightViewProjectionMatrix) const
{
    using namespace DirectX;

    if (context == nullptr || instanceBuffer == nullptr || instanceCount == 0)
    {
        throw std::invalid_argument{"instanced shadow draw requires instances"};
    }

    SceneConstants constants{};
    XMStoreFloat4x4(
        &constants.worldViewProjection,
        LoadMatrix(lightViewProjectionMatrix));
    context->UpdateSubresource(sceneConstantBuffer_.Get(), 0, nullptr, &constants, 0, 0);

    ID3D11Buffer* vertexBuffers[]{model.vertexBuffer.Get(), instanceBuffer};
    constexpr UINT Strides[]{sizeof(asset::Vertex), sizeof(InstanceTransform)};
    constexpr UINT Offsets[]{0, 0};
    ID3D11Buffer* sceneBuffer = sceneConstantBuffer_.Get();
    context->IASetInputLayout(instancedInputLayout_.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 2, vertexBuffers, Strides, Offsets);
    context->IASetIndexBuffer(model.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->VSSetShader(shadowInstancedVertexShader_.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &sceneBuffer);
    context->PSSetShader(nullptr, nullptr, 0);

    const UINT indexCount = static_cast<UINT>(model.assetData.indices.size());
    context->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
    RenderStatistics statistics;
    statistics.drawCalls = 1;
    statistics.shadowDrawCalls = 1;
    statistics.triangleCount = static_cast<std::uint64_t>(indexCount / 3U)
        * instanceCount;
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

RenderStatistics HybridDeferredRenderer::RenderGeometryInstances(
    ID3D11DeviceContext* const context,
    const GpuModel& model,
    ID3D11Buffer* const instanceBuffer,
    const std::uint32_t instanceCount,
    const float* const viewProjectionMatrix) const
{
    using namespace DirectX;

    if (context == nullptr || instanceBuffer == nullptr || instanceCount == 0)
    {
        throw std::invalid_argument{"instanced G-Buffer draw requires instances"};
    }

    SkinConstants skinConstants{};
    for (XMFLOAT4X4& matrix : skinConstants.boneMatrices)
    {
        XMStoreFloat4x4(&matrix, XMMatrixIdentity());
    }
    context->UpdateSubresource(skinConstantBuffer_.Get(), 0, nullptr, &skinConstants, 0, 0);

    ID3D11Buffer* vertexBuffers[]{model.vertexBuffer.Get(), instanceBuffer};
    constexpr UINT Strides[]{sizeof(asset::Vertex), sizeof(InstanceTransform)};
    constexpr UINT Offsets[]{0, 0};
    ID3D11Buffer* sceneBuffer = sceneConstantBuffer_.Get();
    ID3D11Buffer* skinBuffer = skinConstantBuffer_.Get();
    ID3D11SamplerState* materialSampler = materialSampler_.Get();
    context->IASetInputLayout(instancedInputLayout_.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 2, vertexBuffers, Strides, Offsets);
    context->IASetIndexBuffer(model.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->VSSetShader(geometryInstancedVertexShader_.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &sceneBuffer);
    context->VSSetConstantBuffers(1, 1, &skinBuffer);
    context->PSSetShader(geometryPixelShader_.Get(), nullptr, 0);
    context->PSSetConstantBuffers(0, 1, &sceneBuffer);
    context->PSSetSamplers(0, 1, &materialSampler);

    RenderStatistics statistics;
    for (const asset::MeshPart& part : model.assetData.meshParts)
    {
        const GpuMaterial& material = model.materials.at(part.materialIndex);
        SceneConstants constants{};
        XMStoreFloat4x4(
            &constants.worldViewProjection,
            LoadMatrix(viewProjectionMatrix));
        XMStoreFloat4x4(&constants.world, XMMatrixIdentity());
        constants.baseColor = XMFLOAT4{
            material.baseColor.x,
            material.baseColor.y,
            material.baseColor.z,
            material.baseColor.w};
        constants.hasTexture = material.texture != nullptr ? 1U : 0U;
        context->UpdateSubresource(sceneConstantBuffer_.Get(), 0, nullptr, &constants, 0, 0);
        ID3D11ShaderResourceView* texture = material.texture.Get();
        context->PSSetShaderResources(0, 1, &texture);
        context->DrawIndexedInstanced(
            part.indexCount,
            instanceCount,
            part.firstIndex,
            0,
            0);
        ++statistics.drawCalls;
        ++statistics.gBufferDrawCalls;
        statistics.triangleCount += static_cast<std::uint64_t>(part.indexCount / 3U)
            * instanceCount;
    }
    return statistics;
}

RenderStatistics HybridDeferredRenderer::RenderTransparentMarkers(
    ID3D11DeviceContext* const context,
    ID3D11RenderTargetView* const backBufferRenderTarget,
    ID3D11Buffer* const instanceBuffer,
    const std::uint32_t instanceCount,
    const float* const viewProjectionMatrix) const
{
    using namespace DirectX;

    if (context == nullptr
        || backBufferRenderTarget == nullptr
        || instanceBuffer == nullptr
        || instanceCount == 0)
    {
        throw std::invalid_argument{"transparent marker pass requires instances"};
    }

    SceneConstants constants{};
    XMStoreFloat4x4(
        &constants.worldViewProjection,
        LoadMatrix(viewProjectionMatrix));
    context->UpdateSubresource(sceneConstantBuffer_.Get(), 0, nullptr, &constants, 0, 0);

    ID3D11Buffer* vertexBuffers[]{floor_.vertexBuffer.Get(), instanceBuffer};
    constexpr UINT Strides[]{sizeof(asset::Vertex), sizeof(InstanceTransform)};
    constexpr UINT Offsets[]{0, 0};
    ID3D11Buffer* sceneBuffer = sceneConstantBuffer_.Get();
    context->OMSetRenderTargets(1, &backBufferRenderTarget, depthStencilView_.Get());
    context->OMSetBlendState(transparentBlendState_.Get(), nullptr, 0xFFFFFFFFU);
    context->OMSetDepthStencilState(transparentDepthState_.Get(), 0);
    context->RSSetViewports(1, &viewport_);
    context->IASetInputLayout(instancedInputLayout_.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 2, vertexBuffers, Strides, Offsets);
    context->IASetIndexBuffer(floor_.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->VSSetShader(transparentVertexShader_.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &sceneBuffer);
    context->PSSetShader(transparentPixelShader_.Get(), nullptr, 0);

    const UINT indexCount = static_cast<UINT>(floor_.assetData.indices.size());
    context->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFFU);
    context->OMSetDepthStencilState(nullptr, 0);

    RenderStatistics statistics;
    statistics.drawCalls = 1;
    statistics.transparentDrawCalls = 1;
    statistics.triangleCount = static_cast<std::uint64_t>(indexCount / 3U)
        * instanceCount;
    return statistics;
}

void HybridDeferredRenderer::UpdateLightingConstants(
    ID3D11DeviceContext* const context,
    const float* const inverseViewProjectionMatrix,
    const float* const lightViewProjectionMatrix,
    const double sceneSeconds) const
{
    LightingConstants constants{};
    std::memcpy(
        &constants.inverseViewProjection,
        inverseViewProjectionMatrix,
        sizeof(constants.inverseViewProjection));
    std::memcpy(
        &constants.lightViewProjection,
        lightViewProjectionMatrix,
        sizeof(constants.lightViewProjection));
    constants.shadowTexelSize = 1.0F / shadowViewport_.Width;
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

bool HybridDeferredRenderer::ShadowMapReady() const noexcept
{
    return shadowVertexShader_ != nullptr
        && shadowTexture_ != nullptr
        && shadowDepthStencilView_ != nullptr
        && shadowShaderResource_ != nullptr
        && shadowSampler_ != nullptr
        && shadowRasterizerState_ != nullptr;
}
} // namespace dxa::engine
