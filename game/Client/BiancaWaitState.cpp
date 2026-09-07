#include "pch.h"
#include "BiancaWaitState.h"


BiancaWaitState::BiancaWaitState()
    :Super(PlayerStateType::Wait)
{

}

BiancaWaitState::~BiancaWaitState()
{
}


void BiancaWaitState::Enter()
{
    cout << "BiancaWaitState진입\n";
}

void BiancaWaitState::Update()
{

}

void BiancaWaitState::Exit()
{
    cout << "BiancaWaitState종료\n";
}

bool BiancaWaitState::CanTransitionTo(PlayerStateType newState)
{
    // Wait 상태에서는 대부분의 상태로 전환 가능
    switch (newState)
    {
    case PlayerStateType::Run:          // 이동 허용
    case PlayerStateType::Skill_1:      // 스킬 허용
    case PlayerStateType::Skill_2:
    case PlayerStateType::Skill_3:
    case PlayerStateType::Skill_4:
    case PlayerStateType::Craft:        // 제작 허용
    case PlayerStateType::BaseAttack:   // 평타 허용
        return true;
    case PlayerStateType::Wait:
        return false;  // 자기 자신으로는 전환 불가
    default:
        return false;
    }
}