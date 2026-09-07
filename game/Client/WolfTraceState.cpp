#include "pch.h"
#include "WolfTraceState.h"
#include "GameObject.h"

#include "MonsterStateMachine.h"
#include "AnimationStateMachine.h"
#include "NavMeshAgent.h"

#include "WolfTrace.h"

WolfTraceState::WolfTraceState(shared_ptr<GameObject> wolf)
    :Super(MonsterStateType::Trace)
    ,m_wolf(wolf)
{

}

void WolfTraceState::Enter()
{
    m_animTime = 0.f;
    m_isAnimationStarted = true;

    auto attackScript = m_wolf->GetComponent<WolfTrace>();
    if (attackScript) {
        attackScript->SetTarget(m_target);
        attackScript->StartTrace();
    }

    cout << "´Á´ë Trace State ÁøÀÔ\n";
}

void WolfTraceState::Update()
{
    
}

void WolfTraceState::Exit()
{
    m_animTime = 0.f;
    m_isAnimationStarted = false;

    auto attackScript = m_wolf->GetComponent<WolfTrace>();
    if (attackScript) {
        attackScript->StopTrace();
    }

    cout << "´Á´ë Trace State Á¾·á\n";
}

bool WolfTraceState::CanTransitionTo(MonsterStateType newState)
{
    switch (newState)
    {
    case MonsterStateType::Attack:
    case MonsterStateType::Death:
    case MonsterStateType::Wait:
        return true;
    default:
        return false;
    }
}