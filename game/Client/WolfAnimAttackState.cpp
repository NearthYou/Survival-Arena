#include "pch.h"
#include "WolfAnimAttackState.h"
#include "ModelAnimator.h"

WolfAnimAttackState::WolfAnimAttackState()
    : AnimationState(AnimationStateType::BaseAttack)
{
}

void WolfAnimAttackState::Enter(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    _animator->SetAnimationSpeed(m_playSpeed);
    m_expectedDuration = (38.f / 25.f) / 2.f;
    if (m_motionChange)
    {
        m_sequenceDurations = _animator->GetSequenceAnimationDurations(L"Wolf_Atk1_Sequence");
        for (size_t i = 0; i < m_sequenceDurations.size(); i++)
        {
            m_sequenceDurations[i] /= m_playSpeed;
        }
        _animator->SetSequenceAnimationDurations(L"Wolf_Atk1_Sequence", m_sequenceDurations);
        _animator->PlaySequence(L"Wolf_Atk1_Sequence");
        m_motionChange = !m_motionChange;
    }
    else
    {
        m_sequenceDurations = _animator->GetSequenceAnimationDurations(L"Wolf_Atk2_Sequence");
        for (size_t i = 0; i < m_sequenceDurations.size(); i++)
        {
            m_sequenceDurations[i] /= m_playSpeed;
        }
        _animator->SetSequenceAnimationDurations(L"Wolf_Atk2_Sequence", m_sequenceDurations);
        _animator->PlaySequence(L"Wolf_Atk2_Sequence");

        m_motionChange = !m_motionChange;
    }

   
    m_animTime = 0.0f;

    m_isAttackComplete = false;
    SOUND->PlaySound(L"Wolf/wolfAttack.wav", 2, 0.5f);
    cout << "늑대 Attack 애니메이션 시작." << endl;
}

void WolfAnimAttackState::Update(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    m_animTime += DT;

    // 시퀀스 재생 상태 체크
    if (m_isAttackComplete && m_animTime >= m_expectedDuration)
    {
        // 시퀀스가 끝났으면 완료 플래그 설정
        m_isAttackComplete = true;
        cout << "늑대 Attack 애니메이션 완료." << endl;
    }
}

void WolfAnimAttackState::Exit(shared_ptr<ModelAnimator> _animator)
{
    if (!_animator)
        return;

    cout << "늑대 Attack 애니메이션 종료 " << endl;

    // 상태 종료 시 정리
    //m_animTime = 0.0f;
    m_isAnimationStarted = false;
   // m_isAppearComplete = false;

    //재생속도에 따라 애니메이션 속도들 원상복구  
    _animator->SetAnimationSpeed(1.f);
    for (size_t i = 0; i < m_sequenceDurations.size(); i++)
    {
        m_sequenceDurations[i] *= m_playSpeed;
    }
    if (m_motionChange)
    {
        _animator->SetSequenceAnimationDurations(L"Wolf_Atk1_Sequence", m_sequenceDurations);
    }
    else
    {
        _animator->SetSequenceAnimationDurations(L"Wolf_Atk2_Sequence", m_sequenceDurations);
    }

}

bool WolfAnimAttackState::CanTransitionTo(AnimationStateType _nextState)
{
  
        switch (_nextState)
        {
        case AnimationStateType::Wait:
        case AnimationStateType::Trace:
        case AnimationStateType::BaseAttack:
        case AnimationStateType::Death:
            return true;
        default:
            return false;
        }
    
    
}
