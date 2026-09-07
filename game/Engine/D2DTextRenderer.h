#pragma once
#include "pch.h"
#include "Text.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

class D2DTextRenderer
{
    DECLARE_SINGLE(D2DTextRenderer);

private:
    //static D2DTextRenderer* s_instance;

    ComPtr<ID2D1Factory> m_d2dFactory;
    ComPtr<IDWriteFactory> m_writeFactory;
    ComPtr<IWICImagingFactory> m_wicFactory;

    // 폰트 포맷 캐싱
    map<wstring, ComPtr<IDWriteTextFormat>> m_fontFormats;

    // 텍스처 캐싱
    static map<wstring, weak_ptr<Texture>> s_textureCache;
    static const int MAX_CACHE_SIZE = 100;

public:
    //static D2DTextRenderer* GetInstance();
    //static void DestroyInstance();

    bool Init();
    void Shutdown();

    // 텍스처 생성 (캐싱 지원)
    shared_ptr<Texture> CreateTextTexture(
        const wstring& text,
        const wstring& fontName,
        float fontSize,
        const Vec4& textColor, 
        OUT int& textWidth,
        OUT int& textHeight,
        uint64 instanceID,
        const Vec4& outlineColor = Vec4::Zero,
        float outlineWidth = 0.0f,
        TextAlignment alignment = TextAlignment::Left
    );

    static void ClearCache();

private:
    ComPtr<IDWriteTextFormat> GetOrCreateTextFormat(const wstring& fontName, float fontSize);
    wstring GenerateCacheKey(const wstring& text, const wstring& fontName, float fontSize,
        const Vec4& textColor, TextAlignment alignment) const;

    shared_ptr<Texture> CreateTextureFromWICBitmap(ComPtr<IWICBitmap> wicBitmap, int width, int height);
};
