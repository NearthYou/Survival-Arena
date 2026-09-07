#pragma once
#include "GameObject.h"
class BiancaQProjectile : public GameObject
{
    using Super = GameObject;
public:
	BiancaQProjectile(shared_ptr<GameObject> _owner);
	virtual ~BiancaQProjectile();
	
public:
    virtual void Start() override;
    virtual void Update() override;

    //Collision ฐüทร
    virtual void OnCollision(shared_ptr<GameObject> _other) override;
    virtual void OnCollisionEnter(shared_ptr<GameObject> _other) override;
    virtual void OnCollisionExit(shared_ptr<GameObject> _other) override;

public:
    void SetMoveTarget(Vec3& _startPos, Vec3& _endPos, float _timer);
    bool GetArrive() const { return m_arrive; }
    void SetArrive(bool _arrive) { m_arrive = _arrive; }
    void SetBaseAttack(bool _value) { m_baseAttack = _value; }

    float GetSpeed() const { return m_speed; }

private:

    bool m_baseAttack = false;
    bool m_moving = false;
    bool m_arrive = false;

    float m_speed = 30.f;
    float m_elapsedTime = 0.f;
    float m_duration = 0.f;

    Vec3 m_startPos, m_endPos;
    Vec3 m_direction;

    shared_ptr<GameObject> m_Owner;
};

