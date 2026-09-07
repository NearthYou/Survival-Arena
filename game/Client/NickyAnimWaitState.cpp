#include "pch.h"
#include "NickyAnimWaitState.h"
#include "ModelAnimator.h"

NickyAnimWaitState::NickyAnimWaitState()
    : AnimationState(AnimationStateType::Wait)
{
}

void NickyAnimWaitState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;
    
    // Wait 애니메이션 재생
    animator->SetAnimationByTag(L"Wait", false);
    animator->SetNextAnimationSpeed(m_playSpeed);

    cout << "Nicky Wait 애니메이션 재생 시작" << endl;
}

void NickyAnimWaitState::Update(shared_ptr<ModelAnimator> animator)
{
    
}

void NickyAnimWaitState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    cout << "Nicky Wait 애니메이션 재생 종료"  << endl;
    animator->SetAnimationSpeed(1.f);
}

bool NickyAnimWaitState::CanTransitionTo(AnimationStateType nextState)
{
    // Wait 상태에서는 대부분의 상태로 전환 가능
    switch (nextState)
    {
    case AnimationStateType::BaseAttack:
    case AnimationStateType::Skill_1:
    case AnimationStateType::Skill_2:
    case AnimationStateType::Skill_3:
    case AnimationStateType::Skill_4:
    case AnimationStateType::Charging:
    case AnimationStateType::Run:
    case AnimationStateType::Craft:
        return true;
    case AnimationStateType::Wait:
        return false;  // 자기 자신으로는 전환 불가
    default:
        return false;
    }
}
