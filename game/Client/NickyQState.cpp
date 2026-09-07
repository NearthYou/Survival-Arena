#include "pch.h"
#include "NickyQState.h"

#include "ModelAnimator.h"
#include "AnimationStateMachine.h"


NickyQState::NickyQState(shared_ptr<ModelAnimator> modelAnimator)
    :Super(PlayerStateType::Skill_1)
    , m_modelAnimator(modelAnimator)
{

}

NickyQState::~NickyQState()
{

}

void NickyQState::Enter()
{
    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;

    m_chargeTime = 0.f;
    m_durationTime = 0.f;
    m_isReleasing = false;
    m_isCharging = true;

    cout << "NickyQState진입\n";
}

void NickyQState::Update()
{
    UpdateChargingSkill();
}

void NickyQState::Exit()
{
    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;

    cout << "NickyQState종료\n";
}

bool NickyQState::CanTransitionTo(PlayerStateType newState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if ((m_isSkillComplete && newState == PlayerStateType::Wait) || (m_isSkillComplete && newState == PlayerStateType::Run))
    {
        return true;
    }
    return false;
}

void NickyQState::UpdateChargingSkill()
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

void NickyQState::UpdateCharging()
{
    m_chargeTime += DT;

    if (INPUT->GetButtonUp(KEY_TYPE::Q))
    {
        cout << "차징 시간 PSM: " << m_chargeTime << endl;
        m_isReleasing = true;
        m_isCharging = false;
    }
}

void NickyQState::ReleaseSkill()
{
    m_durationTime += DT;

    if (m_durationTime >= m_chargeTime)
    {
        m_isSkillComplete = true;
    }
}

void NickyQState::ForceEnd()
{
    // Release 상태에서만 강제 종료 허용
    if (!m_isReleasing)
    {
        cout << "차징 중에는 강제 종료 불가!" << endl;
        return;
    }

    cout << "NickyQState 자연스러운 완료 처리!" << endl;

    m_isForcedEnd = true;

    m_chargeTime = (13.f / 25.f) / 2.f;
    m_durationTime = 0.f;
}



