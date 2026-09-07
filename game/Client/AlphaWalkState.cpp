#include "pch.h"
#include "AlphaWalkState.h"

AlphaWalkState::AlphaWalkState()
	:Super(MonsterStateType::Run)
{

}

void AlphaWalkState::Enter()
{
	m_animTime = 0.f;
	m_isAnimationStarted = true;

	cout << "알파 걷기 State 진입\n";
}

void AlphaWalkState::Update()
{

}

void AlphaWalkState::Exit()
{
	m_animTime = 0.f;
	m_isAnimationStarted = false;

	cout << "알파 걷기 State 종료\n";
}

bool AlphaWalkState::CanTransitionTo(MonsterStateType newState)
{
	switch (newState)
	{
	case MonsterStateType::Wait:
		return true; // 자기 자신으로는 전환 불가
	case MonsterStateType::Run:
		return false;
	case MonsterStateType::Death:
	case MonsterStateType::Dying:
		return true;
	case MonsterStateType::Appear:
		return false;
	case MonsterStateType::Trace:
		return true;
	default:
		return false;
	}
}