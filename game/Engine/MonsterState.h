#pragma once

enum class MonsterStateType
{
    Wait,
    Appear,
    Run,
    Death,
    Dying,
    Attack,
    Trace
};

class MonsterState
{
public:
    MonsterState(MonsterStateType type) : m_type(type) {}
    virtual ~MonsterState() = default;

    virtual void Enter() = 0;
    virtual void Update() = 0;
    virtual void Exit() = 0;
    virtual bool CanTransitionTo(MonsterStateType newState) = 0;

    MonsterStateType GetType() const { return m_type; }

    void SetTarget(shared_ptr<GameObject> _target) { m_target = _target; }
    shared_ptr<GameObject> GetTarget() { return m_target; }

protected:
    shared_ptr<GameObject> m_target;
    MonsterStateType m_type;
    float m_expectedDuration;
};