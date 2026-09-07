#pragma once
#include "MonsterState.h"
class AlphaDeathState :
    public MonsterState
{
    using Super = MonsterState;
public:
    AlphaDeathState();
    virtual ~AlphaDeathState() = default;

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(MonsterStateType newState);


private:
    bool m_isAnimationStarted;
    float m_animTime;
    bool m_isDeathComplete = false;
};

