#pragma once
#include "MonsterState.h"
class WolfTraceState :
    public MonsterState
{
    using Super = MonsterState;
public:
    WolfTraceState(shared_ptr<GameObject> wolf);
    virtual ~WolfTraceState() = default;

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(MonsterStateType newState);

public:
    void SetOtherObject(shared_ptr<GameObject> _other) { m_otherObj = _other; }

private:
    bool m_isAnimationStarted;
    float m_animTime;

    float m_speed = 4.f;
    

    shared_ptr<GameObject> m_otherObj;
    shared_ptr<GameObject> m_wolf;

    Vec3 m_startPos;
    Vec3 m_targetPos;
};

