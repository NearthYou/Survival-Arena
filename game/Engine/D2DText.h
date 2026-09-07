#pragma once
#include "Component.h"
#include "Text.h"

class Material;
class Mesh;
class Texture;

enum class TextAlignment; // 기존 Text.h에서 정의된 것 사용

class D2DText : public Component
{
    using Super = Component;

public:
    D2DText();
    virtual ~D2DText();

    virtual void Init() override;
    virtual void Update() override;
    virtual void OnDestroy() override;

    // 텍스트 설정 함수들
    void SetText(const wstring& text);
    void SetColor(const Vec4& color);
    void SetPosition(const Vec2& position);
    void SetFontSize(float size);
    void SetFontName(const wstring& fontName);
    void SetAlpha(float alpha);
    void SetAlignment(TextAlignment alignment);

    void SetVisible(bool visible);
    bool IsVisible() const { return m_visible; }

    // 외곽선 관련 함수들
    void SetOutlineColor(const Vec4& color);
    void SetOutlineWidth(float width);
    void EnableOutline(bool enable);

    // Getter 함수들
    const wstring& GetText() const { return m_text; }
    const Vec4& GetColor() const { return m_color; }
    const Vec4& GetOutlineColor() const { return m_outlineColor; }
    const Vec2& GetPosition() const { return m_position; }
    float GetFontSize() const { return m_fontSize; }
    float GetAlpha() const { return m_alpha; }
    float GetOutlineWidth() const { return m_outlineWidth; }
    bool IsOutlineEnabled() const { return m_enableOutline; }
    TextAlignment GetAlignment() const { return m_alignment; }

    // 간단한 생성 함수
    void Create(Vec2 screenPos, const wstring& text, float fontSize = 16.0f,
        Vec4 color = Vec4(0, 0, 0, 1), float alpha = 1.0f,
        Vec4 outlineColor = Vec4(1, 1, 1, 1), float outlineWidth = 1.0f,
        TextAlignment alignment = TextAlignment::Left);

    // UI 패널용 함수들
    void UpdatePosition(const Vec2& parentWorldPos);
    void SetLocalPosition(const Vec2& localPos) { m_localPosition = localPos; }
    const Vec2& GetLocalPosition() const { return m_localPosition; }

    // 업데이트 제한 (성능 최적화)
    void SetUpdateInterval(float interval) { m_updateInterval = interval; }
    void SetTextDelayed(const wstring& text); // 지연 업데이트

    InstanceID GetInstanceID();

private:
    void CreateTextTexture();
    void UpdateMaterial();
    void PushTextData();

private:
    wstring m_text = L"D2DText";
    wstring m_fontName = L"맑은 고딕"; // 한글 기본 폰트
    Vec4 m_color = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    Vec4 m_outlineColor = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    Vec2 m_position = Vec2(0.0f, 0.0f);
    float m_fontSize = 16.0f;
    float m_alpha = 1.0f;
    float m_outlineWidth = 1.0f;
    bool m_enableOutline = false;
    TextAlignment m_alignment = TextAlignment::Left;

    bool m_needUpdate = true;

    bool m_visible = true;

    // 업데이트 제한
    float m_updateInterval = 0.1f;
    float m_lastUpdateTime = 0.0f;
    wstring m_pendingText;
    bool m_hasPendingUpdate = false;

    shared_ptr<Texture> m_textTexture;
    shared_ptr<Material> m_material;
    shared_ptr<Mesh> m_mesh;

    // 텍스트 크기 정보
    int m_textWidth = 0;
    int m_textHeight = 0;

    // UI Panel 관련
    Vec2 m_localPosition;
    const float m_zPos = 0.2f;

    // 셰이더 변수들 (기존 Text와 동일)
    ComPtr<ID3DX11EffectVectorVariable> m_textColorEffect;
    ComPtr<ID3DX11EffectVectorVariable> m_outlineColorEffect;
    ComPtr<ID3DX11EffectScalarVariable> m_textAlphaEffect;
    ComPtr<ID3DX11EffectScalarVariable> m_outlineWidthEffect;
};
