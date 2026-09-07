#pragma once

#include "MonsterState.h"

class WolfAttackState :
    public MonsterState
{
    using Super = MonsterState;
public:
    WolfAttackState(shared_ptr<GameObject> wolf);
    virtual ~WolfAttackState() = default;

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(MonsterStateType newState);

public:
    void SetOtherObject(shared_ptr<GameObject> _other) { m_otherObj = _other; }


private:
    bool m_isAnimationStarted;
    float m_animTime;
    bool m_isAttackComplete = false;

    shared_ptr<GameObject> m_otherObj;
    shared_ptr<GameObject> m_wolf;
};

