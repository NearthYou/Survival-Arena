#include "pch.h"
#include "BiancaRSkillState.h"
#include "ModelAnimator.h"

BiancaRSkillState::BiancaRSkillState()
    : AnimationState(AnimationStateType::Skill_4)
{

}

void BiancaRSkillState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    animator->SetAnimationSpeed(m_playSpeed);
    //재생속도에 따라 애니메이션 속도들 재설정
    m_sequenceDurations = animator->GetSequenceAnimationDurations(L"Skill_4_Sequence");
    for (size_t i = 0; i < m_sequenceDurations.size(); i++)
    {
        m_sequenceDurations[i] /= m_playSpeed;
    }
    animator->SetSequenceAnimationDurations(L"Skill_4_Sequence", m_sequenceDurations);

    // 스킬 시퀀스 재생
    animator->PlaySequence(L"Skill_4_Sequence");

    // 시퀀스 완료 콜백 설정
    animator->SetSequenceCompleteCallback(L"Skill_4_Sequence", [this]() {
        m_isSkillComplete = true;  // 스킬 완료 플래그 설정
        wcout << L"R 스킬 시퀀스 완료!" << endl;
        });

    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;
    m_cachedAnimator = animator;

    cout << "Skill4 상태 진입 - Skill4 애니메이션 재생 시작" << endl;
}

void BiancaRSkillState::Update(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    // 대기 시간 업데이트
    m_skillTime += DT;

    if (m_isSkillComplete)
    {
        // 스킬이 완료되면 자동으로 Wait 상태로 전환 요청
        // 실제 전환은 AnimationStateMachine에서 처리
        return;
    }

    // 시퀀스 재생 상태 체크
    if (m_isAnimationStarted && !animator->IsSequencePlaying())
    {
        // 시퀀스가 끝났으면 완료 플래그 설정
        m_isSkillComplete = true;
        cout << "R 스킬 시퀀스 자동 완료 감지" << endl;
    }
}

void BiancaRSkillState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    cout << "Skill4 상태 종료 - 대기 시간: " << m_skillTime << "초" << endl;


    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;
    m_cachedAnimator.reset();


    //재생속도에 따라 애니메이션 속도들 원상복구  
    animator->SetAnimationSpeed(1.f);
    for (size_t i = 0; i < m_sequenceDurations.size(); i++)
    {
        m_sequenceDurations[i] *= m_playSpeed;
    }
}

bool BiancaRSkillState::CanTransitionTo(AnimationStateType nextState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (m_isSkillComplete && nextState == AnimationStateType::Wait)
    {
        return true;
    }
    return false;
}
