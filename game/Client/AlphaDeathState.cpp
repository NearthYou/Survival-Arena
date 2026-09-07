#include "pch.h"
#include "AlphaDeathState.h"

AlphaDeathState::AlphaDeathState()
	:Super(MonsterStateType::Death)
{

}

void AlphaDeathState::Enter()
{
	m_animTime = 0.f;
	m_isAnimationStarted = true;
	m_isDeathComplete = false;
	cout << "알파 Death State 진입\n";
}

void AlphaDeathState::Update()
{
	m_animTime += DT;


	// 애니메이션 완료 조건 체크 (예: 3초 후 또는 애니메이션 시퀀스 완료 시)
	if (!m_isDeathComplete && m_animTime >= (77.f/25.f) / 2.f) // 3초 예시
	{
		cout << "Death State에서 애니메이션 완료 감지!" << endl;
		m_isDeathComplete = true;
	}
}

void AlphaDeathState::Exit()
{
	m_animTime = 0.f;
	m_isAnimationStarted = false;
	m_isDeathComplete = false;
	cout << "알파 Death State 종료\n";
}

bool AlphaDeathState::CanTransitionTo(MonsterStateType newState)
{
	if (m_isDeathComplete && newState == MonsterStateType::Dying)
		return true;
	return false;
}
