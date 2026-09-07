#include "pch.h"
#include "BiancaESkillCircle.h"

BiancaESkillCircle::BiancaESkillCircle()
{
}

BiancaESkillCircle::~BiancaESkillCircle()
{
}

void BiancaESkillCircle::Start()
{
	Super::Start();
}

void BiancaESkillCircle::Update()
{
	Super::Update();
}

void BiancaESkillCircle::LateUpdate()
{
	Super::LateUpdate();
    // Publish completed contacts after collision detection, independent of update order.
    m_collisionSnapshot.swap(m_object);
    m_object.clear();
}

void BiancaESkillCircle::OnCollision(shared_ptr<GameObject> _other)
{
	if(m_damageFlag)
		m_object.insert(_other);
}
