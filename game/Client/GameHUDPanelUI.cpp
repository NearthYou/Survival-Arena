#include "pch.h"
#include "GameHUDPanelUI.h"
#include "ArenaUiTheme.h"
#include "ModelAnimator.h"
#include <cmath>

#include "UIResourceManager.h"

#include "Player.h"

const vector<wstring> nickySkillIcons = {
	L"SkillIcon_1033100",
	L"SkillIcon_1033200",
	L"SkillIcon_1033300",
	L"SkillIcon_1033400",
	L"SkillIcon_1033500"
};

const vector<wstring> biancaSkillIcons = {
	L"SkillIcon_1042100",
	L"SkillIcon_1042200",
	L"SkillIcon_1042300",
	L"SkillIcon_1042400",
	L"SkillIcon_1042500"
};

GameHUDPanelUI::GameHUDPanelUI(shared_ptr<Player> player, int selectedCharIdx)
	: m_player(player)
	, m_selectedCharIdx(selectedCharIdx)
{

}

GameHUDPanelUI::~GameHUDPanelUI()
{
	
}

void GameHUDPanelUI::Initialize()
{
	CreatePanels();
}

void GameHUDPanelUI::Update()
{
	UpdateSkillCoolDown();
}

void GameHUDPanelUI::SetVisible(bool visible)
{
	m_isVisible = visible;
	m_panel->GetUIPanel()->SetVisible(visible);
}

void GameHUDPanelUI::Cleanup()
{

}


void GameHUDPanelUI::CreatePanels()
{
	m_panel = make_shared<GameObject>();
	m_panel->SetName(L"GameHUDPanel");

	auto panel = make_shared<UIPanel>();
	m_panel->AddComponent(panel);

	panel->Create(Vec2(625.f, 768 - 57), Vec2(374, 115), Vec4(0.075f,0.105f,0.12f,0.97f), nullptr);
	m_panel->SetLayerIndex(LAYER_UI);

	ArenaUi::Block(panel,Vec2(187,1),Vec2(374,2),ArenaUi::Copper(),L"HudAccent");
	CreateSkillIcons();
	CreateStatBars();
	CreateCharacterImage();

	CURSCENE->AddUIObject(m_panel, true);
	CURSCENE->RegisterUIParent(m_panel);
}

