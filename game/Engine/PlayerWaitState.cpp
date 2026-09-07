#include "pch.h"
#include "PlayerWaitState.h"


PlayerWaitState::PlayerWaitState()
	:Super(PlayerStateType::Wait)
{

}

PlayerWaitState::~PlayerWaitState()
{
}


void PlayerWaitState::Enter()
{
    cout << "PlayerWaitState진입\n";
}

void PlayerWaitState::Update()
{

}

void PlayerWaitState::Exit()
{
    cout << "PlayerWaitState종료\n";
}

bool PlayerWaitState::CanTransitionTo(PlayerStateType newState)
{
    // Wait 상태에서는 대부분의 상태로 전환 가능
    switch (newState)
    {
    case PlayerStateType::Skill_1:
    case PlayerStateType::Skill_2:
    case PlayerStateType::Skill_3:
    case PlayerStateType::Skill_4:
    case PlayerStateType::Run:
        return true;
    case PlayerStateType::Wait:
        return false;  // 자기 자신으로는 전환 불가
    default:
        return false;
    }
}