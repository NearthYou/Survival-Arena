#include "pch.h"
#include "BiancaQState.h"

#include "ModelAnimator.h"
#include "AnimationStateMachine.h"


BiancaQState::BiancaQState(shared_ptr<ModelAnimator> modelAnimator)
    :Super(PlayerStateType::Skill_1)
    , m_modelAnimator(modelAnimator)
{

}

BiancaQState::~BiancaQState()
{

}

void BiancaQState::Enter()
{
    m_skillTime = 0.0f;
    m_isSkillComplete = false;
    m_durationTime = 0.f;
    m_expectedDuration = m_modelAnimator->GetAnimationDuration(L"Skill_1") / 2.f;
    cout << "BiancaQState진입\n";
}

void BiancaQState::Update()
{
    UpdateNormalSkill();  
}

void BiancaQState::Exit()
{
    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isSkillComplete = false;
    cout << "BiancaQState종료\n";
}

bool BiancaQState::CanTransitionTo(PlayerStateType newState)
{
    if (m_isSkillComplete && (newState == PlayerStateType::Wait || newState == PlayerStateType::Run))
    {
        return true;
    }
    return false;
}


void BiancaQState::UpdateNormalSkill()
{
    // 대기 시간 업데이트
    m_skillTime += DT;


    // 시퀀스 재생 상태 체크
    if (!m_isSkillComplete && m_skillTime >= m_expectedDuration)
    {
        // 시퀀스가 끝났으면 완료 플래그 설정
        m_isSkillComplete = true;
        cout << "Q 스킬 시퀀스 자동 완료 감지" << endl;
    }
}

