#include "pch.h"
#include "BiancaAnimWaitState.h"
#include "ModelAnimator.h"

BiancaAnimWaitState::BiancaAnimWaitState()
    : AnimationState(AnimationStateType::Wait)
{
}

void BiancaAnimWaitState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    // Wait 애니메이션 재생
    animator->SetAnimationByTag(L"Wait", false);
    animator->SetCurrentAnimationSpeed(2.f);

    cout << "Bianca Wait  애니메이션 재생 시작" << endl;
}

void BiancaAnimWaitState::Update(shared_ptr<ModelAnimator> animator)
{
   
}

void BiancaAnimWaitState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    cout << "Bianca Wait  애니메이션 재생 종료 " << endl;
    animator->SetAnimationSpeed(1.f);
}

bool BiancaAnimWaitState::CanTransitionTo(AnimationStateType nextState)
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
