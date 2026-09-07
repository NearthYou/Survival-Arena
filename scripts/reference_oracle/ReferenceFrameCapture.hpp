#pragma once

#include "Graphics.h"
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#if defined(DXA_GAME_RECORD_VIDEO)
#include "ReferenceVideoCapture.hpp"
#endif

// GPU readback in the private oracle only. Does not change render state or pixels.
inline HRESULT ReferenceReadFrame(std::vector<std::uint8_t>& pixels, UINT& width, UINT& height)
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
    pixels.resize(static_cast<std::size_t>(rowBytes) * description.Height);
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
    width = description.Width;
    height = description.Height;
    return S_OK;
}

inline HRESULT ReferenceWriteFrame(const wchar_t* path)
{
    std::vector<std::uint8_t> pixels;
    UINT width = 0, height = 0;
    const auto result = ReferenceReadFrame(pixels, width, height);
    if (FAILED(result)) return result;
    BITMAPFILEHEADER header{};
    BITMAPINFOHEADER info{};
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(header) + sizeof(info);
    header.bfSize = header.bfOffBits + static_cast<DWORD>(pixels.size());
    info.biSize = sizeof(info);
    info.biWidth = static_cast<LONG>(width);
    info.biHeight = -static_cast<LONG>(height);
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    output.write(reinterpret_cast<const char*>(&info), sizeof(info));
    output.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    return output ? S_OK : E_FAIL;
}

#if defined(DXA_GAME_RECORD_VIDEO)
inline HRESULT ReferenceReadVideoFrame(std::vector<std::uint8_t>& pixels, UINT& width, UINT& height)
{
    static std::array<ComPtr<ID3D11Texture2D>,3> staging;
    static uint64_t sequence = 0;
    ComPtr<ID3D11Texture2D> backBuffer;
    auto result = GRAPHICS->GetSwapChain()->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(result)) return result;
    D3D11_TEXTURE2D_DESC description{};
    backBuffer->GetDesc(&description);
    width = description.Width;
    height = description.Height;
    const bool rgba = description.Format == DXGI_FORMAT_R8G8B8A8_UNORM
        || description.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if (!rgba && description.Format != DXGI_FORMAT_B8G8R8A8_UNORM
        && description.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) return E_NOTIMPL;
    if (!staging[0])
    {
        description.Usage = D3D11_USAGE_STAGING;
        description.BindFlags = 0;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        description.MiscFlags = 0;
        for (auto& texture : staging)
        {
            result = GRAPHICS->GetDevice()->CreateTexture2D(&description, nullptr, texture.GetAddressOf());
            if (FAILED(result)) return result;
        }
    }
    const auto context = GRAPHICS->GetDeviceContext();
    const auto writeSlot = sequence % staging.size();
    context->CopyResource(staging[writeSlot].Get(),backBuffer.Get());
    const auto current = sequence++;
    if (current == 1) return S_FALSE;
    const auto readSlot = current == 0 ? writeSlot : (writeSlot + 1) % staging.size();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    result = context->Map(staging[readSlot].Get(),0,D3D11_MAP_READ,
        current == 0 ? 0 : D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
    if (result == DXGI_ERROR_WAS_STILL_DRAWING) return S_FALSE;
    if (FAILED(result)) return result;
    pixels.resize(static_cast<size_t>(width)*height*4);
    for(UINT y=0;y<height;++y)
    {
        const auto* row=static_cast<const uint8_t*>(mapped.pData)+y*mapped.RowPitch;
        auto* destination=pixels.data()+static_cast<size_t>(y)*width*4;
        for(UINT x=0;x<width;++x)
        {
            destination[x*4]=row[x*4+(rgba?2:0)];
            destination[x*4+1]=row[x*4+1];
            destination[x*4+2]=row[x*4+(rgba?0:2)];
            destination[x*4+3]=255;
        }
    }
    context->Unmap(staging[readSlot].Get(),0);
    return S_OK;
}
#endif

inline std::wstring& ReferencePendingFrame()
{
    static std::wstring path;
    return path;
}

struct ReferenceClipState
{
    std::wstring prefix;
    unsigned frames = 0;
    std::chrono::steady_clock::time_point next{};
};

inline ReferenceClipState& ReferenceClip()
{
    static ReferenceClipState clip;
    return clip;
}

inline void ReferenceQueueFrame(const wchar_t* path)
{
    ReferencePendingFrame() = path ? path : L"";
#if defined(DXA_GAME_RECORD_VIDEO)
    ReferenceVideo().Mark(path);
#endif
#if defined(DXA_GAME_RECORD_CLIP)
    auto& clip = ReferenceClip();
    if (clip.prefix.empty() && path)
    {
        const std::wstring name(path);
        if (name == L"oracle-selection-skins.bmp") clip.prefix = L"selection-clip";
        else if (name == L"oracle-nicky-q-charge.bmp") clip.prefix = L"combat-clip";
        if (!clip.prefix.empty()) clip.next = std::chrono::steady_clock::now();
    }
#endif
}

inline void ReferenceFlushFrame()
{
#if defined(DXA_GAME_RECORD_VIDEO)
    ReferenceVideo().Record(ReferenceReadVideoFrame);
#endif
    const auto path = ReferencePendingFrame();
    if (!path.empty())
    {
        ReferencePendingFrame().clear();
        const auto result = ReferenceWriteFrame(path.c_str());
        std::wofstream log("capture-status.csv", std::ios::app);
        log << path << ',' << result << '\n';
    }
#if defined(DXA_GAME_RECORD_CLIP)
    auto& clip = ReferenceClip();
    const auto now = std::chrono::steady_clock::now();
    if (!clip.prefix.empty() && clip.frames < 64 && now >= clip.next)
    {
        std::wostringstream filename;
        filename << clip.prefix << L'-' << std::setfill(L'0') << std::setw(3) << clip.frames++ << L".bmp";
        const auto result = ReferenceWriteFrame(filename.str().c_str());
        std::wofstream log("capture-status.csv", std::ios::app);
        log << filename.str() << ',' << result << '\n';
        clip.next = now + std::chrono::milliseconds(125);
    }
#endif
}
