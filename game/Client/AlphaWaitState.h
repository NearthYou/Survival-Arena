#pragma once
#include "MonsterState.h"
class AlphaWaitState :
    public MonsterState
{
    using Super = MonsterState;
public:
    AlphaWaitState();
    virtual ~AlphaWaitState() = default;

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(MonsterStateType newState);

private:
    bool m_isAnimationStarted;
    float m_animTime;
};

