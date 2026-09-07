#include "pch.h"
#include "Monster.h"
#include "AI.h"
#include "Item.h"
#include "Player.h"
#include "MonsterStateMachine.h"
#include "HealthBar.h"
#include "MonsterState.h"

Monster::Monster(shared_ptr<Shader> _shader)
	: m_defaultShader(_shader)
{
	SetType(OBJECTTYPE::MONSTER);
}

Monster::~Monster()
{
	if (m_curAI != nullptr)
		m_curAI.reset();
}

void Monster::Start()
{
	Super::Start();

	m_healthBar = make_shared<HealthBar>();
	AddComponent(m_healthBar);
	m_healthBar->Create();
}

void Monster::Update()
{
	Super::Update();


	if (m_isStun > 0.f) {
		m_isStun -= DT;
	}

	if (IsStun())
		return;


	if (m_curAI != nullptr) {
		m_curAI->Update();
	}


	if (m_healthBar) {
		m_healthBar->UpdateHealthBar(m_monsterStatus.hp, m_monsterStatus.maxHp, 0, 0);
	}
}

void Monster::LateUpdate()
{
	Super::LateUpdate();
}

void Monster::FixedUpdate()
{
	Super::FixedUpdate();
}

void Monster::ChangeState(shared_ptr<AI> _nextAI)
{
	if (_nextAI == nullptr)
		return;

	m_curAI->Exit();
	m_curAI = _nextAI;
	m_curAI->Enter();
}

void Monster::ChangeState(wstring&& _key)
{
	auto state = m_AIMap.find(_key);

	if (state == m_AIMap.end())
		wcout << "Not invalid State\n";

	m_curAI->Exit();
	m_curAI = state->second;
	m_curAI->Enter();
}

void Monster::Damaged(DamageInfo _damage)
{
	m_targetPlayer = static_pointer_cast<Player>(_damage.attacker);


	MonsterStatus info = GetMonsterStatus();

	cout << "몬스터 현재 체력 : " << info.hp << endl;

	int32 baseAttack = _damage.damage * 100;
	int32 baseDefense = 100;

	int32 finalDamage = baseAttack / baseDefense;

	int32 monsterHP = info.hp;
	monsterHP -= finalDamage;



	if (monsterHP <= 0) {
		//사망 애니메이션으로. 
		m_healthBar->SetVisible(false);
		//GetMonsterStateMachine()->ChangeState(MonsterStateType::Death);
		m_isStun = true;
	}

	if (_damage.stunTime > 0.f) {
		m_isStun = max(m_isStun, _damage.stunTime);
		SetType(OBJECTTYPE::ITEMBOX);
	}

	SetHP(monsterHP);
}

void Monster::Damaged(shared_ptr<GameObject> _attacker, int _damage)
{
	//MonsterStatus info = GetMonsterStatus();
	//cout << "몬스터 현재 체력 : " << info.hp << endl;

	//int32 baseAttack = _damage * 100;
	//int32 baseDefense = 100;

	//int32 finalDamage = baseAttack / baseDefense;

	//int32 monsterHP = info.hp;
	//monsterHP -= finalDamage;

	//if (monsterHP <= 0) {
	//	//사망 애니메이션으로. 
	//	//GetMonsterStateMachine()->ChangeState(MonsterStateType::Death);
	//	if (m_healthBar != nullptr) {
	//		m_healthBar->SetVisible(false);
	//	}
	//	Death(_attacker);
	//	SetType(OBJECTTYPE::ITEMBOX);
	//	m_isStun = true;
	//}

	///*if (_damage.stunTime > 0.f) {
	//	m_isStun = max(m_isStun, _damage.stunTime);
	//	SetType(OBJECTTYPE::ITEMBOX);
	//}*/

	//SetHP(monsterHP);

	MonsterStatus info = GetMonsterStatus();
	cout << "몬스터 현재 체력 : " << info.hp << endl;

	int32 baseAttack = _damage * 100;
	int32 baseDefense = 100;
	int32 finalDamage = baseAttack / baseDefense;
	int32 monsterHP = info.hp;
	monsterHP -= finalDamage;

	// **1. 피격시 공격자를 타겟으로 설정**
	if (m_monsterStateMachine && _attacker &&
		_attacker->GetActive() && _attacker->GetType() == OBJECTTYPE::PLAYER)
	{
		m_monsterStateMachine->SetTarget(_attacker);
		cout << "피격으로 인한 타겟 설정: " << _attacker->GetName().c_str() << endl;

		
		if (!m_monsterStateMachine->IsInState(MonsterStateType::Attack) && !m_monsterStateMachine->IsInState(MonsterStateType::Trace))
		{
			m_monsterStateMachine->RequestStateChange(MonsterStateType::Trace);
			if (m_animationStateMachine)
				m_animationStateMachine->RequestStateChange(AnimationStateType::Trace);

		}
	}

	SetHP(monsterHP);

	if (monsterHP <= 0)
	{
		cout << "몬스터 사망" << endl;

		if (m_healthBar)
			m_healthBar->SetVisible(false);

		Death(_attacker);
		SetDead(true);
		SetType(OBJECTTYPE::ITEMBOX);
	}
}


void Monster::Death(shared_ptr<GameObject> killer)
{
	// 사망 처리...

	// 경험치 보상 이벤트 발생
	int expReward = CalculateExpReward(); // 몬스터별 경험치 계산
	auto expEvent = make_shared<ExpRewardEventData>(killer, expReward, shared_from_this());
	//EVENT->TriggerEvent(expEvent);
	EVENT->QueueEvent(expEvent);

	//// 몬스터 사망 이벤트도 발생
	//auto deathEvent = make_shared<StateEventData>(EventType::MONSTER_DEATH, shared_from_this(), 0, 0);
	//EVENT->TriggerEvent(deathEvent);
}

float Monster::CalculateExpReward()
{
	return 500.f;
}