#include "pch.h"
#include "BiancaAnimRunState.h"
#include "ModelAnimator.h"

BiancaAnimRunState::BiancaAnimRunState()
    : AnimationState(AnimationStateType::Run)
{
}

void BiancaAnimRunState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    // Run 애니메이션 재생
    animator->SetAnimationByTag(L"Run", true);
    animator->SetCurrentAnimationSpeed(m_playSpeed);


    cout << "Bianca Run 애니메이션 재생 시작" << endl;
}

void BiancaAnimRunState::Update(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

 
}

void BiancaAnimRunState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    cout << "Biaca Run 애니메이션 종료 " << endl;
    animator->SetAnimationSpeed(1.f);
 
}

bool BiancaAnimRunState::CanTransitionTo(AnimationStateType nextState)
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
