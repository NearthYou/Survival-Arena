#pragma once

#include "MonoBehaviour.h"

class PlayerMonoBehaviour :
    public MonoBehaviour
{
    using Super = MonoBehaviour;
public:
    PlayerMonoBehaviour();
    virtual ~PlayerMonoBehaviour() = default;

    virtual void Start() override;
    virtual void Update() override;

    // 플레이어 전용 인터페이스
    virtual void SetOwner(shared_ptr<GameObject> owner) { m_owner = owner; }
    virtual void SetTarget(shared_ptr<GameObject> _target) { m_target = _target; }
    virtual shared_ptr<GameObject> GetTarget() { return m_target; }

protected:
    shared_ptr<GameObject> m_owner;
    shared_ptr<GameObject> m_target;
    shared_ptr<MonsterStateMachine> m_stateMachine;
    shared_ptr<AnimationStateMachine> m_animStateMachine;
};

