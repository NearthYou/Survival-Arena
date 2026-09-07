#include "pch.h"
#include "BiancaAnimWState.h"

#include "ModelAnimator.h"

BiancaAnimWState::BiancaAnimWState()
    : AnimationState(AnimationStateType::Skill_4)
{

}

void BiancaAnimWState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    // 스킬 시퀀스 재생
    animator->PlaySequence(L"Skill_2_Sequence");

    m_expectedDuration = animator->GetAnimationDuration(L"Skill_2");
  
    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;

    cout << "Bianca W 애니메이션 재생 시작" << endl;
}

void BiancaAnimWState::Update(shared_ptr<ModelAnimator> animator)
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
        cout << "Bianca W 스킬 시퀀스 자동 완료 감지" << endl;
    }
}

void BiancaAnimWState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    cout << "Bianca W 애니메이션 재생 종료 " << endl;

    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;
    m_cachedAnimator.reset();
}

bool BiancaAnimWState::CanTransitionTo(AnimationStateType nextState)
{
    if (m_isSkillComplete && (nextState == AnimationStateType::Wait || nextState == AnimationStateType::Run))
    {
        return true;
    }
    return false;
}