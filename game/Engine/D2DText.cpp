#include "pch.h"
#include "D2DText.h"
#include "D2DTextRenderer.h"
#include "Transform.h"
#include "Material.h"
#include "Mesh.h"
#include "Texture.h"
#include "MeshRenderer.h"
#include "Shader.h"

D2DText::D2DText() : Super(ComponentType::D2DText) // 기존 Text와 같은 타입 사용
{
}

D2DText::~D2DText()
{
    m_textColorEffect.Reset();
    m_outlineColorEffect.Reset();
    m_textAlphaEffect.Reset();
    m_outlineWidthEffect.Reset();
}

void D2DText::Init()
{
    auto go = GetGameObject();

    if (go->GetMeshRenderer() == nullptr) {
        go->AddComponent(make_shared<MeshRenderer>());
    }

    m_mesh = make_shared<Mesh>();
    m_mesh->CreateQuad();

    m_material = make_shared<Material>();
    auto shader = make_shared<Shader>(L"TextShader.fx");
    m_material->SetShader(shader);
    m_material->SetRenderQueue(RenderQueue::Transparent);

    // 셰이더 변수 가져오기
    m_textColorEffect = shader->GetVector("TextColor");
    m_outlineColorEffect = shader->GetVector("OutlineColor");
    m_textAlphaEffect = shader->GetScalar("TextAlpha");
    m_outlineWidthEffect = shader->GetScalar("OutlineWidth");

    go->GetMeshRenderer()->SetMesh(m_mesh);
    go->GetMeshRenderer()->SetMaterial(m_material);
    go->GetMeshRenderer()->SetPass(0);
    go->SetLayerIndex(LAYER_UI);

    CreateTextTexture();
}

void D2DText::Update()
{
    Super::Update();

    float currentTime = TIME->GetGameTime();

    // 지연된 업데이트 처리
    if (m_hasPendingUpdate && (currentTime - m_lastUpdateTime) >= m_updateInterval) {
        m_text = m_pendingText;
        m_needUpdate = true;
        m_hasPendingUpdate = false;
        m_lastUpdateTime = currentTime;
    }

    if (m_needUpdate) {
        CreateTextTexture();
        UpdateMaterial();
        m_needUpdate = false;
    }

    // 매 프레임 셰이더 데이터 전달
    PushTextData();
}

void D2DText::OnDestroy()
{
    // D2D 리소스들은 자동으로 정리됨 (shared_ptr 사용)
    Super::OnDestroy();
}

void D2DText::SetText(const wstring& text)
{
    if (m_text != text) {
        m_text = text;
        m_needUpdate = true;
    }
}

void D2DText::SetTextDelayed(const wstring& text)
{
    if (m_text != text) {
        m_pendingText = text;
        m_hasPendingUpdate = true;
    }
}

void D2DText::SetColor(const Vec4& color)
{
    if (m_color != color) {
        m_color = color;
        m_needUpdate = true;
    }
}

void D2DText::SetPosition(const Vec2& position)
{
    m_position = position;

    auto go = GetGameObject();
    if (go) {
        float height = GRAPHICS->GetViewport().GetHeight();
        float width = GRAPHICS->GetViewport().GetWidth();

        float x = position.x - width / 2.0f;
        float y = height / 2.0f - position.y;

        go->GetTransform()->SetPosition(Vec3(x, y, m_zPos));
    }
}

void D2DText::SetFontSize(float size)
{
    if (m_fontSize != size) {
        m_fontSize = size;
        m_needUpdate = true;
    }
}

void D2DText::SetFontName(const wstring& fontName)
{
    if (m_fontName != fontName) {
        m_fontName = fontName;
        m_needUpdate = true;
    }
}

void D2DText::SetAlpha(float alpha)
{
    if (m_alpha != alpha) {
        m_alpha = clamp(alpha, 0.0f, 1.0f);
        m_needUpdate = true;
    }
}

