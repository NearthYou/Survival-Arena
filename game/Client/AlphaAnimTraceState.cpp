#include "pch.h"
#include "AlphaAnimTraceState.h"

AlphaAnimTraceState::AlphaAnimTraceState()
	: AnimationState(AnimationStateType::Trace)
{
}

void AlphaAnimTraceState::Enter(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;
    _animator->PlaySequence(L"Run");
    _animator->SetCurrentAnimationSpeed(m_playSpeed);
   

    m_idleTime = 0.0f;
    m_isAnimationStarted = true;

    //cout << "알파 Run 애니메이션 재생 시작" << endl;
}

void AlphaAnimTraceState::Update(shared_ptr<ModelAnimator> _animator)
{
    

}

void AlphaAnimTraceState::Exit(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    //cout << "알파 Run 상태 종료 " << endl;

    // 상태 종료 시 정리
    m_idleTime = 0.0f;
    m_isAnimationStarted = false;
    _animator->SetAnimationSpeed(1.f);
}

bool AlphaAnimTraceState::CanTransitionTo(AnimationStateType _nextState)
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