void GameHUDPanelUI::CreateSkillIcons()
{
	auto panel = m_panel->GetUIPanel();

	wstring characterTag = L"";
	if (m_selectedCharIdx == 0) characterTag = L"Bianca";
	else if (m_selectedCharIdx == 1) characterTag = L"Nicky";

	auto imageUI = m_panel->GetUIPanel()->AddImageUI(Vec2(0, 0), L"ImageUI");

	vector<wstring> skillTag = { L"Q", L"W", L"E", L"R" };
	for (int i = 0; i < 4; i++)
	{
		shared_ptr<Material> cloneMaterial_skillIcon = RESOURCES->Get<Material>(characterTag + skillTag[i])->Clone();
		imageUI->AddImageLayer(i, Vec2(139 + 55 * i, 30), Vec2(60, 60), cloneMaterial_skillIcon, 1);
        ArenaUi::Block(panel,Vec2(120+55*i,12),Vec2(18,18),ArenaUi::Ink(),L"SkillKeyBox"+to_wstring(i));
        panel->AddD2DText(Vec2(120+55*i,12),skillTag[i],11.f,ArenaUi::Paper(),1,Vec4(0.f),0,L"SkillKey"+to_wstring(i),TextAlignment::Center);
	}

	//Q
	auto textQ = panel->AddD2DText(Vec2(139, 30), L"5", 20.0f,
		ArenaUi::Paper(), 1.0f, Vec4(0, 0, 0, 0), 1.0f,
		L"QSkillCoolDown", TextAlignment::Center);
	textQ->SetUpdateInterval(1.f);

	//W
	auto textW = panel->AddD2DText(Vec2(139 + 55 * 1, 30), L"4", 20.0f,
		ArenaUi::Paper(), 1.0f, Vec4(0, 0, 0, 0), 1.0f,
		L"WSkillCoolDown", TextAlignment::Center);
	textW->SetUpdateInterval(1.f);

	//E
	auto textE = panel->AddD2DText(Vec2(139 + 55 * 2, 30), L"3", 20.0f,
		ArenaUi::Paper(), 1.0f, Vec4(0, 0, 0, 0), 1.0f,
		L"ESkillCoolDown", TextAlignment::Center);
	textE->SetUpdateInterval(1.f);

	//R
	auto textR = panel->AddD2DText(Vec2(139 + 55 * 3, 30), L"2", 20.0f,
		ArenaUi::Paper(), 1.0f, Vec4(0, 0, 0, 0), 1.0f,
		L"RSkillCoolDown", TextAlignment::Center);
	textR->SetUpdateInterval(1.f);


	wstring baseImageTag_skillLevelFive = L"UI_SkillLevelBg_Five";
	wstring baseImageTag_skillLevelThree = L"UI_SkillLevelBg_Three";

	//스킬 레벨 이미지들
	for (int i = 4; i < 8; i++)
	{
		shared_ptr<Material> cloneMaterial;
		//궁극기는 3레벨짜리로
		if (i == 7)
		{
			cloneMaterial = RESOURCES->Get<Material>(baseImageTag_skillLevelThree)->Clone();
			imageUI->AddImageLayer(i, Vec2(139 + 55 * (i - 4), 50), Vec2(60, 7), cloneMaterial, 1);
		}
		//QWE는 5레벨짜리로
		else
		{		
			cloneMaterial = RESOURCES->Get<Material>(baseImageTag_skillLevelFive)->Clone();
			imageUI->AddImageLayer(i, Vec2(139 + 55 * (i - 4), 50), Vec2(60, 7), cloneMaterial, 1);
		}
	}

}

void GameHUDPanelUI::CreateStatBars()
{
	auto panel = m_panel->GetUIPanel();
	
	//HP바 UI
	auto hpPanel = panel->AddPanel(Vec2(244, 70.f), Vec2(253, 10), nullptr, L"ChildHPPanel");
	hpPanel->AddD2DText(
		Vec2(253, 10) / 2.f,
		L"",
		10.f,
		Vec4(1.f, 1.f, 1.f, 1.f),
		1.f,
		Vec4(0.f),
		0.f,
		L"HPText",
		TextAlignment::Center
	);
	auto hpPanelImageUI = hpPanel->AddImageUI(Vec2(0.f), L"HPPanelImageUI");
	hpPanelImageUI->AddImageLayer(0, Vec2(253, 10) / 2.f, Vec2(253,10), ArenaUi::Solid(ArenaUi::Copper()), 2);

	Vec3 hpPos = hpPanelImageUI->GetGameObject()->GetTransform()->GetPosition();


	//SP바 UI
	auto spPanel = panel->AddPanel(Vec2(244, 85.f), Vec2(253, 10), nullptr, L"ChildSPPanel");
	spPanel->AddD2DText(
		Vec2(253, 10) / 2.f,
		L"",
		10.f,
		Vec4(1.f, 1.f, 1.f, 1.f),
		1.f,
		Vec4(0.f),
		0.f,
		L"SPText",
		TextAlignment::Center
	);
	auto spPanelImageUI = spPanel->AddImageUI(Vec2(0.f), L"SPPanelImageUI");
	spPanelImageUI->AddImageLayer(0, Vec2(253, 10) / 2.f, Vec2(253,10), ArenaUi::Solid(Vec4(0.39f,0.65f,0.67f,1)), 2);

	//경험치바 UI
	auto expPanel = panel->AddPanel(Vec2(244, 100.f), Vec2(253, 10), nullptr, L"ChildEXPPanel");
	expPanel->AddD2DText(
		Vec2(253, 10) / 2.f,
		L"",
		10.f,
		Vec4(1.f, 1.f, 1.f, 1.f),
		1.f,
		Vec4(0.f),
		0.f,
		L"EXPText",
		TextAlignment::Center
	);
	auto expPanelImageUI = expPanel->AddImageUI(Vec2(0.f), L"EXPPanelImageUI");
	expPanelImageUI->AddImageLayer(0, Vec2(253, 10) / 2.f, Vec2(253,10), ArenaUi::Solid(Vec4(0.49f,0.6f,0.45f,1)), 2);

}

