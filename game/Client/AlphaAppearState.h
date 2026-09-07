#pragma once
#include "MonsterState.h"

class AlphaAppearState :
    public MonsterState
{
    using Super = MonsterState;
public:
    AlphaAppearState();
    virtual ~AlphaAppearState() = default;

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(MonsterStateType newState);

private:
    float m_expectedDuration = 0.f;
    float m_animTime;
    bool m_isAppearComplete = false;
};

