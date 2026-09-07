#include "pch.h"
#include "WolfAnimDeathState.h"

WolfAnimDeathState::WolfAnimDeathState()
    : AnimationState(AnimationStateType::Death)
{
}

void WolfAnimDeathState::Enter(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    _animator->PlaySequence(L"Wolf_death_Sequence");
    _animator->SetCurrentAnimationSpeed(m_playSpeed);
    m_expectedDuration = _animator->GetAnimationDuration(L"Death") / m_playSpeed;
    // Wait 애니메이션 재생
    

    m_deathTime = 0.0f;
    m_isDeathComplete = false;
    cout << "늑대 Death 애니메이션 재생 시작" << endl;
}

void WolfAnimDeathState::Update(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    // 대기 시간 업데이트
    m_deathTime += DT;

    // 시간 기반으로 완료 체크
    if (!m_isDeathComplete && m_deathTime >= m_expectedDuration)
    {
        m_isDeathComplete = true;
        // 안전하게 시퀀스 정지
        _animator->StopSequence();
        cout << "늑대 죽는 모션 완료!" << endl;
    }
}

void WolfAnimDeathState::Exit(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    cout << "늑대 Death 상태 종료 "<< endl;

    // 상태 종료 시 정리
    m_deathTime = 0.0f;

    _animator->SetAnimationSpeed(1.f);
}

bool WolfAnimDeathState::CanTransitionTo(AnimationStateType _nextState)
{
    switch (_nextState)
    {
    case AnimationStateType::Dying:
        return true;
    default:
        return false;
    }
}