void GameHUDPanelUI::CreateCharacterImage()
{
	auto panel = m_panel->GetUIPanel();

	//캐릭터 이미지 패널 + 레벨
	//캐릭터 초상화
	shared_ptr<Material> cloneMaterial_charLobbyImage;
	if (m_selectedCharIdx == 0) cloneMaterial_charLobbyImage = RESOURCES->Get<Material>(L"CharLobbyBianca");
	else if (m_selectedCharIdx == 1) cloneMaterial_charLobbyImage = RESOURCES->Get<Material>(L"CharLobbyNicky");

	cloneMaterial_charLobbyImage = cloneMaterial_charLobbyImage->Clone();
    cloneMaterial_charLobbyImage->GetMaterialDesc().diffuse = m_player->GetAppearanceTint();
	auto charImagePanel = panel->AddPanel(Vec2(58.f, 58.f), Vec2(100, 100), cloneMaterial_charLobbyImage, L"CharImagePanel");
	charImagePanel->GetGameObject()->GetMeshRenderer()->SetPass(9);
	auto charLevelPanel = charImagePanel->AddPanel(Vec2(10.f, 80), Vec2(30.f, 30.f), nullptr, L"LevelPanel");
	Vec3 pos = charLevelPanel->GetGameObject()->GetTransform()->GetPosition();
	charLevelPanel->GetGameObject()->GetTransform()->SetPosition(Vec3(pos.x, pos.y, pos.z - 0.01));


	charLevelPanel->AddD2DText(
		Vec2(15.f, 15.f),
		to_wstring(m_player->GetStatus().level),
		12.f,
		Vec4(1.f),
		1.f,
		Vec4(0.f),
		0.f,
		L"LevelText",
		TextAlignment::Center
	);

}

void GameHUDPanelUI::RegisterUIObject(shared_ptr<GameObject> uiObject)
{ 

}

void GameHUDPanelUI::UpdateSkillCoolDown()
{
	vector<shared_ptr<D2DText>> skillCoolDownTextUI;
	static vector<wstring> skillNames = { L"QSkillCoolDown", L"WSkillCoolDown", L"ESkillCoolDown", L"RSkillCoolDown" };

	for (const auto& skillName : skillNames) {
		skillCoolDownTextUI.push_back(m_panel->GetUIPanel()->GetD2DText(skillName));
	}

	for (int i = 0; i < 4; i++) {
		ISkill* skill = m_player->GetSkill(i);
		int skillCurCoolDown = static_cast<int>(std::ceil(skill->GetCurrentCooldown()));

		if (skillCoolDownTextUI[i]) {
			skillCoolDownTextUI[i]->SetText(to_wstring(skillCurCoolDown));
			skillCoolDownTextUI[i]->SetVisible(skillCurCoolDown > 0); // 0이면 숨김, 아니면 표시
		}
	}
}

