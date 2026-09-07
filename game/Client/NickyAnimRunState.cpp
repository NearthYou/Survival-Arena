#include "pch.h"
#include "NickyAnimRunState.h"
#include "ModelAnimator.h"

NickyAnimRunState::NickyAnimRunState()
    : AnimationState(AnimationStateType::Run)
{
}

void NickyAnimRunState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;
    // Run 애니메이션 재생
    animator->SetAnimationByTag(L"Run", true); 
    animator->SetCurrentAnimationSpeed(m_playSpeed);
    cout << "Nicky Run 애니메이션 재생 시작" << endl;
}

void NickyAnimRunState::Update(shared_ptr<ModelAnimator> animator)
{
   
}

void NickyAnimRunState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;
    animator->SetAnimationSpeed(1.f);
    cout << "Nicky Run 애니메이션 재생 종료" << endl;  
}

bool NickyAnimRunState::CanTransitionTo(AnimationStateType nextState)
{
    switch (nextState)
    {
    case AnimationStateType::Wait:
    case AnimationStateType::BaseAttack:
    case AnimationStateType::Charging:
    case AnimationStateType::Skill_1:
    case AnimationStateType::Skill_2:
    case AnimationStateType::Skill_3:
    case AnimationStateType::Skill_4:
    case AnimationStateType::Craft:
        return true;
    case AnimationStateType::Run:
        return false;  // 자기 자신으로는 전환 불가
    default:
        return false;
    }
}
