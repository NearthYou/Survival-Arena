#pragma once
#include "BaseCollider.h"

class GameObject;
class OBBBoxCollider :
    public BaseCollider
{
public: 
    OBBBoxCollider();
    virtual ~OBBBoxCollider();

private:
    virtual void Update() override;
    virtual bool Intersects(Ray& _ray, OUT float& _distance) override;
    virtual bool Intersects(shared_ptr<BaseCollider>& _other) override;
public:

    BoundingOrientedBox& GetBoundingBox() { return m_boundingBox; }

private:
    BoundingOrientedBox m_boundingBox;
    shared_ptr<GameObject> m_DebugObject;
};

