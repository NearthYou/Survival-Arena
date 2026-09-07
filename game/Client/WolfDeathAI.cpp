#include "pch.h"
#include "WolfDeathAI.h"
#include "Monster.h"

WolfDeathAI::WolfDeathAI(shared_ptr<Monster> _Owner)
	: Super(_Owner)
{
}

WolfDeathAI::~WolfDeathAI()
{
}

void WolfDeathAI::Enter()
{
	//m_Owner->GetAnimationStateMachine()->ChangeState(AnimationStateType::Dead);
	//m_Owner->SetMonsterState(MonsterState::DIE);

	wcout << L"Enter DIE AI\n";
}

void WolfDeathAI::Update()
{
	m_DeathAnimElapsedTime += DT;

	if (m_DeathAnimElapsedTime >= m_DeathAnimDuration) {
		m_Owner->GetAnimationStateMachine()->ChangeState(AnimationStateType::Dying);
	}
}

void WolfDeathAI::Exit()
{
}
