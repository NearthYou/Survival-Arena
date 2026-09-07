#include "pch.h"
#include "Cursor.h"

Cursor::Cursor()
{
}

Cursor::~Cursor()
{
}

void Cursor::Start()
{
	auto cursorObj = make_shared<GameObject>();
	cursorObj->SetName(L"MouseCursor");

    m_cursorPanel = make_shared<UIPanel>();
    cursorObj->AddComponent(m_cursorPanel);

    // 투명한 배경으로 패널 생성
    m_cursorPanel->Create(Vec2(0, 0), m_cursorSize, Vec4(0, 0, 0, 0), nullptr);
    cursorObj->SetLayerIndex(LAYER_UI);

    // 커서 이미지 UI 추가
    m_cursorImageUI = m_cursorPanel->AddImageUI(Vec2(0, 0), L"CursorImage");

    // 기존에 로드된 머티리얼 사용 (예: 아이템 아이콘 중 하나)
    shared_ptr<Material> cursorMaterial = RESOURCES->Get<Material>(L"Cursor");
    if (cursorMaterial == nullptr) {
        auto CursorTexture = RESOURCES->Load<Texture>(L"CursorImage", L"..\\Resources\\Textures\\UI\\Cursor_01.png");
        auto CursorShader = make_shared<Shader>(L"ImageShader.fx");
        
        cursorMaterial = make_shared<Material>();
        cursorMaterial->SetShader(CursorShader);
        cursorMaterial->SetRenderQueue(RenderQueue::Transparent);
        cursorMaterial->SetTransparent(true);

        cursorMaterial->SetDiffuseMap(CursorTexture);

        MaterialDesc& CursorDesc = cursorMaterial->GetMaterialDesc();
        CursorDesc.diffuse = Vec4(1.0f, 1.0f, 1.0f, 1.f);

        RESOURCES->Add<Material>(L"Cursor", cursorMaterial);
    }
    m_cursorImageUI->AddImageLayer(0, Vec2(0, 0), m_cursorSize, cursorMaterial, 1);
    m_cursorImageUI->SetZPos(0.1);

    // 씬에 추가
    CURSCENE->AddUIObject(cursorObj, true);
    CURSCENE->RegisterUIParent(cursorObj);

    ShowCursor(false);
}

void Cursor::Update()
{
    UpdateCursorPosition();
}

void Cursor::SetVisible(bool _visible)
{
    m_isVisible = _visible;

    if (m_cursorPanel) {
        m_cursorPanel->SetVisible(_visible);
    }
    ShowCursor(!_visible);
}

void Cursor::UpdateCursorPosition()
{
    if (!m_cursorPanel || !m_isVisible)
        return;

    POINT mousePos = INPUT->GetMousePos();

    // 화면 좌표로 변환
    Vec2 screenPos = Vec2(static_cast<float>(mousePos.x + 48), static_cast<float>(mousePos.y + 48));
    m_cursorPanel->SetPosition(screenPos);
}
