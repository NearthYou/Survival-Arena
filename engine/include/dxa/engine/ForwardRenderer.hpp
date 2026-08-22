#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <filesystem>

namespace dxa::engine
{
class ForwardRenderer
{
public:
    void Initialize(ID3D11Device* device, const std::filesystem::path& shaderPath);
    void Render(ID3D11DeviceContext* context, double totalSeconds, float aspectRatio) const;

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;
    std::uint32_t indexCount_ = 0;
};
} // namespace dxa::engine

