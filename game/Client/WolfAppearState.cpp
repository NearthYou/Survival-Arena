#include "pch.h"
#include "WolfAppearState.h"

WolfAppearState::WolfAppearState()
	:Super(MonsterStateType::Appear)
{

}

void WolfAppearState::Enter()
{
	m_animTime = 0.f;
	m_isAppearComplete = false;

	m_expectedDuration = (60.f / 25.f) / 2.f;

	cout << "늑대 Appear State 진입\n";
}

void WolfAppearState::Update()
{
	m_animTime += DT;
	if (!m_isAppearComplete && m_animTime >= (60.f/25.f)/ 2.f)
	{
		cout << "늑대 Appear 애니메이션 완료!" << endl;
		m_isAppearComplete = true;
	}
}

void WolfAppearState::Exit()
{
	m_animTime = 0.f;
	m_isAnimationStarted = false;

	cout << "늑대 Appear State 종료\n";
}

bool WolfAppearState::CanTransitionTo(MonsterStateType newState)
{
	if (m_isAppearComplete && newState == MonsterStateType::Wait)
		return true;
	return false;
}
