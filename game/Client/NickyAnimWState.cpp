#include "pch.h"
#include "NickyAnimWState.h"
#include "ModelAnimator.h"

NickyAnimWState::NickyAnimWState()
    : AnimationState(AnimationStateType::Skill_2)
{

}

void NickyAnimWState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    animator->PlaySequence(L"Skill_2_Sequence");
    animator->SetCurrentAnimationSpeed(m_playSpeed);
    // 스킬 시퀀스 재생

    m_expectedDuration = 0.f;
     //재생속도에 따라 애니메이션 속도들 재설정
    m_sequenceDurations = animator->GetSequenceAnimationDurations(L"Skill_2_Sequence");
    for (size_t i = 0; i < m_sequenceDurations.size(); i++)
    {
        m_sequenceDurations[i] /= m_playSpeed;
        m_expectedDuration += m_sequenceDurations[i];
    }
    animator->SetSequenceAnimationDurations(L"Skill_2_Sequence", m_sequenceDurations);

    cout << "AnimtionState 에서의 기대 시간 : " << m_expectedDuration << endl;
    
    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;

    cout << "Nicky W  애니메이션 재생 시작" << endl;
}

void NickyAnimWState::Update(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    // 대기 시간 업데이트
    m_skillTime += DT;
   

    // 시퀀스 재생 상태 체크
    if (!m_isSkillComplete && m_skillTime >= m_expectedDuration)
    {
        // 시퀀스가 끝났으면 완료 플래그 설정
        m_isSkillComplete = true;
        wcout << L"W 스킬 시퀀스 자동 완료 감지" << endl;
    }
}

void NickyAnimWState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;
    animator->SetAnimationSpeed(1.f);
    cout << "Nicky W 애니메이션 종료 " << endl;

    //재생속도에 따라 애니메이션 속도들 원상복구  
    for (size_t i = 0; i < m_sequenceDurations.size(); i++)
    {
        m_sequenceDurations[i] *= m_playSpeed;
    }
    animator->SetSequenceAnimationDurations(L"Skill_2_Sequence", m_sequenceDurations);

    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;
   
    m_cachedAnimator.reset();
}

bool NickyAnimWState::CanTransitionTo(AnimationStateType nextState)
{
    if (nextState == AnimationStateType::Counter)
        return true;

    if (m_isSkillComplete && (nextState == AnimationStateType::Wait || nextState == AnimationStateType::Run))
    {
        return true;
    }
    return false;
}
