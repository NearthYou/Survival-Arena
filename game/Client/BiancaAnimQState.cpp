#include "pch.h"
#include "BiancaAnimQState.h"
#include "ModelAnimator.h"

BiancaAnimQState::BiancaAnimQState()
    : AnimationState(AnimationStateType::Skill_4)
{

}

void BiancaAnimQState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    animator->PlaySequence(L"Skill_1_Sequence");
    animator->SetCurrentAnimationSpeed(m_playSpeed);

    m_expectedDuration = animator->GetAnimationDuration(L"Skill_1") / m_playSpeed;
    // 스킬 시퀀스 재생
    

    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;

    cout << "BiancaQ 애니메이션 재생 시작" << endl;
}

void BiancaAnimQState::Update(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    m_skillTime += DT;

    // 시간 기반으로 완료 체크
    if (!m_isSkillComplete && m_skillTime >= m_expectedDuration)
    {
        m_isSkillComplete = true;
        // 안전하게 시퀀스 정지
        animator->StopSequence();
        wcout << L"Bianca Q 스킬 시간 기반 완료!" << endl;
    }
}

void BiancaAnimQState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    cout << "BiancaQ 애니메이션 재생 종료" << endl;

    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;
    m_cachedAnimator.reset();

    animator->SetAnimationSpeed(1.f);
}

bool BiancaAnimQState::CanTransitionTo(AnimationStateType nextState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (m_isSkillComplete && (nextState == AnimationStateType::Wait || nextState == AnimationStateType::Run))
    {
        return true;
    }
    return false;
}
