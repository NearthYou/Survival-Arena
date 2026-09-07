#pragma once
#include "BaseCollider.h"

class MeshRenderer;
class SphereCollider :
    public BaseCollider
{
public:
    SphereCollider();
    virtual ~SphereCollider();

    virtual void Update() override;
    virtual bool Intersects(Ray& _ray, OUT float& _distance) override;
    virtual bool Intersects(shared_ptr<BaseCollider>& _other) override;

    void SetRadius(float _radius) { m_radius = _radius; }
    float GetRadius() { return m_radius; }


    BoundingSphere& GetBoundSphere() { return m_boundingSphere; }

private:
    float m_radius = 1.f;
    BoundingSphere m_boundingSphere;
};

