#include "pch.h"
#include "NickyWaitState.h"

NickyWaitState::NickyWaitState()
    :Super(PlayerStateType::Wait)
{

}

NickyWaitState::~NickyWaitState()
{
}


void NickyWaitState::Enter()
{
    cout << "NickyWaitState진입\n";
}

void NickyWaitState::Update()
{

}

void NickyWaitState::Exit()
{
    cout << "NickyWaitState종료\n";
}

bool NickyWaitState::CanTransitionTo(PlayerStateType newState)
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