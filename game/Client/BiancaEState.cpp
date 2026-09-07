#include "pch.h"
#include "BiancaEState.h"
#include "ModelAnimator.h"

BiancaEState::BiancaEState(shared_ptr<ModelAnimator> modelAnimator)
    :Super(PlayerStateType::Skill_3)
    , m_modelAnimator(modelAnimator)
{

}

BiancaEState::~BiancaEState()
{

}

void BiancaEState::Enter()
{
    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;

    m_chargeTime = 0.f;
    m_durationTime = 0.f;
    m_isReleasing = false;
    m_isCharging = true;

    cout << "BiancaEState진입\n";
}

void BiancaEState::Update()
{
    UpdateChargingSkill();
}

void BiancaEState::Exit()
{
    // 상태 종료 시 정리
    m_isCharging = true;
    m_isReleasing = false;

    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;

    cout << "BiancaEState종료\n";
}

bool BiancaEState::CanTransitionTo(PlayerStateType newState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if ((m_isSkillComplete && newState == PlayerStateType::Wait) || (m_isSkillComplete && newState == PlayerStateType::Run))
    {
        return true;
    }
    return false;
}

void BiancaEState::UpdateChargingSkill()
{
    if (m_isReleasing)
    {
        ReleaseSkill();
    }
    else if (m_isCharging)
    {
        UpdateCharging();
    }
}

void BiancaEState::UpdateCharging()
{
    m_chargeTime += DT;

    if (INPUT->GetButtonUp(KEY_TYPE::E) && !INPUT->GetButton(KEY_TYPE::LCTRL))
    {
        cout << "차징 시간 : " << m_chargeTime << endl;
        m_isReleasing = true;
        m_isCharging = false;
    }
}

void BiancaEState::ReleaseSkill()
{
    m_durationTime += DT;

    if (m_durationTime >= m_chargeTime)
        m_isSkillComplete = true;
}

void BiancaEState::ForceEnd()
{
    // Release 상태에서만 강제 종료 허용
    if (!m_isReleasing)
    {
        cout << "차징 중에는 강제 종료 불가!" << endl;
        return;
    }

    cout << "BiancaQState 자연스러운 완료 처리!" << endl;

    m_isForcedEnd = true;

    m_chargeTime = (17.f / 25.f) / 2.f;
    m_durationTime = 0.f;
}


