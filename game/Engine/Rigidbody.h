#pragma once
#include "Component.h"

class GameObject;
class Rigidbody :
    public Component
{
    using Super = Component;
public:
    Rigidbody();
    virtual ~Rigidbody();

public:
    virtual void FixedUpdate() override;


public:
    void AddForce(Vec3 _force) { m_Force += _force; }
    void SetMass(float _mass) { m_mass = _mass; }
    float GetMass() { return m_mass; }
    float GetSpeed() { return m_velocity.Length(); }

    void SetVelocity(Vec3 _velocity) { m_velocity = _velocity; }
    Vec3 GetVelocity() { return m_velocity; }
    void AddVelocity(Vec3 _velocity) { m_velocity += _velocity; }

    void SetMaxVelocity(Vec3 _maxVelocity) { m_maxVelocity = _maxVelocity; }

    void SetStatic(bool _value) { m_static = _value; }
    bool GetStatic() { return m_static; }
private:
    void OnCollision(shared_ptr<GameObject> _other);
    void OnCollisionEnter(shared_ptr<GameObject> _other);
    void OnCollisionExit(shared_ptr<GameObject> _other);


    bool ComputePushMove(shared_ptr<BaseCollider> _my, shared_ptr<BaseCollider> _other, Vec3& _outDir, float& _outForce);

private:
    void Move();

private:
    friend class GameObject;

    Vec3 m_Force;
    Vec3 m_Accel;           //가속도 
    Vec3 m_vAccel;          //최종 가속도. 
    Vec3 m_velocity;        //속도
    Vec3 m_maxVelocity;     //최대 속력

    float m_mass;       //강체 질량
    float m_fricCoeff;  //마찰 계수

    bool m_static = false;        //벽 오브젝트인가?
};

