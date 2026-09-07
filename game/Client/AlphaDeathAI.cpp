#include "pch.h"
#include "AlphaDeathAI.h"
#include "Player.h"
#include "Monster.h"

AlphaDeathAI::AlphaDeathAI(shared_ptr<Monster> _Owner)
	: Super(_Owner)
{
}

AlphaDeathAI::~AlphaDeathAI()
{
}

void AlphaDeathAI::Enter()
{
	m_Owner->GetAnimationStateMachine()->ChangeState(AnimationStateType::Dead);
	m_Owner->SetMonsterState(MonsterState::DIE);

	wcout << L"Alpha Enter DIE AI\n";
}

void AlphaDeathAI::Update()
{
	m_DeathAnimElapsedTime += DT;

	if (m_DeathAnimElapsedTime >= m_DeathAnimDuration || !m_Owner->GetModelAnimator()->IsSequencePlaying())
	{
		m_Owner->GetAnimationStateMachine()->ChangeState(AnimationStateType::Dying);
	}
}

void AlphaDeathAI::Exit()
{
}
