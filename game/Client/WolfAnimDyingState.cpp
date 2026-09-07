#include "pch.h"
#include "WolfAnimDyingState.h"

WolfAnimDyingState::WolfAnimDyingState()
	: AnimationState(AnimationStateType::Death)
{
}

void WolfAnimDyingState::Enter(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    _animator->PlaySequence(L"Wolf_dying_Sequence");
    _animator->SetCurrentAnimationSpeed(m_playSpeed);
    m_expectedDuration = _animator->GetAnimationDuration(L"Dying") / m_playSpeed;
 
 

    m_dyingTime = 0.0f;
    m_isDyingComplete = false;
    cout << "늑대 Dying 애니메이션 재생 시작" << endl;
}

void WolfAnimDyingState::Update(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    // 대기 시간 업데이트
    m_dyingTime += DT;


    // 시간 기반으로 완료 체크
    if (!m_isDyingComplete && m_dyingTime >= m_expectedDuration)
    {
        m_isDyingComplete = true;
        // 안전하게 시퀀스 정지
        //_animator->StopSequence();
        cout << "늑대 시체됨 !" << endl;
    }
}

void WolfAnimDyingState::Exit(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    cout << "늑대 Dying 상태 종료" << endl;

    // 상태 종료 시 정리
    m_dyingTime = 0.0f;
    _animator->SetAnimationSpeed(1.f);
}

bool WolfAnimDyingState::CanTransitionTo(AnimationStateType _nextState)
{
    return false;
}
