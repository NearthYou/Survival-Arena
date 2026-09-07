#include "pch.h"
#include "NickyEState.h"
#include "ModelAnimator.h"

NickyEState::NickyEState(shared_ptr<ModelAnimator> modelAnimator)
    :Super(PlayerStateType::Skill_3)
    , m_modelAnimator(modelAnimator)
{

}

NickyEState::~NickyEState()
{

}

void NickyEState::Enter()
{
    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;
    m_durationTime = 0.f;

    m_expectedDuration = m_modelAnimator->GetAnimationDuration(L"Skill_03") / 2.f;
    // 스킬 시퀀스 재생

    cout << "NickyEState진입\n";
}

void NickyEState::Update()
{
    UpdateNormalSkill();
}

void NickyEState::Exit()
{
   

    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;
    m_durationTime = 0.f;

    cout << "NickyEState종료\n";
}

bool NickyEState::CanTransitionTo(PlayerStateType newState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (m_isSkillComplete && (newState == PlayerStateType::Wait || newState == PlayerStateType::Run))
    {
        return true;
    }
    return false;
}

void NickyEState::UpdateNormalSkill()
{
    // 대기 시간 업데이트
    m_skillTime += DT;

    // 시퀀스 재생 상태 체크
    if (!m_isSkillComplete && m_skillTime >= m_expectedDuration)
    {
        // 시퀀스가 끝났으면 완료 플래그 설정
        m_isSkillComplete = true;
        cout << "E 스킬 시퀀스 자동 완료 감지" << endl;
    }

}