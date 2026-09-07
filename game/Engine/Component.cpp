#include "pch.h"
#include "Component.h"
#include "GameObject.h"
#include "Transform.h"
#include "Rigidbody.h"

Component::Component(ComponentType _type)
	: m_type(_type)
{
}

Component::~Component()
{
}



shared_ptr<GameObject> Component::GetGameObject()
{
	//lock을 해서 주게 되면 weak_ptr로 받는다? 
	//lock은 원래의 객체가 아직 살아있다면 유효한 shared_ptr반환
	//원래 객체가 파괴되어 있으면 nullptr을 반환한다. 
	//lock은 weak_ptr에서 실제 객체에 접근하고 싶을 때 사용하는 함수.
	
	//객체의 소유권 없이 안전하게 접근하려면. 
	return m_gameObject.lock();
}

shared_ptr<Transform> Component::GetTransform()
{
	return m_gameObject.lock()->GetTransform();
}

shared_ptr<Rigidbody> Component::GetRigidbody()
{
	return m_gameObject.lock()->GetRigidbody();
}
