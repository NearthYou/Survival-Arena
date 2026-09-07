#include "pch.h"
#include "WolfDyingState.h"


WolfDyingState::WolfDyingState()
	:Super(MonsterStateType::Dying)
{

}

void WolfDyingState::Enter()
{
	m_animTime = 0.f;
	m_isAnimationStarted = true;

	cout << "알파 Dying State 진입\n";
}

void WolfDyingState::Update()
{

}

void WolfDyingState::Exit()
{
	m_animTime = 0.f;
	m_isAnimationStarted = false;

	cout << "알파 Dying State 종료\n";
}

bool WolfDyingState::CanTransitionTo(MonsterStateType newState)
{
	return false;
}