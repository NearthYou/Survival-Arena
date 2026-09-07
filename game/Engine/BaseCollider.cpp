#include "pch.h"
#include "BaseCollider.h"
#include "GameObject.h"


uint32 BaseCollider::s_nextID = 0;

BaseCollider::BaseCollider(ColliderType _type) 
	: Component(ComponentType::Collider), m_colliderType(_type), m_id(s_nextID++)
{
}

BaseCollider::~BaseCollider()
{
}

