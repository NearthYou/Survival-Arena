#pragma once
#include "MonsterState.h"

class Monster;

class WolfDeathState :
    public MonsterState
{
    using Super = MonsterState;
public:
    WolfDeathState(shared_ptr<GameObject> wolf);
    virtual ~WolfDeathState() = default;

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(MonsterStateType newState);


private:
    shared_ptr<GameObject> m_wolf;
    float m_expectedDuration = 0.f;
    float m_animTime;
    bool m_isDeathComplete = false;
};

