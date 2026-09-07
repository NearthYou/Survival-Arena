#include "pch.h"
#include "AlphaAnimWaitState.h"

AlphaAnimWaitState::AlphaAnimWaitState()
    : AnimationState(AnimationStateType::Wait)
{
}

AlphaAnimWaitState::~AlphaAnimWaitState()
{
}

void AlphaAnimWaitState::Enter(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;
    _animator->SetAnimationSpeed(2.f);

    // Wait 애니메이션 재생
    _animator->SetAnimationByTag(L"Wait", true);

    m_animTime = 0.0f;
    m_isAnimationStarted = true;

    //cout << "Alpha Wait 상태 진입 - Wait 애니메이션 재생 시작" << endl;
}

void AlphaAnimWaitState::Update(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    // 대기 시간 업데이트
    m_animTime += DT;

    // 애니메이션이 정상적으로 재생되고 있는지 확인
    if (m_isAnimationStarted)
    {
        wstring currentAnimTag = _animator->GetCurrentAnimationTag();
        if (currentAnimTag == L"Wait")
        {
            // Wait 애니메이션이 정상적으로 재생 중
            // 필요시 추가 로직 구현
        }
    }
}

void AlphaAnimWaitState::Exit(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    //cout << "Wait 상태 종료 - 대기 시간: " << m_animTime << "초" << endl;

    // 상태 종료 시 정리
    m_animTime = 0.0f;
    m_isAnimationStarted = false; 
    _animator->SetAnimationSpeed(1.f);
}

bool AlphaAnimWaitState::CanTransitionTo(AnimationStateType _nextState)
{
    // Wait 상태에서는 대부분의 상태로 전환 가능
    switch (_nextState)
    {
  
    case AnimationStateType::BaseAttack:
    case AnimationStateType::Skill_1:
    case AnimationStateType::Run:
    case AnimationStateType::Dying:
    case AnimationStateType::Death:
    case AnimationStateType::Trace:
        return true;
    case AnimationStateType::Wait:
        return false;  // 자기 자신으로는 전환 불가
    default:
        return false;
    }
}
