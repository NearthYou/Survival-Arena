#include "pch.h"
#include "AlphaWaitState.h"

AlphaWaitState::AlphaWaitState()
	:Super(MonsterStateType::Wait)
{

}

void AlphaWaitState::Enter()
{
	m_animTime = 0.f;
	m_isAnimationStarted = true;

	cout << "알파 Wait State 진입\n";
}

void AlphaWaitState::Update()
{

}

void AlphaWaitState::Exit()
{
	m_animTime = 0.f;
	m_isAnimationStarted = false;

	cout << "알파 Wait State 종료\n";
}

bool AlphaWaitState::CanTransitionTo(MonsterStateType newState)
{
    switch (newState)
    {
    case MonsterStateType::Wait:
        return false; // 자기 자신으로는 전환 불가
    case MonsterStateType::Run:
    case MonsterStateType::Death:
    case MonsterStateType::Dying:
    case MonsterStateType::Trace:
        return true;
    default:
        return false;
    }
}