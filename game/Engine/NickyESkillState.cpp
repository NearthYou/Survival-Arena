#include "pch.h"
#include "NickyESkillState.h"
#include "ModelAnimator.h"

NickyESkillState::NickyESkillState()
	: AnimationState(AnimationStateType::Skill_3)
{
}

void NickyESkillState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    animator->SetAnimationSpeed(m_playSpeed);
    m_expectedDuration = animator->GetAnimationDuration(L"Skill_03") / m_playSpeed;
    // 스킬 시퀀스 재생
    animator->PlaySequence(L"Skill_3_Sequence");

    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;

    cout << "E 스킬 상태 진입 - E 스킬 애니메이션 재생 시작" << endl;
}

void NickyESkillState::Update(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    m_skillTime += DT;

    // 시간 기반으로 완료 체크
    if (!m_isSkillComplete && m_skillTime >= m_expectedDuration)
    {
        m_isSkillComplete = true;
        // 시퀀스 정지
        animator->StopSequence();
        wcout << L"E 스킬 시간 기반 완료!" << endl;
    }
}

void NickyESkillState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;
    animator->SetAnimationSpeed(1.f);

    cout << "E 스킬 상태 종료" << endl;

    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;
    m_cachedAnimator.reset();
}

bool NickyESkillState::CanTransitionTo(AnimationStateType nextState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (m_isSkillComplete && nextState == AnimationStateType::Wait)
    {
        return true;
    }
    return false;
}
