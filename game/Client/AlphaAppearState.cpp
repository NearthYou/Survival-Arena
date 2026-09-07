#include "pch.h"
#include "AlphaAppearState.h"

AlphaAppearState::AlphaAppearState()
	:Super(MonsterStateType::Appear)
{

}

void AlphaAppearState::Enter()
{
	m_animTime = 0.f;
	m_isAppearComplete = false;
	m_expectedDuration = (151.f / 25.f) / 2.f;
	cout << "알파 Appear State 진입\n";
}

void AlphaAppearState::Update()
{
	m_animTime += DT;
	// 애니메이션 완료 조건 체크 (예: 3초 후 또는 애니메이션 시퀀스 완료 시)
	if (!m_isAppearComplete && m_animTime >= m_expectedDuration) // 3초 예시
	{
		cout << "Appear 애니메이션 완료!" << endl;
		m_isAppearComplete = true;
	}
}

void AlphaAppearState::Exit()
{
	m_animTime = 0.f;
	m_isAppearComplete = false;
	cout << "알파 Appear State 종료\n";
}

bool AlphaAppearState::CanTransitionTo(MonsterStateType newState)
{
	if (m_isAppearComplete && newState == MonsterStateType::Wait)
		return true;
	return false;
}


