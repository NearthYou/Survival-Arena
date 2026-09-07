#pragma once
#include "BaseCollider.h"

class GameObject;
class AABBBoxCollider :
    public BaseCollider
{
public:
    AABBBoxCollider();
    virtual ~AABBBoxCollider();

    virtual void Update() override;
    virtual bool Intersects(Ray& _ray, OUT float& _distance) override;
    virtual bool Intersects(shared_ptr<BaseCollider>& _other) override;


    BoundingBox& GetBoundingBox() { return m_boundingBox; }
private:
    BoundingBox m_boundingBox;
};

