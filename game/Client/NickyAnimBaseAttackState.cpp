#include "pch.h"
#include "NickyAnimBaseAttackState.h"
#include "ModelAnimator.h"

#include "NickyBaseAttack.h"

NickyAnimBaseAttackState::NickyAnimBaseAttackState()
    : AnimationState(AnimationStateType::BaseAttack)

{

}

void NickyAnimBaseAttackState::Enter(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    animator->SetAnimationSpeed(m_playSpeed);

    // 모션 번갈아가기
    if (m_motionChange)
    {
        animator->SetAnimationByTag(L"BaseAttack_01", true);
        SOUND->PlaySound(L"Nicky/Nicky_atk01.wav", 0, 0.5f);
        cout << "BaseAttack_01 애니메이션 재생" << endl;
    }
    else
    {
        animator->SetAnimationByTag(L"BaseAttack_02", true);
        SOUND->PlaySound(L"Nicky/Nicky_atk02.wav", 0, 0.5f);
        cout << "BaseAttack_02 애니메이션 재생" << endl;
    }

    // 다음번을 위해 토글
    m_motionChange = !m_motionChange;

    m_skillTime = 0.0f;
    m_isSkillComplete = false;
}

void NickyAnimBaseAttackState::Update(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;

    m_skillTime += DT;

    // 시간 기반으로 완료 체크
    if (!m_isSkillComplete && m_skillTime >= (38.f / 25.f) / 2.f)
    {
        m_isSkillComplete = true;
        cout << "BaseAttack 애니메이션 완료!" << endl;
    }
}

void NickyAnimBaseAttackState::Exit(shared_ptr<ModelAnimator> animator)
{
    if (!animator)
        return;
    animator->SetAnimationSpeed(1.f);

    cout << "Nicky BaseAttack 애니메이션 종료" << endl;

    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isSkillComplete = false;
    m_cachedAnimator.reset();
}

bool NickyAnimBaseAttackState::CanTransitionTo(AnimationStateType nextState)
{
    switch (nextState)
    {
    case AnimationStateType::Wait:
        // PlayerState에서 연속 공격이 끝났다고 판단할 때만 Wait로 전환
        return m_isSkillComplete;

    case AnimationStateType::BaseAttack:
        // 연속 공격을 위해 BaseAttack -> BaseAttack 전환 허용
        return m_isSkillComplete;

    case AnimationStateType::Skill_1:
    case AnimationStateType::Skill_2:
    case AnimationStateType::Skill_3:
    case AnimationStateType::Skill_4:
    case AnimationStateType::Run:
    case AnimationStateType::Craft:
        // 다른 액션들은 언제든 전환 가능 (외부 입력 우선)
        return true;

    default:
        return false;
    }
}
