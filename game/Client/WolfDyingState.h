#pragma once
#include "MonsterState.h"
class WolfDyingState :
    public MonsterState
{
    using Super = MonsterState;
public:
    WolfDyingState();
    virtual ~WolfDyingState() = default;

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(MonsterStateType newState);



private:
    bool m_isAnimationStarted;
    float m_animTime;
};

