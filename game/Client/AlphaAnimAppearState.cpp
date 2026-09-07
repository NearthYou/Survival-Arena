#include "pch.h"
#include "AlphaAnimAppearState.h"

AlphaAnimAppearState::AlphaAnimAppearState()
    : AnimationState(AnimationStateType::Appear)
{
}

AlphaAnimAppearState::~AlphaAnimAppearState()
{
}

void AlphaAnimAppearState::Enter(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    _animator->PlaySequence(L"Appear");
    _animator->SetCurrentAnimationSpeed(m_playSpeed);
    m_expectedDuration = _animator->GetAnimationDuration(L"Appear") / m_playSpeed;

    m_animTime = 0.0f;
    m_isAppearComplete = false;
    cout << "Alpha Appear 애니메이션 재생 시작" << endl;
}

void AlphaAnimAppearState::Update(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    // 대기 시간 업데이트
    m_animTime += DT;

    // 기존의 시간 기반 완료 체크를 제거하고, 시퀀스 완료만 확인
    if (!m_isAppearComplete && m_animTime >= m_expectedDuration)
    {
        // 시퀀스가 자연스럽게 완료되었을 때만 완료 처리
        cout << "Appear 시퀀스 자연 완료!" << endl;
        _animator->StopSequence();
        m_isAppearComplete = true;
    }
}

void AlphaAnimAppearState::Exit(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    cout << "Alpha Appear 애니메이션 상태 종료" << endl;

    // 상태 종료 시 정리
    m_animTime = 0.0f;
    m_isAppearComplete = false;
    _animator->SetAnimationSpeed(1.f);
}

bool AlphaAnimAppearState::CanTransitionTo(AnimationStateType _nextState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (_nextState == AnimationStateType::Wait)
        return true;
    return false;
}
