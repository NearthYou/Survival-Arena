#include "pch.h"
#include "NickyRunState.h"

NickyRunState::NickyRunState()
    :Super(PlayerStateType::Run)
{

}

NickyRunState::~NickyRunState()
{
}


void NickyRunState::Enter()
{
    cout << "NickyRunState진입\n";
    m_stepSoundTime = 0.f;
    leftStep = false;
}

void NickyRunState::Update()
{
    m_stepSoundTime += DT;

    if (m_stepSoundTime > 0.27f) {
        if (leftStep) {
            SOUND->PlaySound(L"SFX/footstepAsphalt_s1.wav", 14, 0.5f);
            leftStep = false;
        }
        else {
            SOUND->PlaySound(L"SFX/footstepAsphalt_s2.wav", 14, 0.5f);
            leftStep = true;
        }
        m_stepSoundTime = 0.f;
    }
}

void NickyRunState::Exit()
{
    cout << "NickyRunState종료\n";
}

bool NickyRunState::CanTransitionTo(PlayerStateType newState)
{
    switch (newState)
    {
    case PlayerStateType::Wait:
        // 이동 완료 시에만 Wait 상태로 전환 허용
        return true;
    case PlayerStateType::Skill_1:
    case PlayerStateType::Skill_2:
    case PlayerStateType::Skill_3:
    case PlayerStateType::Skill_4:
    case PlayerStateType::Craft:
    case PlayerStateType::BaseAttack:
        return true;
    case PlayerStateType::Run:
        return false;  // 자기 자신으로는 전환 불가
    default:
        return false;
    }
}