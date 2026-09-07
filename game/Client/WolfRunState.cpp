#include "pch.h"
#include "WolfRunState.h"


WolfRunState::WolfRunState()
	:Super(MonsterStateType::Run)
{

}

void WolfRunState::Enter()
{
	m_animTime = 0.f;
	m_isAnimationStarted = true;

	cout << "알파 Wait State 진입\n";
}

void WolfRunState::Update()
{

}

void WolfRunState::Exit()
{
	m_animTime = 0.f;
	m_isAnimationStarted = false;

	cout << "알파 Wait State 종료\n";
}

bool WolfRunState::CanTransitionTo(MonsterStateType newState)
{
	if (newState == MonsterStateType::Wait)
		return true;
	return false;
}