#pragma once
#include "MonsterState.h"
class WolfRunState :
    public MonsterState
{
    using Super = MonsterState;
public:
    WolfRunState();
    virtual ~WolfRunState() = default;

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(MonsterStateType newState);

private:
    bool m_isAnimationStarted;
    float m_animTime;
};

