#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>

namespace dxa::engine
{
enum class GraphicsDriver;

struct GraphicsDeviceConfig
{
    HWND window = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    GraphicsDriver driver;
    bool enableDebugLayer = false;
};

class GraphicsDevice
{
public:
    void Initialize(const GraphicsDeviceConfig& config);
    void BeginFrame(const std::array<float, 4>& clearColor) const;
    void EndFrame(bool vsync) const;

    [[nodiscard]] ID3D11Device* Device() const noexcept;
    [[nodiscard]] ID3D11DeviceContext* Context() const noexcept;
    [[nodiscard]] bool DebugLayerEnabled() const noexcept;

private:
    void CreateTargets(std::uint32_t width, std::uint32_t height);

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView_;
    D3D11_VIEWPORT viewport_{};
    bool debugLayerEnabled_ = false;
};
} // namespace dxa::engine

