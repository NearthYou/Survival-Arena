#include "pch.h"
#include "WolfAnimTraceState.h"

WolfAnimTraceState::WolfAnimTraceState()
    : AnimationState(AnimationStateType::Trace)
{
}

void WolfAnimTraceState::Enter(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return; 
    _animator->PlaySequence(L"Wolf_Run_Sequence");
    _animator->SetCurrentAnimationSpeed(m_playSpeed);
    // Wait 애니메이션 재생
    //_animator->SetAnimationByTag(L"Run", false);
   
    m_idleTime = 0.0f;
    m_isAnimationStarted = true;

    cout << "늑대 Run 애니메이션 재생 시작" << endl;
}

void WolfAnimTraceState::Update(shared_ptr<ModelAnimator> _animator)
{
    
}

void WolfAnimTraceState::Exit(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    cout << "늑대 Run 상태 종료 " << endl;

    // 상태 종료 시 정리
    m_idleTime = 0.0f;
    m_isAnimationStarted = false;
    _animator->SetAnimationSpeed(1.f);
}

bool WolfAnimTraceState::CanTransitionTo(AnimationStateType _nextState)
{
    // Wait 상태에서는 대부분의 상태로 전환 가능
    switch (_nextState)
    {
 
    case AnimationStateType::BaseAttack:
    case AnimationStateType::Run:
    case AnimationStateType::Dying:
    case AnimationStateType::Death:
    case AnimationStateType::Wait:
        return true;
    default:
        return false;
    }
}