void D2DText::SetAlignment(TextAlignment alignment)
{
    if (m_alignment != alignment) {
        m_alignment = alignment;
        m_needUpdate = true;
    }
}

void D2DText::SetOutlineColor(const Vec4& color)
{
    if (m_outlineColor != color) {
        m_outlineColor = color;
        m_needUpdate = true;
    }
}

void D2DText::SetOutlineWidth(float width)
{
    if (m_outlineWidth != width) {
        m_outlineWidth = clamp(width, 0.0f, 5.0f);
        m_needUpdate = true;
    }
}

void D2DText::EnableOutline(bool enable)
{
    if (m_enableOutline != enable) {
        m_enableOutline = enable;
        m_needUpdate = true;
    }
}

void D2DText::Create(Vec2 screenPos, const wstring& text, float fontSize,
    Vec4 color, float alpha, Vec4 outlineColor, float outlineWidth, TextAlignment alignment)
{
    if (!m_material) {
        Init();
    }

    m_text = text;
    m_fontSize = fontSize;
    m_color = color;
    m_alpha = alpha;
    m_outlineColor = outlineColor;
    m_alignment = alignment;
    m_outlineWidth = outlineWidth;
    m_position = screenPos;
    m_localPosition = screenPos;

    

    auto go = GetGameObject();
    SetPosition(screenPos);
    go->GetTransform()->SetScale(Vec3(1, 1, 1));

    m_needUpdate = true;
}

void D2DText::CreateTextTexture()
{
    if (m_text.empty()) return;

    // GameObject 포인터를 고유 ID로 사용
    uint64 instanceID = (uint64)GetGameObject().get();

  
    auto d2dRenderer = D2DTextRenderer::GetInstance();
    m_textTexture = d2dRenderer->CreateTextTexture(
        m_text,
        m_fontName,
        m_fontSize,
        m_color,
        m_textWidth,
        m_textHeight,
        instanceID,
        m_enableOutline ? m_outlineColor : Vec4::Zero,
        m_enableOutline ? m_outlineWidth : 0.0f,
        m_alignment
    );

    if (m_textTexture) {
        // Transform 스케일 조정
        auto go = GetGameObject();
        if (go) {
            float scaleX = static_cast<float>(m_textWidth);
            float scaleY = static_cast<float>(m_textHeight);
         
            go->GetTransform()->SetScale(Vec3(scaleX, scaleY, 1.0f));
        }
    }
}

void D2DText::UpdateMaterial()
{
    if (m_material && m_textTexture) {
        m_material->SetDiffuseMap(m_textTexture);

        MaterialDesc& desc = m_material->GetMaterialDesc();
        desc.ambient = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        desc.diffuse = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        desc.specular = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        desc.emissive = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

void D2DText::PushTextData()
{
    if (m_material && m_material->GetShader()) {
        m_material->GetShader()->PushTextData(m_color, m_outlineColor, m_alpha, m_outlineWidth);
    }
}

void D2DText::UpdatePosition(const Vec2& parentWorldPos)
{
    auto go = m_gameObject.lock();
    if (!go) return;

    Vec2 newWorldPos;
    newWorldPos.x = parentWorldPos.x + m_localPosition.x;
    newWorldPos.y = parentWorldPos.y + m_localPosition.y;

    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();

    float x = newWorldPos.x - width / 2;
    float y = height / 2 - newWorldPos.y;

    go->GetTransform()->SetPosition(Vec3(x, y, m_zPos));
}
void D2DText::SetVisible(bool visible)
{
    m_visible = visible;

    GetGameObject()->SetActive(visible);
}

// D2DText.cpp에서
InstanceID D2DText::GetInstanceID()
{
    // 텍스트 내용이나 GameObject 포인터를 추가하여 고유성 보장
    return make_pair(
        (uint64)m_mesh.get(),
        (uint64)GetGameObject().get()  // GameObject 포인터로 고유성 확보
    );
}