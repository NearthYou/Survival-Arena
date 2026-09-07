#pragma once
#include "MonsterState.h"
class AlphaDyingState :
    public MonsterState
{
    using Super = MonsterState;
public:
    AlphaDyingState();
    virtual ~AlphaDyingState() = default;

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(MonsterStateType newState);



private:
    bool m_isAnimationStarted;
    float m_animTime;
};

