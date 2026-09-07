#include "pch.h"
#include "BiancaWaitState.h"
#include "ModelAnimator.h"

BiancaWaitState::BiancaWaitState()
    : AnimationState(AnimationStateType::Wait)
{
}

void BiancaWaitState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    // Wait 애니메이션 재생
    animator->SetAnimationByTag(L"Wait", false);
    animator->SetAnimationSpeed(2.f);
    m_idleTime = 0.0f;
    m_isAnimationStarted = true;

    cout << "Wait 상태 진입 - Wait 애니메이션 재생 시작" << endl;
}

void BiancaWaitState::Update(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    // 대기 시간 업데이트
    m_idleTime += DT;

    // 애니메이션이 정상적으로 재생되고 있는지 확인
    if (m_isAnimationStarted)
    {
        wstring currentAnimTag = animator->GetCurrentAnimationTag();
        if (currentAnimTag == L"Wait")
        {
            // Wait 애니메이션이 정상적으로 재생 중
            // 필요시 추가 로직 구현
        }
    }
}

void BiancaWaitState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    cout << "Wait 상태 종료 - 대기 시간: " << m_idleTime << "초" << endl;
    animator->SetAnimationSpeed(1.f);
    // 상태 종료 시 정리
    m_idleTime = 0.0f;
    m_isAnimationStarted = false;
}

bool BiancaWaitState::CanTransitionTo(AnimationStateType nextState)
{
    // Wait 상태에서는 대부분의 상태로 전환 가능
    switch (nextState)
    {
    case AnimationStateType::Move:
    case AnimationStateType::BaseAttack:
    case AnimationStateType::Skill_1:
    case AnimationStateType::Skill_2:
    case AnimationStateType::Skill_3:
    case AnimationStateType::Skill_4:
    case AnimationStateType::Charging:
    case AnimationStateType::Run:
        return true;
    case AnimationStateType::Wait:
        return false;  // 자기 자신으로는 전환 불가
    default:
        return false;
    }
}
