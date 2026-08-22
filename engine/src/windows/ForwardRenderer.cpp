#include <dxa/engine/ForwardRenderer.hpp>

#include <DirectXMath.h>
#include <d3dcompiler.h>

#include <array>
#include <cstdint>
#include <sstream>
#include <stdexcept>

namespace dxa::engine
{
namespace
{
struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 color;
};

struct FrameConstants
{
    DirectX::XMFLOAT4X4 worldViewProjection;
};

void RequireSuccess(const HRESULT result, const char* operation)
{
    if (FAILED(result))
    {
        std::ostringstream message;
        message << operation << " failed with HRESULT 0x" << std::hex << std::uppercase
                << static_cast<std::uint32_t>(result);
        throw std::runtime_error(message.str());
    }
}

Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
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
        throw std::runtime_error("shader compilation failed: " + details);
    }
    return shader;
}
} // namespace

void ForwardRenderer::Initialize(
    ID3D11Device* const device,
    const std::filesystem::path& shaderPath)
{
    if (device == nullptr)
    {
        throw std::invalid_argument("forward renderer requires a Direct3D device");
    }

    const auto vertexShaderBytecode = CompileShader(shaderPath, "VSMain", "vs_5_0");
    const auto pixelShaderBytecode = CompileShader(shaderPath, "PSMain", "ps_5_0");

    RequireSuccess(
        device->CreateVertexShader(
            vertexShaderBytecode->GetBufferPointer(),
            vertexShaderBytecode->GetBufferSize(),
            nullptr,
            vertexShader_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateVertexShader");
    RequireSuccess(
        device->CreatePixelShader(
            pixelShaderBytecode->GetBufferPointer(),
            pixelShaderBytecode->GetBufferSize(),
            nullptr,
            pixelShader_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreatePixelShader");

    constexpr D3D11_INPUT_ELEMENT_DESC InputElements[]{
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    RequireSuccess(
        device->CreateInputLayout(
            InputElements,
            static_cast<UINT>(std::size(InputElements)),
            vertexShaderBytecode->GetBufferPointer(),
            vertexShaderBytecode->GetBufferSize(),
            inputLayout_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateInputLayout");

    constexpr std::array Vertices{
        Vertex{{-1.0F, -1.0F, -1.0F}, {0.95F, 0.25F, 0.20F}},
        Vertex{{-1.0F, 1.0F, -1.0F}, {0.20F, 0.80F, 0.95F}},
        Vertex{{1.0F, 1.0F, -1.0F}, {0.95F, 0.80F, 0.20F}},
        Vertex{{1.0F, -1.0F, -1.0F}, {0.40F, 0.95F, 0.35F}},
        Vertex{{-1.0F, -1.0F, 1.0F}, {0.75F, 0.30F, 0.95F}},
        Vertex{{-1.0F, 1.0F, 1.0F}, {0.25F, 0.95F, 0.75F}},
        Vertex{{1.0F, 1.0F, 1.0F}, {0.95F, 0.45F, 0.65F}},
        Vertex{{1.0F, -1.0F, 1.0F}, {0.40F, 0.55F, 1.0F}}};
    constexpr std::array<std::uint16_t, 36> Indices{
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        4, 5, 1, 4, 1, 0,
        3, 2, 6, 3, 6, 7,
        1, 5, 6, 1, 6, 2,
        4, 0, 3, 4, 3, 7};
    indexCount_ = static_cast<std::uint32_t>(Indices.size());

    D3D11_BUFFER_DESC vertexBufferDescription{};
    vertexBufferDescription.ByteWidth = static_cast<UINT>(sizeof(Vertices));
    vertexBufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
    vertexBufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vertexData{};
    vertexData.pSysMem = Vertices.data();
    RequireSuccess(
        device->CreateBuffer(
            &vertexBufferDescription, &vertexData, vertexBuffer_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer(vertices)");

    D3D11_BUFFER_DESC indexBufferDescription{};
    indexBufferDescription.ByteWidth = static_cast<UINT>(sizeof(Indices));
    indexBufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
    indexBufferDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA indexData{};
    indexData.pSysMem = Indices.data();
    RequireSuccess(
        device->CreateBuffer(
            &indexBufferDescription, &indexData, indexBuffer_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer(indices)");

    D3D11_BUFFER_DESC constantBufferDescription{};
    constantBufferDescription.ByteWidth = sizeof(FrameConstants);
    constantBufferDescription.Usage = D3D11_USAGE_DEFAULT;
    constantBufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    RequireSuccess(
        device->CreateBuffer(
            &constantBufferDescription, nullptr, constantBuffer_.ReleaseAndGetAddressOf()),
        "ID3D11Device::CreateBuffer(constants)");
}

void ForwardRenderer::Render(
    ID3D11DeviceContext* const context,
    const double totalSeconds,
    const float aspectRatio) const
{
    using namespace DirectX;

    const XMMATRIX world =
        XMMatrixRotationX(0.35F) * XMMatrixRotationY(static_cast<float>(totalSeconds));
    const XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(0.0F, 1.8F, -5.0F, 1.0F),
        XMVectorZero(),
        XMVectorSet(0.0F, 1.0F, 0.0F, 0.0F));
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 0.1F, 100.0F);

    FrameConstants constants{};
    XMStoreFloat4x4(&constants.worldViewProjection, XMMatrixTranspose(world * view * projection));
    context->UpdateSubresource(constantBuffer_.Get(), 0, nullptr, &constants, 0, 0);

    constexpr UINT Stride = sizeof(Vertex);
    constexpr UINT Offset = 0;
    ID3D11Buffer* vertexBuffer = vertexBuffer_.Get();
    ID3D11Buffer* constantBuffer = constantBuffer_.Get();
    context->IASetInputLayout(inputLayout_.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &Stride, &Offset);
    context->IASetIndexBuffer(indexBuffer_.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &constantBuffer);
    context->PSSetShader(pixelShader_.Get(), nullptr, 0);
    context->DrawIndexed(indexCount_, 0, 0);
}
} // namespace dxa::engine

