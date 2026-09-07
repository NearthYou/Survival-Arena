#include "pch.h"
#include "TimePanelUI.h"
#include "UIResourceManager.h"

TimePanelUI::TimePanelUI()
{

}
TimePanelUI::~TimePanelUI()
{

}

void TimePanelUI::Update()
{
	UpdateTime();
}

void TimePanelUI::Initialize()
{
	CreatePanels();
}

void TimePanelUI::SetVisible(bool visible)
{
	m_isVisible = visible;
	m_panel->GetUIPanel()->SetVisible(visible);
}

void TimePanelUI::Cleanup()
{

}

void TimePanelUI::UpdateTime()
{
	if (!m_panel) return;

	m_lastFloatTime += DT;

	int currentSeconds = static_cast<int>(m_lastFloatTime);

	if (currentSeconds <= m_lastTime)
		return;

	auto timeText = m_panel->GetUIPanel()->GetD2DText(L"TimeText");
	if (timeText) {
		// 현재 게임 시간 계산 (예시)
		float currentTime = m_lastFloatTime; // 또는 게임 시간 로직
		int minutes = (int)(currentTime / 60.0f);
		int seconds = (int)(currentTime) % 60;

		wchar_t timeBuffer[8];
		swprintf_s(timeBuffer, 8, L"%02d : %02d", minutes, seconds);
		wstring timeString = timeBuffer;

		timeText->SetText(timeString);
	}
}



void TimePanelUI::CreatePanels()
{
	m_panel = make_shared<GameObject>();
	m_panel->SetName(L"Time Panel");

	auto panel = make_shared<UIPanel>();
	m_panel->AddComponent(panel);


	shared_ptr<Material> TimePanelBackGround = RESOURCES->Get<Material>(L"Time_UI_BG")->Clone();
	panel->Create(Vec2(GAME->GetGameDesc().width / 2.f, 0.f), Vec2(117, 58), Vec4(1.f, 1.f, 1.f, 0.5f), TimePanelBackGround);
	m_panel->SetLayerIndex(LAYER_UI);

	// 가운데에 하얀 텍스트 추가
	panel->AddD2DText(
		Vec2(117 / 2.f, 58 / 2.f + 14.f),      // 패널 가운데 위치
		L"00 : 00",                        // 시간 텍스트 (예시)
		16.0f,                          // 폰트 크기
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

void TimePanelUI::RegisterUIObject(shared_ptr<GameObject> uiObject)
{

}
