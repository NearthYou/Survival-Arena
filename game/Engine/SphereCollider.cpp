#include "pch.h"
#include "SphereCollider.h"
#include "AABBBoxCollider.h"
#include "OBBBoxCollider.h"
#include "MeshRenderer.h"
#include "GameObject.h"
#include "Material.h"
#include "Mesh.h"

SphereCollider::SphereCollider()
	:BaseCollider(ColliderType::Sphere)
{
	m_DebugObject = make_shared<GameObject>();
	m_DebugObject->AddComponent(make_shared<MeshRenderer>());

	m_DebugObject->GetMeshRenderer()->SetMaterial(RESOURCES->Get<Material>(L"default"));
	m_DebugObject->GetMeshRenderer()->SetMesh(RESOURCES->Get<Mesh>(L"Sphere"));
	m_DebugObject->GetMeshRenderer()->GetMaterial()->SetCastShadow(false);
	m_DebugObject->GetMeshRenderer()->SetPass(3);
	
	CURSCENE->Add(m_DebugObject);
}

SphereCollider::~SphereCollider()
{
}

void SphereCollider::Update()
{
	/*m_DebugObject->SetActive(GetGameObject()->GetActive());
	m_boundingSphere.Center = GetGameObject()->GetTransform()->GetPosition() + m_offSetPos;

	Vec3 scale = GetGameObject()->GetTransform()->GetScale();

	m_boundingSphere.Radius = m_radius * max(max(scale.x, scale.y), scale.z);

	m_DebugObject->GetTransform()->SetScale(GetGameObject()->GetTransform()->GetScale() * 2.f * m_offsetScale);
	m_DebugObject->GetTransform()->SetRotation(GetGameObject()->GetTransform()->GetRotation());
	m_DebugObject->GetTransform()->SetPosition(GetGameObject()->GetTransform()->GetPosition() + m_offSetPos);*/


	m_DebugObject->SetActive(GetGameObject()->GetActive());
	m_boundingSphere.Center = GetGameObject()->GetTransform()->GetPosition() + m_offSetPos;

	Vec3 scale = GetGameObject()->GetTransform()->GetScale();
	float maxScale = max(max(scale.x, scale.y), scale.z);

	// offsetScale을 실제 충돌 판정에도 적용
	float offsetScaleMax = max(max(m_offsetScale.x, m_offsetScale.y), m_offsetScale.z);
	m_boundingSphere.Radius = m_radius * maxScale * offsetScaleMax;

	// 디버그 오브젝트도 동일하게 설정
	m_DebugObject->GetTransform()->SetScale(GetGameObject()->GetTransform()->GetScale() * 2.f * m_offsetScale);
	m_DebugObject->GetTransform()->SetRotation(GetGameObject()->GetTransform()->GetRotation());
	m_DebugObject->GetTransform()->SetPosition(GetGameObject()->GetTransform()->GetPosition() + m_offSetPos);
}

bool SphereCollider::Intersects(Ray& _ray, OUT float& _distance)
{
	return m_boundingSphere.Intersects(_ray.position, _ray.direction, OUT _distance);
}

bool SphereCollider::Intersects(shared_ptr<BaseCollider>& _other)
{
	ColliderType type = _other->GetColliderType();

	switch (type) {
		case ColliderType::Sphere:
			return m_boundingSphere.Intersects(dynamic_pointer_cast<SphereCollider>(_other)->GetBoundSphere());
		case ColliderType::AABB:
			return m_boundingSphere.Intersects(dynamic_pointer_cast<AABBBoxCollider>(_other)->GetBoundingBox());
		case ColliderType::OBB:
			return m_boundingSphere.Intersects(dynamic_pointer_cast<OBBBoxCollider>(_other)->GetBoundingBox());
	}

	return false;
}
