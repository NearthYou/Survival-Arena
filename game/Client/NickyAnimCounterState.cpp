#include "pch.h"
#include "NickyAnimCounterState.h"

NickyAnimCounterState::NickyAnimCounterState()
    : AnimationState(AnimationStateType::Counter)

{
}

void NickyAnimCounterState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    animator->PlaySequence(L"Skill_2_Counter_Sequence");
    animator->SetCurrentAnimationSpeed(m_playSpeed);
    m_expectedDuration = animator->GetAnimationDuration(L"Skill_02_Counter") / m_playSpeed;
    // 스킬 시퀀스 재생
  
    m_skillTime = 0.0f;
    m_isSkillComplete = false;

    cout << "Nicky Counter 애니메이션 재생 시작" << endl;
}

void NickyAnimCounterState::Update(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;
    m_skillTime += DT;

    // 시퀀스 재생 상태 체크
    if (!m_isSkillComplete && m_skillTime >= m_expectedDuration)
    {
        // 시퀀스가 끝났으면 완료 플래그 설정
        m_isSkillComplete = true;
        animator->StopSequence();
    }
}

void NickyAnimCounterState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    animator->SetAnimationSpeed(1.f);

    cout << "Nicky Counter 애니메이션 종료ㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈㅈ" << endl;

    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isSkillComplete = false;
    m_cachedAnimator.reset();
}

bool NickyAnimCounterState::CanTransitionTo(AnimationStateType nextState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (m_isSkillComplete && (nextState == AnimationStateType::Wait || nextState == AnimationStateType::Run))
    {
        return true;
    }
    return false;
}
