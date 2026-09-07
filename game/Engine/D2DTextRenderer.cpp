#include "pch.h"
#include "D2DTextRenderer.h"
#include "Texture.h"

//D2DTextRenderer* D2DTextRenderer::s_instance = nullptr;
map<wstring, weak_ptr<Texture>> D2DTextRenderer::s_textureCache;

//D2DTextRenderer* D2DTextRenderer::GetInstance()
//{
//    if (!s_instance) {
//        s_instance = new D2DTextRenderer();
//        s_instance->Initialize();
//    }
//    return s_instance;
//}

//void D2DTextRenderer::DestroyInstance()
//{
//    if (s_instance) {
//        s_instance->Shutdown();
//        delete s_instance;
//        s_instance = nullptr;
//    }
//}

bool D2DTextRenderer::Init()
{
    HRESULT hr = S_OK;

    // D2D Factory 생성
    hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        m_d2dFactory.GetAddressOf()
    );
    if (FAILED(hr)) return false;

    // DirectWrite Factory 생성
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_writeFactory.GetAddressOf())
    );
    if (FAILED(hr)) return false;

    // WIC Factory 생성
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(m_wicFactory.GetAddressOf())
    );
    if (FAILED(hr)) return false;

    return true;
}

void D2DTextRenderer::Shutdown()
{
    ClearCache();
    m_fontFormats.clear();

    if (m_wicFactory) m_wicFactory.Reset();
    if (m_writeFactory) m_writeFactory.Reset();
    if (m_d2dFactory) m_d2dFactory.Reset();
}

shared_ptr<Texture> D2DTextRenderer::CreateTextTexture(
    const wstring& text,
    const wstring& fontName,
    float fontSize,
    const Vec4& textColor,
    OUT int& textWidth,
    OUT int& textHeight,
    uint64 instanceID,
    const Vec4& outlineColor,
    float outlineWidth,
    TextAlignment alignment
   )
{
    if (text.empty()) return nullptr;

    // 캐시 확인
    wstring cacheKey = GenerateCacheKey(text, fontName, fontSize, textColor, alignment);

    if (instanceID != 0) {
        cacheKey += L"_inst_" + to_wstring(instanceID);
    }

    auto cacheIt = s_textureCache.find(cacheKey);
    if (cacheIt != s_textureCache.end()) {
        if (auto cachedTexture = cacheIt->second.lock()) {
            return cachedTexture;
        }
        s_textureCache.erase(cacheIt);
    }

    // 텍스트 포맷 가져오기
    auto textFormat = GetOrCreateTextFormat(fontName, fontSize);
    if (!textFormat) return nullptr;

    // 정렬 설정
    DWRITE_TEXT_ALIGNMENT dwriteAlignment = DWRITE_TEXT_ALIGNMENT_LEADING;
    switch (alignment) {
    case TextAlignment::Center:
        dwriteAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        break;
    case TextAlignment::Right:
        dwriteAlignment = DWRITE_TEXT_ALIGNMENT_TRAILING;
        break;
    }
    textFormat->SetTextAlignment(dwriteAlignment);
    textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // 텍스트 레이아웃 생성 및 크기 측정
    ComPtr<IDWriteTextLayout> textLayout;
    HRESULT hr = m_writeFactory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.length()),
        textFormat.Get(),
        1000.0f, // 임시 최대 너비
        1000.0f, // 임시 최대 높이
        textLayout.GetAddressOf()
    );
    if (FAILED(hr)) return nullptr;

    // 텍스트 메트릭 가져오기
    DWRITE_TEXT_METRICS metrics;
    textLayout->GetMetrics(&metrics);

    // 패딩 추가 (외곽선 등을 위해)
    int padding = static_cast<int>(max(outlineWidth * 2, 4.0f));
    textWidth = static_cast<int>(ceil(metrics.width)) + padding * 2;
    textHeight = static_cast<int>(ceil(metrics.height)) + padding * 2;

    // 최소 크기 보장
    textWidth = max(textWidth, 64);
    textHeight = max(textHeight, 32);

    // 실제 크기로 텍스트 레이아웃 재생성
    textLayout.Reset();
    hr = m_writeFactory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.length()),
        textFormat.Get(),
        static_cast<float>(textWidth - padding * 2),
        static_cast<float>(textHeight - padding * 2),
        textLayout.GetAddressOf()
    );
    if (FAILED(hr)) return nullptr;

    // WIC 비트맵 생성
    ComPtr<IWICBitmap> wicBitmap;
    hr = m_wicFactory->CreateBitmap(
        textWidth,
        textHeight,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnDemand,
        wicBitmap.GetAddressOf()
    );
    if (FAILED(hr)) return nullptr;

    // D2D 렌더 타겟 생성
    ComPtr<ID2D1RenderTarget> renderTarget;
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0, 0,
        D2D1_RENDER_TARGET_USAGE_NONE,
        D2D1_FEATURE_LEVEL_DEFAULT
    );

    hr = m_d2dFactory->CreateWicBitmapRenderTarget(
        wicBitmap.Get(),
        rtProps,
        renderTarget.GetAddressOf()
    );
    if (FAILED(hr)) return nullptr;

    // 브러시 생성
    ComPtr<ID2D1SolidColorBrush> textBrush, outlineBrush;
    renderTarget->CreateSolidColorBrush(
        D2D1::ColorF(textColor.x, textColor.y, textColor.z, textColor.w),
        textBrush.GetAddressOf()
    );

    if (outlineWidth > 0) {
        renderTarget->CreateSolidColorBrush(
            D2D1::ColorF(outlineColor.x, outlineColor.y, outlineColor.z, outlineColor.w),
            outlineBrush.GetAddressOf()
        );
    }

    // 렌더링
    renderTarget->BeginDraw();
    renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0)); // 투명 배경

    D2D1_POINT_2F origin = D2D1::Point2F(static_cast<float>(padding), static_cast<float>(padding));

    // 외곽선 렌더링 (간단한 방법: 여러 번 그리기)
    if (outlineWidth > 0 && outlineBrush) {
        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                if (x == 0 && y == 0) continue;
                D2D1_POINT_2F outlineOrigin = D2D1::Point2F(
                    origin.x + x * outlineWidth,
                    origin.y + y * outlineWidth
                );
                renderTarget->DrawTextLayout(
                    outlineOrigin,
                    textLayout.Get(),
                    outlineBrush.Get()
                );
            }
        }
    }

    // 메인 텍스트 렌더링
    renderTarget->DrawTextLayout(origin, textLayout.Get(), textBrush.Get());

    hr = renderTarget->EndDraw();
    if (FAILED(hr)) return nullptr;

    // DirectX 텍스처로 변환
    auto resultTexture = CreateTextureFromWICBitmap(wicBitmap, textWidth, textHeight);

    // 캐시에 저장할 때도 같은 키 사용
    if (resultTexture && s_textureCache.size() < MAX_CACHE_SIZE) {
        s_textureCache[cacheKey] = resultTexture;
    }

    return resultTexture;
}

