#include "pch.h"
#include "GameObject.h"
#include "AABBBoxCollider.h"
#include "SphereCollider.h"
#include "OBBBoxCollider.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "Mesh.h"

AABBBoxCollider::AABBBoxCollider()
	: BaseCollider(ColliderType::AABB)
{
	m_DebugObject = make_shared<GameObject>();
	m_DebugObject->AddComponent(make_shared<MeshRenderer>());

	m_DebugObject->GetMeshRenderer()->SetMaterial(RESOURCES->Get<Material>(L"default"));
	m_DebugObject->GetMeshRenderer()->SetMesh(RESOURCES->Get<Mesh>(L"Cube"));
	m_DebugObject->GetMeshRenderer()->GetMaterial()->SetCastShadow(false);
	m_DebugObject->GetMeshRenderer()->SetPass(3);

	CURSCENE->Add(m_DebugObject);
}

AABBBoxCollider::~AABBBoxCollider()
{
}

void AABBBoxCollider::Update()
{
	m_DebugObject->SetActive(GetGameObject()->GetActive());

	m_boundingBox.Center = GetTransform()->GetPosition() + m_offSetPos;
	m_boundingBox.Extents = GetTransform()->GetScale() * 0.5f * m_offsetScale;

	m_DebugObject->GetTransform()->SetScale(GetGameObject()->GetTransform()->GetScale() * m_offsetScale);
	//m_DebugObject->GetTransform()->SetRotation(GetGameObject()->GetTransform()->GetRotation());
	m_DebugObject->GetTransform()->SetPosition(GetGameObject()->GetTransform()->GetPosition() + m_offSetPos);
}

bool AABBBoxCollider::Intersects(Ray& _ray, OUT float& _distance)
{
	return m_boundingBox.Intersects(_ray.position, _ray.direction, OUT _distance);
}

bool AABBBoxCollider::Intersects(shared_ptr<BaseCollider>& _other)
{
	ColliderType type = _other->GetColliderType();

	switch (type) {
	case ColliderType::Sphere:
		return m_boundingBox.Intersects(dynamic_pointer_cast<SphereCollider>(_other)->GetBoundSphere());
	case ColliderType::AABB:
		return m_boundingBox.Intersects(dynamic_pointer_cast<AABBBoxCollider>(_other)->GetBoundingBox());
	case ColliderType::OBB:
		return m_boundingBox.Intersects(dynamic_pointer_cast<OBBBoxCollider>(_other)->GetBoundingBox());
	}

	return false;
}

