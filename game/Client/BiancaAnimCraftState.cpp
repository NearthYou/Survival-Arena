#include "pch.h"
#include "BiancaAnimCraftState.h"
#include "ModelAnimator.h"

BiancaAnimCraftState::BiancaAnimCraftState()
    : AnimationState(AnimationStateType::Craft)

{
}

void BiancaAnimCraftState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    //animator->SetAnimationSpeed(m_playSpeed);
    ////m_expectedDuration = animator->GetAnimationDuration(L"Craft") / m_playSpeed;
    //// 스킬 시퀀스 재생
    //animator->PlaySequence(L"Craft_Sequence");

    animator->SetAnimationByTag(L"Craft", false);
    animator->SetNextAnimationSpeed(m_playSpeed);

    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;

    cout << "비앙카 craft 기대시간 : " << m_expectedDuration << endl;

    cout << "Bianca Craft 애니메이션 재생 시작" << endl;
}

void BiancaAnimCraftState::Update(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    m_skillTime += DT;

    // 시간 기반으로 완료 체크
    if (!m_isSkillComplete && m_skillTime >= m_expectedDuration)
    {
        m_isSkillComplete = true;
        // 시퀀스 정지
        //animator->StopSequence();
        cout << "Craft 시간 기반 완료!" << endl;
    }
}

void BiancaAnimCraftState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;
    animator->SetAnimationSpeed(1.f);

    cout << "Bianca Craft 애니메이션 종료" << endl;

    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;
    m_cachedAnimator.reset();
}

bool BiancaAnimCraftState::CanTransitionTo(AnimationStateType nextState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (m_isSkillComplete && nextState == AnimationStateType::Wait)
    {
        return true;
    }
    return false;
}
