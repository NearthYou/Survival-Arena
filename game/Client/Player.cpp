#include "pch.h"
#include "Player.h"
#include "BaseSkill.h"
#include "Item.h"
#include "EquipableItem.h"
#include "GameHUDPanelUI.h"
#include "PlayerStatusPanelUI.h"
#include "UIManager.h"
#include "HealthBar.h"
#include "PlayerStateMachine.h"
#include "FogOfWar.h"
#include "Monster.h"	

Player::Player()
{

}

Player::~Player()
{
	for (auto& equipment : m_curEquipment) {
		if (equipment != nullptr)
			equipment.reset();
	}

	for (auto& item : m_inventory) {
		if (item != nullptr)
			item.reset();
	}
}

void Player::Start()
{
	Super::Start();
    SetAppearanceTint(m_appearanceTint);
	m_healthBar = make_shared<HealthBar>();
	AddComponent(m_healthBar);
	m_healthBar->Create();

	//GetComponent<FogOfWar>()->UpdateFOWSystem();
	 // 경험치 보상 이벤트 구독
	EVENT->Subscribe(EventType::MONSTER_EXP_REWARD, [this](shared_ptr<EventData> eventData) {
		auto expData = dynamic_pointer_cast<ExpRewardEventData>(eventData);
		if (expData && expData->m_killer.get() == this) {
			// 자신이 처치한 몬스터일 때만 경험치 획득
			SetCurExp(m_status.curExp + expData->m_expAmount);
		}
	});
}

void Player::Update()
{
	Super::Update();
	m_healingCoolTime += TIME->GetDeltaTime();

	if (m_healingCoolTime >= 0.25f) {
		SetHP(m_status.hp + m_status.healing);
		//SetCurExp(m_status.curExp + 20);
		m_healingCoolTime = 0.f;
	}

	for (auto& skill : m_skills) {
		if (skill != nullptr) {
			skill->Update();
		}
	}

	if (m_isStun > 0.f) {
		m_isStun -= DT;
	}

	if (m_healthBar) {
		m_healthBar->UpdateHealthBar(m_status.hp, m_status.max_HP, m_status.stamina, m_status.max_Stamina);
	}
}

void Player::LateUpdate()
{
	Super::LateUpdate();
}

void Player::FixedUpdate()
{
	Super::FixedUpdate();
}

void Player::WearEquipment(shared_ptr<EquipableItem> _item)
{
	//기존 Inventory에서 Equip빼주고. 
	//미리 빼주기 때문에 무조건 빔. 
	for (int idx = 0; idx < 10; ++idx) {
		if (_item == m_inventory[idx]) {
			m_inventory[idx] = nullptr;
			break;
		}
	}

	//Equip Index에 맞게 장착하기. 
	EquipmentType itemType = _item->GetEquipType();
	if (itemType == EquipmentType::HEAD) {
		//비어있지 않다면. 
		if (m_curEquipment[2] != nullptr) {
			//장비 해제 후, 넣어주기. 
			TakeOffEquipment(2);
			m_curEquipment[2] = _item;
		}
		else {//비어있다면 그대로 넣어주기. 
			m_curEquipment[2] = _item;
		}
	}//하술 동일. 
	else if (itemType == EquipmentType::CHEST) {
		if (m_curEquipment[1] != nullptr) {
			TakeOffEquipment(1);
			m_curEquipment[1] = _item;
		}
		else {
			m_curEquipment[1] = _item;
		}
	}
	else if (itemType == EquipmentType::ARM) {
		if (m_curEquipment[3] != nullptr) {
			TakeOffEquipment(3);
			m_curEquipment[3] = _item;
		}
		else {
			m_curEquipment[3] = _item;
		}
	}
	else if (itemType == EquipmentType::LEG) {
		if (m_curEquipment[4] != nullptr) {
			TakeOffEquipment(4);
			m_curEquipment[4] = _item;
		}
		else {
			m_curEquipment[4] = _item;
		}
	}
	else if (itemType == EquipmentType::WEAPON) {
		if (m_curEquipment[0] != nullptr) {
			TakeOffEquipment(0);
			m_curEquipment[0] = _item;
		}
		else {
			m_curEquipment[0] = _item;
		}
	}

	ApplyEquipStatus(_item->GetStatus());
}

