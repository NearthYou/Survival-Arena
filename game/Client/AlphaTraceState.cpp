#include "pch.h"
#include "AlphaTraceState.h"
#include "GameObject.h"

#include "MonsterStateMachine.h"
#include "AnimationStateMachine.h"
#include "NavMeshAgent.h"
#include "AlphaTrace.h"

AlphaTraceState::AlphaTraceState(shared_ptr<GameObject> wolf)
    :Super(MonsterStateType::Trace)
    , m_alpha(wolf)
{
}

void AlphaTraceState::Enter()
{
    m_animTime = 0.f;
    m_isAnimationStarted = true;

    auto attackScript = m_alpha->GetComponent<AlphaTrace>();
    if (attackScript) {
        attackScript->SetTarget(m_target);
        attackScript->StartTrace();
    }

    cout << "알파 Trace State 진입\n";
}

void AlphaTraceState::Update()
{
}

void AlphaTraceState::Exit()
{
    m_animTime = 0.f;
    m_isAnimationStarted = false;

    auto attackScript = m_alpha->GetComponent<AlphaTrace>();
    if (attackScript) {
        attackScript->StopTrace();
    }

    cout << "알파 Trace State 종료\n";
}

bool AlphaTraceState::CanTransitionTo(MonsterStateType newState)
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
