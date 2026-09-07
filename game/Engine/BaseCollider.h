#pragma once
#include "Component.h"

enum class ColliderType {
	Sphere,
	AABB,
	OBB,
	Capsule
};

class BaseCollider : public Component
{
public:
	static uint32 s_nextID;


	BaseCollider(ColliderType _type);
	virtual ~BaseCollider();

	virtual bool Intersects(Ray& _ray, OUT float& _distance) = 0;
	virtual bool Intersects(shared_ptr<BaseCollider>& _other) = 0;


	ColliderType GetColliderType() { return m_colliderType; }

	uint32 GetID() { return m_id; }

	void SetActive(bool _active) { m_active = _active; }
	bool GetActive() { return m_active; }

	void SetOffset(Vec3 _offsetPos) { m_offSetPos = _offsetPos; }
	Vec3 GetOffset() { return m_offSetPos; }

	void SetOffsetScale(Vec3 _offsetScale) { m_offsetScale = _offsetScale; }
	Vec3 GetOffsetScale() { return m_offsetScale; }

	void SetVisible(bool _value) { m_DebugObject->SetColliderActive(_value); }

protected:
	ColliderType m_colliderType;

	uint32 m_id;			//충돌체 고유한 ID값. 
	bool m_active = true;	//충돌체 활성화 여부. 
	bool m_visible = true;

	Vec3 m_offSetPos = { 0,0,0 };
	Vec3 m_offsetScale = { 1, 1, 1 };
	shared_ptr<GameObject> m_DebugObject;
};