void Player::TakeOffEquipment(int _index)
{
	if (_index < 0 || _index >= 5) return;
	if (m_curEquipment[_index] == nullptr) return;
	int emptyInventoryIDX = -1;
	for (int idx = 0; idx < 10; ++idx) {
		if (m_inventory[idx] == nullptr) {
			emptyInventoryIDX = idx;
			break;
		}
	}

	//비는 곳 없음. 
	if (emptyInventoryIDX == -1)
		return;
	ReleaseEquipStatus(m_curEquipment[_index]->GetStatus());
	m_inventory[emptyInventoryIDX] = move(m_curEquipment[_index]);
	m_curEquipment[_index] = nullptr;
	SOUND->PlaySound(L"SFX/PickUpItem.wav", 5, 0.5f);
}

void Player::StartCraftAnimation()
{
	// 이동 중지
	auto navMeshAgent = GetComponent<NavMeshAgent>();
	if (navMeshAgent)
		navMeshAgent->Stop();

	// 애니메이션 상태 전환
	auto animStateMachine = GetComponent<AnimationStateMachine>();
	//if (animStateMachine)
		//animStateMachine->ChangeState(AnimationStateType::Craft);

	// 플레이어 상태 전환
	auto playerStateMachine = GetComponent<PlayerStateMachine>();
	//if (playerStateMachine)
		//playerStateMachine->ChangeState(PlayerStateType::Craft);
}

void Player::LevelUp()
{
	if (m_status.curExp < m_status.curExpLimit)
		return;

	if (m_status.level >= 20)
		return;

	
	//총 레벨업해야되는 값 ( 예를들어 경험치가 들어왔는데 최대 exp를 훨씬 초과하는 경우. ex) 2레벨업, 3레벨업 씩 해야되는경우
	int shouldLevelUpValue = 0;
	while (m_status.curExp >= m_status.curExpLimit)
	{
		m_status.curExp -= m_status.curExpLimit;
		m_status.curExpLimit += m_growStatus.ExpLimit;
		shouldLevelUpValue++;
	}

	//m_status.curExp -= m_status.curExpLimit;
	m_status.availableSkillPoints += shouldLevelUpValue;

	SetLevel(m_status.level + shouldLevelUpValue);

	wstring levelStr = to_wstring(m_status.level);
	//Level UI에 변경해주기. 
	m_healthBar->m_healthBarPanel->GetD2DText(L"LevelText")->SetText(levelStr);

	
	//m_status.curExpLimit += m_growStatus.ExpLimit;
	m_status.max_HP				+= m_growStatus.hp * shouldLevelUpValue;
	m_status.hp					+= m_growStatus.hp * shouldLevelUpValue;

	m_status.max_Stamina		+= m_growStatus.stamina * shouldLevelUpValue;
	m_status.stamina			+= m_growStatus.stamina * shouldLevelUpValue;

	m_status.hitAttack			+= m_growStatus.HitAttack * shouldLevelUpValue;

	m_status.hitSpeed			+= m_growStatus.hitSpeed * shouldLevelUpValue;
	m_status.defense			+= m_growStatus.defense * shouldLevelUpValue;
	m_status.healing			+= m_growStatus.healing * shouldLevelUpValue;
	m_status.healing_Stamina	+= m_growStatus.healing_Stamina * shouldLevelUpValue;

	if (auto manager = m_uiManager.lock()) {
		manager->GetGameHUD()->UpdateStatBar();
		manager->GetStatusUI()->UpdatePlayerStatus();
	}
	SOUND->PlaySound(L"SFX/effect_levelup.wav", 5, 0.5f);
}

void Player::Damaged(DamageInfo _damage)
{
	PlayerStatus info = GetStatus();
	
	int32 baseAttack = _damage.damage * 100;
	int32 baseDefense = info.defense; + 100;

	int32 finalDamage = baseAttack / baseDefense;

	int32 playerHP = info.hp;
	playerHP -= finalDamage;

	if (playerHP <= 0) {
		//사망 애니메이션으로. 
		//캐릭터 사망은 보여줄 일 없을듯.
		m_isStun = true;
	}
	
	if (_damage.stunTime > 0.f) {
		m_isStun = max(m_isStun, _damage.stunTime);
	}

	SetHP(playerHP);
}

