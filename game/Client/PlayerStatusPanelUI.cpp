#include "pch.h"
#include "PlayerStatusPanelUI.h"

#include "UIResourceManager.h"

#include "Player.h"

const vector<wstring> charStatIconNames = {
	L"AttackPower",
	L"SkillAmpRatio",
	L"IncreaseBasicAttackDamageRatio",
	L"Defense",
	L"AttackSpeedRatio",
	L"CooldownReduction",
	L"CriticalStrikeChance",
	L"MoveSpeedRatio"
};

PlayerStatusPanelUI::PlayerStatusPanelUI(shared_ptr<Player> player)
	:m_player(player)
{

}

PlayerStatusPanelUI::~PlayerStatusPanelUI()
{

}


void PlayerStatusPanelUI::Initialize()
{
	CreatePanels();
}

void PlayerStatusPanelUI::Update()
{
	//UpdatePlayerStatus();
}

void PlayerStatusPanelUI::SetVisible(bool visible)
{
	m_isVisible = visible;
	m_panel->GetUIPanel()->SetVisible(visible);
}

void PlayerStatusPanelUI::Cleanup()
{

}


void PlayerStatusPanelUI::CreatePanels()
{
	m_panel = make_shared<GameObject>();
	m_panel->SetName(L"CharStatPanel");

	auto panel = make_shared<UIPanel>();
	m_panel->AddComponent(panel);

	panel->Create(Vec2(323.f, 768 - 57), Vec2(155, 115), Vec4(0.075f,0.105f,0.12f,0.97f), nullptr);
	m_panel->SetLayerIndex(LAYER_UI);

	auto imageUI = m_panel->GetUIPanel()->AddImageUI(Vec2(0, 0), L"ImageUI");

	PlayerStatus& playerStatus = m_player->GetStatus();
	for (int i = 0; i < charStatIconNames.size(); i++)
	{
		wstring prefixTag = L"Ico_ChaStat_";
		shared_ptr<Material> cloneMaterial_charStatIcon = RESOURCES->Get<Material>(prefixTag + charStatIconNames[i])->Clone();
		imageUI->AddImageLayer(i, Vec2(16 + (i % 2) * 70, 13 + (i / 2) * 28), Vec2(17, 17), cloneMaterial_charStatIcon, 5);
	}

	// 스탯 텍스트 설정 구조체
	struct StatTextConfig {
		int col, row;                    // 그리드 위치 (열, 행)
		function<wstring()> getValue;    // 값 가져오는 함수
		Vec4 color;                      // 텍스트 색상
		wstring name;                    // 텍스트 이름
	};
	// 소숫점 1자리로 제한하는 함수
	auto FormatFloat = [](float value, int precision = 1) -> wstring {
		std::wstringstream ss;
		ss << std::fixed << std::setprecision(precision) << value;
		return ss.str();
		};

	FormatFloat(playerStatus.hitSpeed, 1);

	// 스탯 텍스트 설정 배열
	vector<StatTextConfig> statConfigs = {
		{0, 0, [&]() { return to_wstring((int)playerStatus.hitAttack); },			ColorNormalize(Vec4(218, 187, 102, 255)), L"AttackPower"},
		{1, 0, [&]() { return to_wstring((int)playerStatus.hitAttack); },			ColorNormalize(Vec4(211, 160, 221, 255)), L"SkillAmpRatio"},
		{0, 1, [&]() { return to_wstring((int)playerStatus.hitAttack); },			ColorNormalize(Vec4(209, 120, 66 , 255)), L"IncreaseBasicAttackDamageRatio"},
		{1, 1, [&]() { return to_wstring((int)playerStatus.defense); },				ColorNormalize(Vec4(124, 175, 203, 255)), L"Defense"},
		{0, 2, [&]() { return FormatFloat(playerStatus.hitSpeed, 1); },				ColorNormalize(Vec4(171, 162, 118, 255)), L"AttackSpeedRatio"},
		{1, 2, [&]() { return to_wstring((int)playerStatus.cooldownReduction); },	ColorNormalize(Vec4(200, 200, 200, 255)), L"CooldownReduction"},
		{0, 3, [&]() { return to_wstring((int)playerStatus.hitAttack); },			ColorNormalize(Vec4(236, 96 , 113, 255)), L"CriticalStrikeChance"},
		{1, 3, [&]() { return FormatFloat(playerStatus.moveSpeed, 1); },			ColorNormalize(Vec4(200, 200, 200, 255)), L"MoveSpeedRatio"}
	};

	// 스탯 텍스트 생성
	for (const auto& config : statConfigs) {
		panel->AddD2DText(
			Vec2(16 + config.col * 70 + 20, 13 + 28 * config.row),
			config.getValue(),
			17.0f,
			config.color,
			1.0f,
			Vec4(0, 0, 0, 0),
			1.0f,
			config.name,
			TextAlignment::Center
		);
	}

	CURSCENE->AddUIObject(m_panel, true);
	CURSCENE->RegisterUIParent(m_panel);
}

void PlayerStatusPanelUI::RegisterUIObject(shared_ptr<GameObject> uiObject)
{

}

Vec4 PlayerStatusPanelUI::ColorNormalize(Vec4 input)
{
	return input / 255.f;
}

void PlayerStatusPanelUI::UpdatePlayerStatus()
{
	vector<shared_ptr<D2DText>> playerStatusTextUI;
	vector<wstring> statusNames = charStatIconNames;

	for (const auto& statusName : statusNames) {
		playerStatusTextUI.push_back(m_panel->GetUIPanel()->GetD2DText(statusName));
	}

	PlayerStatus& playerStatus = m_player->GetStatus();

	// 소숫점 1자리로 제한하는 함수
	auto FormatFloat = [](float value, int precision = 1) -> wstring {
		std::wstringstream ss;
		ss << std::fixed << std::setprecision(precision) << value;
		return ss.str();
		};

	playerStatusTextUI[0]->SetText(to_wstring((int)playerStatus.hitAttack));     // 정수
	playerStatusTextUI[1]->SetText(to_wstring((int)playerStatus.hitAttack));     // 정수
	playerStatusTextUI[2]->SetText(to_wstring((int)playerStatus.hitAttack));     // 정수
	playerStatusTextUI[3]->SetText(to_wstring((int)playerStatus.defense));       // 정수
	playerStatusTextUI[4]->SetText(FormatFloat(playerStatus.hitSpeed, 1));       // 소숫점 1자리
	playerStatusTextUI[5]->SetText(to_wstring((int)playerStatus.cooldownReduction)); // 정수
	playerStatusTextUI[6]->SetText(to_wstring((int)playerStatus.hitAttack));     // 정수
	playerStatusTextUI[7]->SetText(FormatFloat(playerStatus.moveSpeed, 1));      // 소숫점 1자리
}