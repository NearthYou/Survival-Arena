#pragma once
#include "MonsterState.h"


class WolfWaitState :
    public MonsterState
{
    using Super = MonsterState;
public:
    WolfWaitState();
    virtual ~WolfWaitState() = default;

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(MonsterStateType newState);

private:
    bool m_isAnimationStarted;
    float m_animTime;
};

