#include "pch.h"
#include "WolfAnimRunState.h"

WolfAnimRunState::WolfAnimRunState()
    : AnimationState(AnimationStateType::Run)
{
}

void WolfAnimRunState::Enter(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;
    _animator->SetAnimationSpeed(m_playSpeed);
    // Run 애니메이션 재생
    _animator->SetAnimationByTag(L"Run", false);  // 부드러운 전환

    m_moveTime = 0.0f;
    m_isAnimationStarted = true;

    cout << "늑대 애니메이션 재생 시작" << endl;
}

void WolfAnimRunState::Update(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    m_moveTime += DT;

    // 이동 애니메이션이 정상적으로 재생되고 있는지 확인
    if (m_isAnimationStarted)
    {
        wstring currentAnimTag = _animator->GetCurrentAnimationTag();
        if (currentAnimTag == L"Run")
        {
            // Run 애니메이션이 정상적으로 재생 중
        }
    }
}

void WolfAnimRunState::Exit(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    cout << "늑대 Run 상태 종료" << endl;

    m_moveTime = 0.0f;
    m_isAnimationStarted = false;
    _animator->SetAnimationSpeed(1.f);
}

bool WolfAnimRunState::CanTransitionTo(AnimationStateType _nextState)
{
    switch (_nextState)
    {
  
    case AnimationStateType::Wait:
    case AnimationStateType::BaseAttack:
        return true;
    case AnimationStateType::Death:
    case AnimationStateType::Dying:
    case AnimationStateType::Run:
        return false;  // 자기 자신으로는 전환 불가
    default:
        return false;
    }
}
