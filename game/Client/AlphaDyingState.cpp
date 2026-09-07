#include "pch.h"
#include "AlphaDyingState.h"

AlphaDyingState::AlphaDyingState()
	:Super(MonsterStateType::Dying)
{

}

void AlphaDyingState::Enter()
{
	m_animTime = 0.f;
	m_isAnimationStarted = true;

	cout << "알파 Dying State 진입\n";
}

void AlphaDyingState::Update()
{

}

void AlphaDyingState::Exit()
{
	m_animTime = 0.f;
	m_isAnimationStarted = false;

	cout << "알파 Dying State 종료\n";
}

bool AlphaDyingState::CanTransitionTo(MonsterStateType newState)
{
	return false;
}