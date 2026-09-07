#include "pch.h"
#include "PlayerRunState.h"

PlayerRunState::PlayerRunState()
	:Super(PlayerStateType::Run)
{

}

PlayerRunState::~PlayerRunState()
{
}


void PlayerRunState::Enter()
{
    cout << "PlayerRunState진입\n";
}

void PlayerRunState::Update()
{

}

void PlayerRunState::Exit()
{
    cout << "PlayerRunState종료\n";
}

bool PlayerRunState::CanTransitionTo(PlayerStateType newState)
{
    switch (newState)
    {
    case PlayerStateType::Wait:
    case PlayerStateType::Skill_1:
    case PlayerStateType::Skill_2:
    case PlayerStateType::Skill_3:
    case PlayerStateType::Skill_4:
        return true;
    case PlayerStateType::Run:
        return false;  // 자기 자신으로는 전환 불가
    default:
        return false;
    }
}