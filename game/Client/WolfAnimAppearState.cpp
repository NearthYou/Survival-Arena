#include "pch.h"
#include "WolfAnimAppearState.h"

WolfAnimAppearState::WolfAnimAppearState()
	: AnimationState(AnimationStateType::Appear)
{
}

void WolfAnimAppearState::Enter(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    m_expectedDuration = 0.f;

    _animator->PlaySequence(L"Wolf_Appear_Sequence");
    _animator->SetCurrentAnimationSpeed(m_playSpeed);
    //재생속도에 따라 애니메이션 속도들 재설정
    m_sequenceDurations = _animator->GetSequenceAnimationDurations(L"Wolf_Appear_Sequence");
    for (size_t i = 0; i < m_sequenceDurations.size(); i++)
    {
        m_sequenceDurations[i] /= m_playSpeed;
        m_expectedDuration += m_sequenceDurations[i];
    }
    _animator->SetSequenceAnimationDurations(L"Wolf_Appear_Sequence", m_sequenceDurations);


    m_animTime = 0.0f;
    m_isAppearComplete = false;

    cout << "늑대 Appear 애니메이션 시작." << endl;
}

void WolfAnimAppearState::Update(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    // 대기 시간 업데이트
    m_animTime += DT;

    // 시퀀스 재생 상태 체크
    if (m_isAnimationStarted && m_animTime >= m_expectedDuration)
    {
        // 시퀀스가 끝났으면 완료 플래그 설정
        m_isAppearComplete = true;
        cout << "늑대 Appera 시퀸스 완료." << endl;
    }
}

void WolfAnimAppearState::Exit(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    cout << "늑대 Appear  종료 " << endl;

    // 상태 종료 시 정리
    m_animTime = 0.0f;
    m_isAnimationStarted = false;
    m_isAppearComplete = false;

    //재생속도에 따라 애니메이션 속도들 원상복구  
    _animator->SetAnimationSpeed(1.f);
    for (size_t i = 0; i < m_sequenceDurations.size(); i++)
    {
        m_sequenceDurations[i] *= m_playSpeed;
    }
    _animator->SetSequenceAnimationDurations(L"Wolf_Appear_Sequence", m_sequenceDurations);

}

bool WolfAnimAppearState::CanTransitionTo(AnimationStateType _nextState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (_nextState == AnimationStateType::Wait)
    {
        return true;
    }
    return false;
}
