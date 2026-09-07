#include "pch.h"
#include "WolfDeathState.h"

#include "Monster.h"
#include "GameObject.h"

WolfDeathState::WolfDeathState(shared_ptr<GameObject> wolf)
	:Super(MonsterStateType::Death)
	,m_wolf(wolf)
{

}

void WolfDeathState::Enter()
{
	m_animTime = 0.f;
	m_isDeathComplete = false;
	m_expectedDuration = (59.f / 25.f) / 2.f;
	cout << "늑대 Death State 진입\n";
}

void WolfDeathState::Update()
{
	m_animTime += DT;

	// 애니메이션 완료 조건 체크 (예: 3초 후 또는 애니메이션 시퀀스 완료 시)
	if (!m_isDeathComplete && m_animTime >= m_expectedDuration) // 3초 예시
	{
		cout << "Death State에서 애니메이션 완료 감지!" << endl;
		m_isDeathComplete = true;
	}
}

void WolfDeathState::Exit()
{
	m_animTime = 0.f;
	m_isDeathComplete = false;
	cout << "늑대 Death State 종료\n";
}

bool WolfDeathState::CanTransitionTo(MonsterStateType newState)
{
	if (m_isDeathComplete && newState == MonsterStateType::Dying)
		return true;
	return false;
}