void Player::Damaged(shared_ptr<Monster> _attacker, int _damage)
{
	if (m_playerStateMachine->GetCurrentState() == PlayerStateType::Skill_2)
	{
		if (m_playerStateMachine->GetState(PlayerStateType::Counter))
		{
			m_playerStateMachine->GetState(PlayerStateType::Counter)->SetTarget(_attacker);
			
			m_playerStateMachine->RequestStateChange(PlayerStateType::Counter);
			GetAnimationStateMachine()->RequestStateChange(AnimationStateType::Counter);
		}	
	}

	PlayerStatus info = GetStatus();

	int32 baseAttack = _damage * 100;
	int32 baseDefense = info.defense + 100;

	int32 finalDamage = baseAttack / baseDefense;

	int32 playerHP = info.hp;
	playerHP -= finalDamage;

	if (playerHP <= 0) {
		//사망 애니메이션으로. 
		//캐릭터 사망은 보여줄 일 없을듯.
		m_isStun = true;
	}
	SetHP(playerHP);
}

void Player::SetLevel(int _value)
{
	m_status.level = _value; 
	if (m_status.level > 20) m_status.level = 20;

	if (auto manager = m_uiManager.lock()) {
		manager->GetGameHUD()->UpdatePlayerLevel();
	}
}

void Player::SetCurExp(int _value)
{
	m_status.curExp = _value; 
	LevelUp();

	if (auto manager = m_uiManager.lock()) {
		manager->GetGameHUD()->UpdateStatBar();
	}
}

void Player::SetHP(int32 _value)
{
	if (_value > m_status.max_HP)
		m_status.hp = m_status.max_HP;
	else
		m_status.hp = _value;

	if (auto manager = m_uiManager.lock()) {
		manager->GetGameHUD()->UpdateStatBar();
	}
	
}

void Player::SetStamina(int32 _value)
{
	if (_value > m_status.max_Stamina)
		m_status.stamina = m_status.max_Stamina;
	else
		m_status.stamina = _value;

	if (auto manager = m_uiManager.lock()) {
		manager->GetGameHUD()->UpdateStatBar();
	}
}

void Player::ApplyEquipStatus(const ItemStatus& _Equipstatus)
{
	float hpRatio = static_cast<float>(m_status.hp) / static_cast<float>(m_status.max_HP);
	float staminaRatio = static_cast<float>(m_status.hp) / static_cast<float>(m_status.max_HP);

	m_status.max_HP += _Equipstatus.maxHP;
	m_status.max_Stamina += _Equipstatus.maxSP;
	m_status.hitAttack += _Equipstatus.attackPower;
	m_status.hitSpeed += _Equipstatus.attackSpeed;
	m_status.defense += _Equipstatus.defense;
	m_status.cooldownReduction += _Equipstatus.cooldownReduction;
	m_status.healing += _Equipstatus.hpRegen;
	m_status.healing_Stamina += _Equipstatus.spRegen;

	m_status.hp = static_cast<int>(m_status.max_HP * hpRatio);
	m_status.stamina = m_status.max_Stamina * staminaRatio;

	if (auto manager = m_uiManager.lock()) {
		manager->GetGameHUD()->UpdateStatBar();
		manager->GetStatusUI()->UpdatePlayerStatus();
	}

	SOUND->PlaySound(L"SFX/equipmentinstall_underrare.wav", 5, 0.5f);
}

void Player::ReleaseEquipStatus(const ItemStatus& _Equipstatus)
{
	float hpRatio = static_cast<float>(m_status.hp) / static_cast<float>(m_status.max_HP);
	float staminaRatio = static_cast<float>(m_status.hp) / static_cast<float>(m_status.max_HP);

	m_status.max_HP -= _Equipstatus.maxHP;
	m_status.max_Stamina -= _Equipstatus.maxSP;
	m_status.hitAttack -= _Equipstatus.attackPower;
	m_status.hitSpeed -= _Equipstatus.attackSpeed;
	m_status.defense -= _Equipstatus.defense;
	m_status.cooldownReduction -= _Equipstatus.cooldownReduction;
	m_status.healing -= _Equipstatus.hpRegen;
	m_status.healing_Stamina -= _Equipstatus.spRegen;

	m_status.hp = m_status.max_HP * hpRatio;
	m_status.stamina = m_status.max_Stamina * staminaRatio;

	if (auto manager = m_uiManager.lock()) {
		manager->GetGameHUD()->UpdateStatBar();
		manager->GetStatusUI()->UpdatePlayerStatus();
	}
}


void Player::SetAppearanceTint(const Vec4& tint)
{
    m_appearanceTint = tint;
    if (auto animator = GetModelAnimator()) animator->SetDisplayTint(tint);
}
