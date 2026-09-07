#include "pch.h"
#include "AlphaAnimDyingState.h"

AlphaAnimDyingState::AlphaAnimDyingState()
    : AnimationState(AnimationStateType::Dying)
{
}

AlphaAnimDyingState::~AlphaAnimDyingState()
{
}

void AlphaAnimDyingState::Enter(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    _animator->SetAnimationSpeed(m_playSpeed);
    m_expectedDuration = _animator->GetAnimationDuration(L"Dying") / m_playSpeed;
    // Wait 애니메이션 재생
    _animator->PlaySequence(L"Dying");

    m_animTime = 0.0f;
    m_isAnimationStarted = true;
    m_isDyingComplete = false;
    cout << "Alpha Dying 애니메이션 재생 시작" << endl;
}

void AlphaAnimDyingState::Update(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    // 대기 시간 업데이트
    m_animTime += DT;

    // 기존의 시간 기반 완료 체크를 제거하고, 시퀀스 완료만 확인
    if (!m_isDyingComplete && m_animTime >= m_expectedDuration)
    {
        // 시퀀스가 자연스럽게 완료되었을 때만 완료 처리
        cout << "Dying 시퀀스 자연 완료!" << endl;
       // _animator->StopSequence();
        m_isDyingComplete = true;
    }
}

void AlphaAnimDyingState::Exit(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    cout << "Dying 상태 종료 " << endl;

    // 상태 종료 시 정리
    m_animTime = 0.0f;
    m_isAnimationStarted = false;
    m_isDyingComplete = false;
}

bool AlphaAnimDyingState::CanTransitionTo(AnimationStateType _nextState)
{
	return false;
}
