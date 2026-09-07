#include "pch.h"
#include "DayPanelUI.h"
#include "UIResourceManager.h"

DayPanelUI::DayPanelUI()
{

}
DayPanelUI::~DayPanelUI()
{

}

void DayPanelUI::Update()
{

}

void DayPanelUI::Initialize()
{

	CreatePanels();
}

void DayPanelUI::SetVisible(bool visible)
{
	m_isVisible = visible;
	m_panel->GetUIPanel()->SetVisible(visible);
}

void DayPanelUI::Cleanup()
{

}




void DayPanelUI::CreatePanels()
{
	m_panel = make_shared<GameObject>();
	m_panel->SetName(L"Day Panel");

	auto panel = make_shared<UIPanel>();
	m_panel->AddComponent(panel);


	shared_ptr<Material> TimePanelBackGround = RESOURCES->Get<Material>(L"Time_UI_BG")->Clone();
	panel->Create(Vec2(GAME->GetGameDesc().width / 2.f - 70.f, 0.f), Vec2(58, 58), Vec4(1.f, 1.f, 1.f, 0.5f), TimePanelBackGround);
	m_panel->SetLayerIndex(LAYER_UI);

	// 태양 아이콘 ImageUI 먼저 추가 (텍스트 뒤에 배치)
	auto sunImageUI = panel->AddImageUI(Vec2(58 / 2.f, 58 / 2.f - 5.f), L"SUN_UI_ICON");
	shared_ptr<Material> sunIconMaterial = RESOURCES->Get<Material>(L"SUN_UI_ICON")->Clone();
	sunImageUI->AddImageLayer(
		1,                             // 레이어 인덱스
		Vec2(0, 0),                     // 로컬 위치 (ImageUI 내에서의 위치)
		Vec2(24, 24),                   // 이미지 크기 (패널에 맞게 조정)
		sunIconMaterial,                // 머티리얼
		1                               // 렌더 순서
	);

	// 가운데에 하얀 텍스트 추가
	panel->AddD2DText(
		Vec2(58 / 2.f, 58 / 2.f + 6.f),      // 패널 가운데 위치
		L"1일 차",                        // 시간 텍스트 (예시)
		8.0f,                          // 폰트 크기
		Vec4(1.f, 1.f, 1.f, 1.f),      // 하얀색 (RGBA)
		1.0f,                           // 불투명도
		Vec4(0, 0, 0, 0),               // 배경색 (투명)
		0.0f,                           // 배경 불투명도
		L"TimeText",                    // 텍스트 이름
		TextAlignment::Center           // 가운데 정렬
	);

	m_panel->GetMeshRenderer()->SetActive(true);

	CURSCENE->AddUIObject(m_panel, true);
	CURSCENE->RegisterUIParent(m_panel);
}

void DayPanelUI::RegisterUIObject(shared_ptr<GameObject> uiObject)
{

}