void GameHUDPanelUI::UpdateStatBar()
{
	PlayerStatus& playerStatus = m_player->GetStatus();

	auto hpPanel = m_panel->GetUIPanel()->GetChildUIPanel(L"ChildHPPanel");
	auto spPanel = m_panel->GetUIPanel()->GetChildUIPanel(L"ChildSPPanel");
	auto expPanel = m_panel->GetUIPanel()->GetChildUIPanel(L"ChildEXPPanel");


	auto hpPanelText = hpPanel->GetD2DText(L"HPText");
	wstring hpText = to_wstring(playerStatus.hp) + L"/" + to_wstring(playerStatus.max_HP);
	hpPanelText->SetText(hpText);

	// 직접 size 수정 대신 SetLayerSize() 사용
	auto hpImageUI = hpPanel->GetImageUI(L"HPPanelImageUI");
	float ratio = ((float)playerStatus.hp / (float)playerStatus.max_HP);
	Vec2 newSize = Vec2(253.f * ratio, 10.f);
	Vec2 newPos = Vec2(253.f / 2.f - (253.f / 2.f) * (1 - ratio), 10.f / 2.f);
	hpImageUI->SetLayerSize(0, newSize);  // 레이어 0의 크기 변경
	hpImageUI->SetLayerPosition(0, newPos);


	auto spPanelText = spPanel->GetD2DText(L"SPText");
	wstring spText = to_wstring(playerStatus.stamina) + L"/" + to_wstring(playerStatus.max_Stamina);
	spPanelText->SetText(spText);

	// 직접 size 수정 대신 SetLayerSize() 사용
	auto spImageUI = spPanel->GetImageUI(L"SPPanelImageUI");
	ratio = ((float)playerStatus.stamina / (float)playerStatus.max_Stamina);
	newSize = Vec2(253.f * ratio, 10.f);
	newPos = Vec2(253.f / 2.f - (253.f / 2.f) * (1 - ratio), 10.f / 2.f);
	spImageUI->SetLayerSize(0, newSize);  // 레이어 0의 크기 변경
	spImageUI->SetLayerPosition(0, newPos);


	auto expPanelText = expPanel->GetD2DText(L"EXPText");
	wstring expText = to_wstring(playerStatus.curExp) + L"/" + to_wstring(playerStatus.curExpLimit);
	expPanelText->SetText(expText);

	// 직접 size 수정 대신 SetLayerSize() 사용
	auto expImageUI = expPanel->GetImageUI(L"EXPPanelImageUI");
	ratio = ((float)playerStatus.curExp / (float)playerStatus.curExpLimit);
	newSize = Vec2(253.f * ratio, 10.f);
	newPos = Vec2(253.f / 2.f - (253.f / 2.f) * (1 - ratio), 10.f / 2.f);
	expImageUI->SetLayerSize(0, newSize);  // 레이어 0의 크기 변경
	expImageUI->SetLayerPosition(0, newPos);
}

void GameHUDPanelUI::UpdatePlayerLevel()
{
	auto levelPanel = m_panel->GetUIPanel()->GetChildUIPanel(L"CharImagePanel")->GetChildUIPanel(L"LevelPanel");

	PlayerStatus& playerStatus = m_player->GetStatus();

	levelPanel->GetD2DText(L"LevelText")->SetText(to_wstring(playerStatus.level));
}


void GameHUDPanelUI::UpdateSkillLevelBar(int skillIndex)
{
	ISkill* skill = m_player->GetSkill(skillIndex);

	int curSkillLevel = skill->GetCurSkillLevel();
	
	curSkillLevel += 1;

	auto imageUI = m_panel->GetUIPanel()->GetImageUI(L"ImageUI");
	wstring baseNameFive = L"UI_SkillLevelBg_Five_";
	wstring baseNameThree = L"UI_SkillLevelBg_Three_";
	wstring materialName;

	shared_ptr<Material> cloneMaterial;

	if (skillIndex == 3)
	{
		materialName = baseNameThree + L"LV" + to_wstring(curSkillLevel);
		cloneMaterial = RESOURCES->Get<Material>(materialName)->Clone();
		imageUI->SetMaterial(skillIndex + 4, cloneMaterial);
	}
	else
	{
		materialName = baseNameFive + L"LV" + to_wstring(curSkillLevel);
		cloneMaterial = RESOURCES->Get<Material>(materialName)->Clone();
		imageUI->SetMaterial(skillIndex + 4, cloneMaterial);
	}

}