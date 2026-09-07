#include "pch.h"
#include "NickyWState.h"

#include "ModelAnimator.h"

#include "PlayerStateMachine.h"
#include "AnimationStateMachine.h"

NickyWState::NickyWState(shared_ptr<ModelAnimator> modelAnimator, shared_ptr<GameObject> _player)
    :Super(PlayerStateType::Skill_2)
    , m_modelAnimator(modelAnimator)
    , m_player(_player)
{

}

NickyWState::~NickyWState()
{

}

void NickyWState::Enter()
{
    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;

    m_expectedDuration = m_modelAnimator->GetAnimationDuration(L"Skill_02_Guard") + m_modelAnimator->GetAnimationDuration(L"Skill_02_Loop");
    m_expectedDuration /= 2.f;

  
    cout << "NickyWState진입\n";
}

void NickyWState::Update()
{
    m_skillTime += DT;
    
    // 시퀀스 재생 상태 체크
    if (!m_isSkillComplete && m_skillTime >= m_expectedDuration)
    {
        // 시퀀스가 끝났으면 완료 플래그 설정
        m_isSkillComplete = true;
        cout << "W 스킬 시퀀스 자동 완료 감지" << endl;
    }
}

void NickyWState::Exit()
{
    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;

    cout << "NickyWState종료\n";
}

bool NickyWState::CanTransitionTo(PlayerStateType newState)
{
    if (newState == PlayerStateType::Counter)
        return true;

    if (m_isSkillComplete && (newState == PlayerStateType::Wait || newState == PlayerStateType::Run))
    {
        return true;
    }
    return false;
}
