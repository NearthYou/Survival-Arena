#include "pch.h"
#include "WolfWaitState.h"

WolfWaitState::WolfWaitState()
    :Super(MonsterStateType::Wait)
{

}

void WolfWaitState::Enter()
{
    m_animTime = 0.f;
    m_isAnimationStarted = true;

    cout << "늑대 Wait State 진입\n";
}

void WolfWaitState::Update()
{

}

void WolfWaitState::Exit()
{
    m_animTime = 0.f;
    m_isAnimationStarted = false;

    cout << "늑대 Wait State 종료\n";
}

bool WolfWaitState::CanTransitionTo(MonsterStateType newState)
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