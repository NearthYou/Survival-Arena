#include "pch.h"
#include "SkillLevelUpPanelUI.h"
#include "ArenaUiTheme.h"

#include "GameHUDPanelUI.h"

#include "Player.h"

SkillLevelUpPanelUI::SkillLevelUpPanelUI(shared_ptr<Player> player)
	:m_player(player)
{

}

SkillLevelUpPanelUI::~SkillLevelUpPanelUI()
{

}


void SkillLevelUpPanelUI::Initialize()
{
	CreatePanels();
}

void SkillLevelUpPanelUI::Update()
{
	UpdateSkillLevelPanel();
}

void SkillLevelUpPanelUI::SetVisible(bool visible)
{
	m_isVisible = visible;
	m_panel->GetUIPanel()->SetVisible(visible);
}

void SkillLevelUpPanelUI::Cleanup()
{
	
}


void SkillLevelUpPanelUI::CreatePanels()
{
	m_panel = make_shared<GameObject>();
	m_panel->SetName(L"SkillLevelUpPanel");

	auto panel = make_shared<UIPanel>();
	m_panel->AddComponent(panel);

	panel->Create(Vec2(679.f, 768 - 57 - 85), Vec2(258, 40), Vec4(1.f, 1.f, 1.f, 0.0f), nullptr);
	m_panel->SetLayerIndex(LAYER_UI);

	shared_ptr<Material> cloneMaterial_skillLevelUpBtn = RESOURCES->Get<Material>(L"Btn_LevelUp_MouseOver")->Clone();
    cloneMaterial_skillLevelUpBtn->GetMaterialDesc().diffuse = ArenaUi::Copper();
	for (int i = 0; i < 4; i++)
	{
		auto skillLevelUpBtn = panel->AddButton(Vec2(27 + 55 * i, 25), Vec2(36,36), cloneMaterial_skillLevelUpBtn->Clone(), L"SkillLevelUpBtn" + to_wstring(i));
		skillLevelUpBtn->GetGameObject()->GetMeshRenderer()->SetPass(5);
		skillLevelUpBtn->OnClick += [this, i] {

			if (m_player->GetStatus().availableSkillPoints <= 0)
				return;

			ISkill* skill = m_player->GetSkill(i);

			int curSkillLevel = skill->GetCurSkillLevel();
			int maxSkillLevel = skill->GetMaxSkillLevel();

			if (curSkillLevel == maxSkillLevel) return;

			m_player->GetSkill(i)->SkillLevelUp();
			m_gameHUD.lock()->UpdateSkillLevelBar(i);
			m_player->GetStatus().availableSkillPoints--;

		};
	}

	CURSCENE->AddUIObject(m_panel, true);
	CURSCENE->RegisterUIParent(m_panel);
}

void SkillLevelUpPanelUI::RegisterUIObject(shared_ptr<GameObject> uiObject)
{

}

void SkillLevelUpPanelUI::UpdateSkillLevelPanel()
{
	PlayerStatus& playerStatus = m_player->GetStatus();

	if (playerStatus.availableSkillPoints > 0) SetVisible(true);
	else SetVisible(false);

	if (m_player->GetSkill(0)->GetCurSkillLevel() == m_player->GetSkill(0)->GetMaxSkillLevel())
	{
		m_panel->GetUIPanel()->GetButton(L"SkillLevelUpBtn" + to_wstring(0))->SetVisible(false);
	}
	if (m_player->GetSkill(1)->GetCurSkillLevel() == m_player->GetSkill(1)->GetMaxSkillLevel())
	{
		m_panel->GetUIPanel()->GetButton(L"SkillLevelUpBtn" + to_wstring(1))->SetVisible(false);
	}
	if (m_player->GetSkill(2)->GetCurSkillLevel() == m_player->GetSkill(2)->GetMaxSkillLevel())
	{
		m_panel->GetUIPanel()->GetButton(L"SkillLevelUpBtn" + to_wstring(2))->SetVisible(false);
	}
	if (m_player->GetSkill(3)->GetCurSkillLevel() == m_player->GetSkill(3)->GetMaxSkillLevel())
	{
		m_panel->GetUIPanel()->GetButton(L"SkillLevelUpBtn" + to_wstring(3))->SetVisible(false);
	}
}