#include "pch.h"
#include "PlayerEState.h"
#include "ModelAnimator.h"

PlayerEState::PlayerEState(shared_ptr<ModelAnimator> modelAnimator, bool isChargingSkill)
	:Super(PlayerStateType::Skill_3)
    ,m_modelAnimator(modelAnimator)
    ,m_isChargingSkill(isChargingSkill)
{

}

PlayerEState::~PlayerEState()
{

}

void PlayerEState::Enter()
{
    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;

    m_chargeTime = 0.f;
    m_durationTime = 0.f;
    m_isReleasing = false;
    m_isCharging = true;

    cout << "PlayerEState진입\n";
}

void PlayerEState::Update()
{
    if (m_isChargingSkill)
    {
        UpdateChargingSkill();
    }

    else
    {
        UpdateNormalSkill();
    }
}

void PlayerEState::Exit()
{
    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;

    cout << "PlayerEState종료\n";
}

bool PlayerEState::CanTransitionTo(PlayerStateType newState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (m_isSkillComplete && newState == PlayerStateType::Wait)
    {
        return true;
    }
    return false;
}

void PlayerEState::UpdateChargingSkill()
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

void PlayerEState::UpdateCharging()
{
    m_chargeTime += DT;

    if (INPUT->GetButtonUp(KEY_TYPE::E))
    {
        cout << "차징 시간 : " << m_chargeTime << endl;
        m_isReleasing = true;
        m_isCharging = false;
    }
}

void PlayerEState::ReleaseSkill()
{
    m_durationTime += DT;

    if (m_durationTime >= m_chargeTime)
        m_isSkillComplete = true;
}

void PlayerEState::UpdateNormalSkill()
{
    // 대기 시간 업데이트
    m_skillTime += DT;

    if (m_isSkillComplete)
    {
        // 스킬이 완료되면 자동으로 Wait 상태로 전환 요청
        // 실제 전환은 AnimationStateMachine에서 처리
        return;
    }

    // 시퀀스 재생 상태 체크
    if (m_isAnimationStarted && !m_modelAnimator->IsSequencePlaying())
    {
        // 시퀀스가 끝났으면 완료 플래그 설정
        m_isSkillComplete = true;
        cout << "E 스킬 시퀀스 자동 완료 감지" << endl;
    }
}