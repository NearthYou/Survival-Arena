#include <dxa/engine/HybridDeferredRenderer.hpp>

#include <dxa/engine/assets/AnimationPlayback.hpp>

#include "HybridDeferredInternal.hpp"

#include <DirectXMath.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

namespace dxa::engine
{
namespace
{
using detail::HybridLightingConstants;
using detail::HybridSceneConstants;
using detail::HybridSkinConstants;
using detail::InstanceTransform;

[[nodiscard]] DirectX::XMMATRIX LoadMatrix(const float* elements)
{
    return DirectX::XMLoadFloat4x4(
        reinterpret_cast<const DirectX::XMFLOAT4X4*>(elements));
}

[[nodiscard]] HybridSkinConstants MakeIdentitySkinConstants()
{
    HybridSkinConstants constants{};
    for (DirectX::XMFLOAT4X4& matrix : constants.boneMatrices)
    {
        DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixIdentity());
    }
    return constants;
}

[[nodiscard]] HybridSkinConstants MakeSkinConstants(
    const detail::GpuSceneModel& model,
    const double totalSeconds)
{
    HybridSkinConstants constants = MakeIdentitySkinConstants();
    const std::span<const asset::Matrix4> palette =
        asset::SampleAnimationPalette(model.assetData, 0, totalSeconds);
    for (std::size_t jointIndex = 0; jointIndex < palette.size(); ++jointIndex)
    {
        std::memcpy(
            &constants.boneMatrices[jointIndex],
            palette[jointIndex].elements.data(),
            sizeof(DirectX::XMFLOAT4X4));
    }
    return constants;
}
} // namespace

RenderStatistics HybridDeferredRenderer::RenderShadowModel(
    ID3D11DeviceContext* const context,
    const detail::GpuSceneModel& model,
    const float* const worldMatrix,
    const float* const lightViewProjectionMatrix,
    const double totalSeconds) const
{
    using namespace DirectX;

    const XMMATRIX world = LoadMatrix(worldMatrix);
    const XMMATRIX lightViewProjection = LoadMatrix(lightViewProjectionMatrix);
    const HybridSkinConstants skinConstants = MakeSkinConstants(model, totalSeconds);
    context->UpdateSubresource(skinConstantBuffer_.Get(), 0, nullptr, &skinConstants, 0, 0);

    HybridSceneConstants constants{};
    XMStoreFloat4x4(&constants.worldViewProjection, world * lightViewProjection);
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
    const detail::GpuSceneModel& model,
    ID3D11Buffer* const instanceBuffer,
    const std::uint32_t instanceCount,
    const float* const lightViewProjectionMatrix) const
{
    using namespace DirectX;

    if (context == nullptr || instanceBuffer == nullptr || instanceCount == 0)
    {
        throw std::invalid_argument{"instanced shadow draw requires instances"};
    }

    HybridSceneConstants constants{};
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
    const detail::GpuSceneModel& model,
    const float* const worldMatrix,
    const float* const viewProjectionMatrix,
    const double totalSeconds) const
{
    using namespace DirectX;

    const XMMATRIX world = LoadMatrix(worldMatrix);
    const XMMATRIX viewProjection = LoadMatrix(viewProjectionMatrix);
    const HybridSkinConstants skinConstants = MakeSkinConstants(model, totalSeconds);
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
        const detail::GpuSceneMaterial& material = model.materials.at(part.materialIndex);
        HybridSceneConstants constants{};
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
    const detail::GpuSceneModel& model,
    ID3D11Buffer* const instanceBuffer,
    const std::uint32_t instanceCount,
    const float* const viewProjectionMatrix) const
{
    using namespace DirectX;

    if (context == nullptr || instanceBuffer == nullptr || instanceCount == 0)
    {
        throw std::invalid_argument{"instanced G-Buffer draw requires instances"};
    }

    const XMMATRIX viewProjection = LoadMatrix(viewProjectionMatrix);
    const HybridSkinConstants skinConstants = MakeIdentitySkinConstants();
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
        const detail::GpuSceneMaterial& material = model.materials.at(part.materialIndex);
        HybridSceneConstants constants{};
        XMStoreFloat4x4(&constants.worldViewProjection, viewProjection);
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

    HybridSceneConstants constants{};
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
    HybridLightingConstants constants{};
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
} // namespace dxa::engine
