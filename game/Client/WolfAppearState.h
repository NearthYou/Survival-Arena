#pragma once
#include "MonsterState.h"
class WolfAppearState :
    public MonsterState
{
    using Super = MonsterState;
public:
    WolfAppearState();
    virtual ~WolfAppearState() = default;

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(MonsterStateType newState);

private:
    bool m_isAnimationStarted;
    float m_animTime;
    bool m_isAppearComplete = false;

    float m_expectedDuration = 0.f;
};

