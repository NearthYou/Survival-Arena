#include "pch.h"
#include "AlphaAnimDeathState.h"

AlphaAnimDeathState::AlphaAnimDeathState()
    : AnimationState(AnimationStateType::Death)
{
}

AlphaAnimDeathState::~AlphaAnimDeathState()
{
}

void AlphaAnimDeathState::Enter(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    _animator->SetAnimationSpeed(m_playSpeed);
    m_expectedDuration = _animator->GetAnimationDuration(L"Death") / m_playSpeed;
    // Wait 애니메이션 재생
    _animator->PlaySequence(L"Death");


    m_animTime = 0.0f;
    m_isAnimationStarted = true;
    m_isDeathComplete = false;
    cout << "Alpha Death 애니메이션 재생 시작" << endl;
}

void AlphaAnimDeathState::Update(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    // 대기 시간 업데이트
    m_animTime += DT;

  
    // 기존의 시간 기반 완료 체크를 제거하고, 시퀀스 완료만 확인
    if ((!m_isDeathComplete && m_animTime >= m_expectedDuration))
    {
        // 시퀀스가 자연스럽게 완료되었을 때만 완료 처리
        cout << "Death 시퀀스 자연 완료!" << endl;
        _animator->StopSequence();
        m_isDeathComplete = true;
    }
}

void AlphaAnimDeathState::Exit(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    cout << "Death 상태 종료" << endl;

    // 상태 종료 시 정리
    m_animTime = 0.0f;
    m_isAnimationStarted = false;
    m_isDeathComplete = false;
    _animator->SetAnimationSpeed(1.f);
}

bool AlphaAnimDeathState::CanTransitionTo(AnimationStateType _nextState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (_nextState == AnimationStateType::Dying)
        return true;
    return false;
}
