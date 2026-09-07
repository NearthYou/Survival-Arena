#pragma once
#include "MonsterState.h"
class AlphaWalkState :
    public MonsterState
{
    using Super = MonsterState;
public:
    AlphaWalkState();
    virtual ~AlphaWalkState() = default;

    virtual void Enter();
    virtual void Update() ;
    virtual void Exit();
    virtual bool CanTransitionTo(MonsterStateType newState);

private:
    bool m_isAnimationStarted;
    float m_animTime;
};

