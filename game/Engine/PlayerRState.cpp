#include "pch.h"
#include "PlayerRState.h"
#include "ModelAnimator.h"

PlayerRState::PlayerRState(shared_ptr<ModelAnimator> modelAnimator)
    :Super(PlayerStateType::Skill_4)
    , m_modelAnimator(modelAnimator)
{

}

PlayerRState::~PlayerRState()
{

}

void PlayerRState::Enter()
{
    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;

    cout << "PlayerRState진입\n";
}

void PlayerRState::Update()
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
        cout << "R 스킬 시퀀스 자동 완료 감지" << endl;
    }
}

void PlayerRState::Exit()
{
    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;

    cout << "PlayerRState종료\n";
}

bool PlayerRState::CanTransitionTo(PlayerStateType newState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (m_isSkillComplete && newState == PlayerStateType::Wait)
    {
        return true;
    }
    return false;
}
