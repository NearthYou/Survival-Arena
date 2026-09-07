#pragma once

class Monster;
class AI
{
public:
	AI(shared_ptr<Monster> _Owner);
	virtual ~AI();
public:
	virtual void Enter() = 0;
	virtual void Update() = 0;
	virtual void Exit() = 0;

protected:
	friend class Monster;
	shared_ptr<Monster> m_Owner;
};

