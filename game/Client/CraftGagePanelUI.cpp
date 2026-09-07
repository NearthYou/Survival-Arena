#include "pch.h"
#include "CraftGagePanelUI.h"

#include "Player.h"

CraftGagePanelUI::CraftGagePanelUI(shared_ptr<Player> player)
	:m_player(player)
{

}

CraftGagePanelUI::~CraftGagePanelUI()
{

}


void CraftGagePanelUI::Initialize()
{
	CreatePanels();

	SetVisible(false);
}

void CraftGagePanelUI::Update()
{
	if (m_createdItem && m_isVisible)
	{
		m_duration += DT;
		UpdateGageBar();
	}
}

void CraftGagePanelUI::SetVisible(bool visible)
{
	m_isVisible = visible;
	m_panel->GetUIPanel()->SetVisible(visible);
}

void CraftGagePanelUI::Cleanup()
{

}

void CraftGagePanelUI::UpdateGageBar()
{ // 직접 size 수정 대신 SetLayerSize() 사용
	float craftTime = GetCraftTimeByGrade(m_createdItem->GetItemGrade());

	float ratio = ((float)m_duration / (float)craftTime);
	if (ratio >= 1.f)
	{
		m_duration = 0.f;
		SetVisible(false);
		return;
	}

	auto imageUI = m_panel->GetUIPanel()->GetImageUI(L"ImageUI");
	
	Vec2 newSize = Vec2(268.f * ratio, 10.f);
	Vec2 newPos = Vec2(268.f / 2.f - (268.f / 2.f) * (1 - ratio), 10.f / 2.f);
	imageUI->SetLayerSize(0, newSize);  // 레이어 0의 크기 변경
	imageUI->SetLayerPosition(0, newPos);

	auto textUI = m_panel->GetUIPanel()->GetD2DText(L"Text");
	textUI->SetText(m_createdItem->GetName() + L"  제작 중");
}

void CraftGagePanelUI::CreatePanels()
{
	m_panel = make_shared<GameObject>();
	m_panel->SetName(L"CraftGagePanel");

	auto panel = make_shared<UIPanel>();
	m_panel->AddComponent(panel);

	panel->Create(Vec2(683, 557), Vec2(268, 10), Vec4(0.f, 0.f, 0.f, 1.f), nullptr);
	m_panel->SetLayerIndex(LAYER_UI);

	shared_ptr<Material> cloneMaterial_craftGageBar = RESOURCES->Get<Material>(L"BlueBar");
	auto imageUI = panel->AddImageUI(Vec2(0, 0), L"ImageUI");
	imageUI->AddImageLayer(0, Vec2(134,5), Vec2(268, 10) * (1/RESOLUTION_CONSTANT), cloneMaterial_craftGageBar, 1);

	panel->AddD2DText(
		Vec2(134, -10),
		L"",
		12.f,
		Vec4(1.f),
		1.f,
		Vec4(0.f),
		0.f,
		L"Text",
		TextAlignment::Center
	);
	CURSCENE->AddUIObject(m_panel, true);
	CURSCENE->RegisterUIParent(m_panel);
}

void CraftGagePanelUI::RegisterUIObject(shared_ptr<GameObject> uiObject)
{

}


float CraftGagePanelUI::GetCraftTimeByGrade(ITEMGRADE grade)
{
	switch (grade) {
	case ITEMGRADE::COMMON:    return 1.0f; 
	case ITEMGRADE::UNCOMMON:  return 3.0f; 
	case ITEMGRADE::RARE:      return 5.0f; 
	case ITEMGRADE::EPIC:      return 7.0f; 
	case ITEMGRADE::LEGENDARY: return 9.0f; 
	default:                   return 11.0f;
	}
}