shared_ptr<Texture> D2DTextRenderer::CreateTextureFromWICBitmap(ComPtr<IWICBitmap> wicBitmap, int width, int height)
{
    // WIC 비트맵을 DirectX 텍스처로 변환
    ComPtr<IWICBitmapLock> lock;
    WICRect rect = { 0, 0, width, height };
    HRESULT hr = wicBitmap->Lock(&rect, WICBitmapLockRead, lock.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    UINT bufferSize;
    BYTE* buffer;
    hr = lock->GetDataPointer(&bufferSize, &buffer);
    if (FAILED(hr)) return nullptr;

    // DirectX 텍스처 생성
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = buffer;
    initData.SysMemPitch = width * 4;

    ComPtr<ID3D11Texture2D> texture2D;
    hr = DEVICE->CreateTexture2D(&desc, &initData, texture2D.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    ComPtr<ID3D11ShaderResourceView> srv;
    hr = DEVICE->CreateShaderResourceView(texture2D.Get(), nullptr, srv.GetAddressOf());
    if (FAILED(hr)) return nullptr;

    // Texture 객체 생성
    auto resultTexture = make_shared<Texture>();
    resultTexture->SetSRV(srv);

    return resultTexture;
}

ComPtr<IDWriteTextFormat> D2DTextRenderer::GetOrCreateTextFormat(const wstring& fontName, float fontSize)
{
    wstring formatKey = fontName + L"_" + to_wstring((int)fontSize);
    auto it = m_fontFormats.find(formatKey);
    if (it != m_fontFormats.end()) {
        return it->second;
    }

    ComPtr<IDWriteTextFormat> textFormat;
    HRESULT hr = m_writeFactory->CreateTextFormat(
        fontName.c_str(),
        nullptr,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize,
        L"ko-KR", // 한국어 로케일
        textFormat.GetAddressOf()
    );

    if (SUCCEEDED(hr)) {
        m_fontFormats[formatKey] = textFormat;
        return textFormat;
    }

    return nullptr;
}

wstring D2DTextRenderer::GenerateCacheKey(const wstring& text, const wstring& fontName,
    float fontSize, const Vec4& textColor, TextAlignment alignment) const
{
    return text + L"_" + fontName + L"_" + to_wstring((int)fontSize) + L"_" +
        to_wstring((int)(textColor.x * 255)) + L"_" + to_wstring((int)alignment);
}

void D2DTextRenderer::ClearCache()
{
    s_textureCache.clear();
}
