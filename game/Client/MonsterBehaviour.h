#pragma once

#include "MonoBehaviour.h"

class MonsterBehaviour :
    public MonoBehaviour
{
    using Super = MonoBehaviour;
public:
    MonsterBehaviour();
    virtual ~MonsterBehaviour() = default;

    virtual void Start() override;
    virtual void Update() override;

    // 몬스터 전용 인터페이스
    virtual void SetTarget(shared_ptr<GameObject> target) { m_target = target; }
    virtual void SetOwner(shared_ptr<GameObject> owner) { m_owner = owner; }

protected:
    shared_ptr<GameObject> m_target;
    shared_ptr<GameObject> m_owner;
    shared_ptr<NavMeshAgent> m_navAgent;
    shared_ptr<MonsterStateMachine> m_stateMachine;
    shared_ptr<AnimationStateMachine> m_animStateMachine;
};

