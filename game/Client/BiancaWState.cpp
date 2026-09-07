#include "pch.h"
#include "BiancaWState.h"
#include "ModelAnimator.h"

BiancaWState::BiancaWState(shared_ptr<ModelAnimator> modelAnimator)
    :Super(PlayerStateType::Skill_2)
    , m_modelAnimator(modelAnimator)
{

}

BiancaWState::~BiancaWState()
{

}

void BiancaWState::Enter()
{
    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;
    m_expectedDuration = 0.f;

    m_expectedDuration = m_modelAnimator->GetAnimationDuration(L"Skill_2");

    cout << "PlayerState 에서의 기대 시간 : " << m_expectedDuration << endl;

    cout << "BiancaWState진입\n";
}

void BiancaWState::Update()
{
    // 대기 시간 업데이트
    m_skillTime += DT;


    // 시퀀스 재생 상태 체크
    if (!m_isSkillComplete && m_skillTime >= m_expectedDuration)
    {
        // 시퀀스가 끝났으면 완료 플래그 설정
        m_isSkillComplete = true;
        cout << "W 스킬 시퀀스 자동 완료 감지" << endl;
    }
}

void BiancaWState::Exit()
{
    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;
    m_expectedDuration = 0.f;

    cout << "BiancaWState종료\n";
}

bool BiancaWState::CanTransitionTo(PlayerStateType newState)
{
    if (m_isSkillComplete && (newState == PlayerStateType::Wait || newState == PlayerStateType::Run))
    {
        return true;
    }
    return false;
}
