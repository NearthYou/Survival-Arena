#include "pch.h"
#include "NickyRunState.h"
#include "ModelAnimator.h"

NickyRunState::NickyRunState()
    : AnimationState(AnimationStateType::Move)
{
}

void NickyRunState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;
    // Run 애니메이션 재생
    animator->SetAnimationByTag(L"Run", false);  // 부드러운 전환
    animator->SetAnimationSpeed(2.f);

    m_moveTime = 0.0f;
    m_isAnimationStarted = true;

    cout << "Run 상태 진입 - Run 애니메이션 재생 시작" << endl;
}

void NickyRunState::Update(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    m_moveTime += DT;

    // 이동 애니메이션이 정상적으로 재생되고 있는지 확인
    if (m_isAnimationStarted)
    {
        wstring currentAnimTag = animator->GetCurrentAnimationTag();
        if (currentAnimTag == L"Run")
        {
            // Run 애니메이션이 정상적으로 재생 중
        }
    }
}

void NickyRunState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;
    animator->SetAnimationSpeed(1.f);
    cout << "Run 상태 종료 - 이동 시간: " << m_moveTime << "초" << endl;

    m_moveTime = 0.0f;
    m_isAnimationStarted = false;
}

bool NickyRunState::CanTransitionTo(AnimationStateType nextState)
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
        return true;
    case AnimationStateType::Run:
        return false;  // 자기 자신으로는 전환 불가
    default:
        return false;
    }
}
