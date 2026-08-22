#include <dxa/engine/GraphicsDevice.hpp>

#include <dxa/engine/EngineApp.hpp>

#include <dxgi.h>

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace dxa::engine
{
namespace
{
[[noreturn]] void ThrowGraphicsError(const char* operation, const HRESULT result)
{
    std::ostringstream message;
    message << operation << " failed with HRESULT 0x" << std::hex << std::uppercase
            << static_cast<std::uint32_t>(result);
    throw std::runtime_error(message.str());
}

void RequireSuccess(const HRESULT result, const char* operation)
{
    if (FAILED(result))
    {
        ThrowGraphicsError(operation, result);
    }
}
} // namespace

void GraphicsDevice::Initialize(const GraphicsDeviceConfig& config)
{
    if (config.window == nullptr || config.width == 0 || config.height == 0)
    {
        throw std::invalid_argument("graphics device requires a window and non-zero dimensions");
    }

    DXGI_SWAP_CHAIN_DESC swapChainDescription{};
    swapChainDescription.BufferDesc.Width = config.width;
    swapChainDescription.BufferDesc.Height = config.height;
    swapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDescription.SampleDesc.Count = 1;
    swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDescription.BufferCount = 2;
    swapChainDescription.OutputWindow = config.window;
    swapChainDescription.Windowed = TRUE;
    swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_DRIVER_TYPE driverType =
        config.driver == GraphicsDriver::Warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (config.enableDebugLayer)
    {
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
    }

    constexpr D3D_FEATURE_LEVEL FeatureLevels[]{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL selectedFeatureLevel = D3D_FEATURE_LEVEL_11_0;

    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        driverType,
        nullptr,
        creationFlags,
        FeatureLevels,
        static_cast<UINT>(std::size(FeatureLevels)),
        D3D11_SDK_VERSION,
        &swapChainDescription,
        swapChain_.ReleaseAndGetAddressOf(),
        device_.ReleaseAndGetAddressOf(),
        &selectedFeatureLevel,
        context_.ReleaseAndGetAddressOf());

    if (result == DXGI_ERROR_SDK_COMPONENT_MISSING && config.enableDebugLayer)
    {
        creationFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            driverType,
            nullptr,
            creationFlags,
            FeatureLevels,
            static_cast<UINT>(std::size(FeatureLevels)),
            D3D11_SDK_VERSION,
            &swapChainDescription,
            swapChain_.ReleaseAndGetAddressOf(),
            device_.ReleaseAndGetAddressOf(),
            &selectedFeatureLevel,
            context_.ReleaseAndGetAddressOf());
    }
    RequireSuccess(result, "D3D11CreateDeviceAndSwapChain");

    if (selectedFeatureLevel < D3D_FEATURE_LEVEL_11_0)
    {
        throw std::runtime_error("Direct3D feature level 11.0 is required");
    }

    debugLayerEnabled_ = (creationFlags & D3D11_CREATE_DEVICE_DEBUG) != 0;
    CreateTargets(config.width, config.height);
}

void GraphicsDevice::CreateTargets(const std::uint32_t width, const std::uint32_t height)
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    RequireSuccess(
        swapChain_->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())),
        "IDXGISwapChain::GetBuffer");
    RequireSuccess(
        device_->CreateRenderTargetView(
            backBuffer.Get(), nullptr, renderTargetView_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateRenderTargetView");

    D3D11_TEXTURE2D_DESC depthDescription{};
    depthDescription.Width = width;
    depthDescription.Height = height;
    depthDescription.MipLevels = 1;
    depthDescription.ArraySize = 1;
    depthDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDescription.SampleDesc.Count = 1;
    depthDescription.Usage = D3D11_USAGE_DEFAULT;
    depthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    RequireSuccess(
        device_->CreateTexture2D(
            &depthDescription, nullptr, depthTexture_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateTexture2D(depth)");
    RequireSuccess(
        device_->CreateDepthStencilView(
            depthTexture_.Get(), nullptr, depthStencilView_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateDepthStencilView");

    viewport_.TopLeftX = 0.0F;
    viewport_.TopLeftY = 0.0F;
    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    viewport_.MinDepth = 0.0F;
    viewport_.MaxDepth = 1.0F;
}

void GraphicsDevice::BeginFrame(const std::array<float, 4>& clearColor) const
{
    ID3D11RenderTargetView* renderTarget = renderTargetView_.Get();
    context_->OMSetRenderTargets(1, &renderTarget, depthStencilView_.Get());
    context_->RSSetViewports(1, &viewport_);
    context_->ClearRenderTargetView(renderTargetView_.Get(), clearColor.data());
    context_->ClearDepthStencilView(
        depthStencilView_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0F, 0);
}

void GraphicsDevice::EndFrame(const bool vsync) const
{
    RequireSuccess(swapChain_->Present(vsync ? 1U : 0U, 0), "IDXGISwapChain::Present");
}

ID3D11Device* GraphicsDevice::Device() const noexcept
{
    return device_.Get();
}

ID3D11DeviceContext* GraphicsDevice::Context() const noexcept
{
    return context_.Get();
}

bool GraphicsDevice::DebugLayerEnabled() const noexcept
{
    return debugLayerEnabled_;
}
} // namespace dxa::engine

