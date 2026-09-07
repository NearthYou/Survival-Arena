#include "pch.h"
#include "AI.h"
#include "Monster.h"

AI::AI(shared_ptr<Monster> _Owner) : m_Owner(_Owner)
{
}

AI::~AI()
{
	if (m_Owner != nullptr)
		m_Owner.reset();
}
