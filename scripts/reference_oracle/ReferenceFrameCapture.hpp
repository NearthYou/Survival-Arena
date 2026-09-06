#pragma once

#include "Graphics.h"
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// GPU readback in the private oracle only. Does not change render state or pixels.
inline HRESULT ReferenceWriteFrame(const wchar_t* path)
{
    ComPtr<ID3D11Texture2D> backBuffer;
    auto result = GRAPHICS->GetSwapChain()->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(result)) return result;
    D3D11_TEXTURE2D_DESC description{};
    backBuffer->GetDesc(&description);
    const bool rgba = description.Format == DXGI_FORMAT_R8G8B8A8_UNORM
        || description.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if (!rgba && description.Format != DXGI_FORMAT_B8G8R8A8_UNORM
        && description.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) return E_NOTIMPL;
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    result = GRAPHICS->GetDevice()->CreateTexture2D(&description, nullptr, staging.GetAddressOf());
    if (FAILED(result)) return result;
    const auto context = GRAPHICS->GetDeviceContext();
    context->CopyResource(staging.Get(), backBuffer.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    result = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(result)) return result;
    const auto rowBytes = description.Width * 4U;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(rowBytes) * description.Height);
    for (UINT y = 0; y < description.Height; ++y)
    {
        const auto* row = static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.RowPitch;
        for (UINT x = 0; x < description.Width; ++x)
        {
            auto* pixel = pixels.data() + y * rowBytes + x * 4U;
            pixel[0] = row[x * 4U + (rgba ? 2U : 0U)];
            pixel[1] = row[x * 4U + 1U];
            pixel[2] = row[x * 4U + (rgba ? 0U : 2U)];
            pixel[3] = 255U;
        }
    }
    context->Unmap(staging.Get(), 0);
    BITMAPFILEHEADER header{};
    BITMAPINFOHEADER info{};
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(header) + sizeof(info);
    header.bfSize = header.bfOffBits + static_cast<DWORD>(pixels.size());
    info.biSize = sizeof(info);
    info.biWidth = static_cast<LONG>(description.Width);
    info.biHeight = -static_cast<LONG>(description.Height);
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    output.write(reinterpret_cast<const char*>(&info), sizeof(info));
    output.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    return output ? S_OK : E_FAIL;
}

inline std::wstring& ReferencePendingFrame()
{
    static std::wstring path;
    return path;
}

inline void ReferenceQueueFrame(const wchar_t* path)
{
    ReferencePendingFrame() = path ? path : L"";
}

inline void ReferenceFlushFrame()
{
    const auto path = ReferencePendingFrame();
    if (path.empty()) return;
    ReferencePendingFrame().clear();
    const auto result = ReferenceWriteFrame(path.c_str());
    std::wofstream log("capture-status.csv", std::ios::app);
    log << path << ',' << result << '\n';
}
