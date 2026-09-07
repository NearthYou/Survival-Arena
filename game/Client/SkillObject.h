#pragma once

#include "GameObject.h"

class SkillObject : public GameObject
{ 
public:

	shared_ptr<GameObject> GetOwner() { return m_owner; }
	void SetOwner(shared_ptr<GameObject> _owner) { m_owner = _owner; }

private:
	shared_ptr<GameObject> m_owner;
};

