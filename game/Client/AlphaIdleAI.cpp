#include "pch.h"
#include "AlphaIdleAI.h"
#include "Player.h"
#include "Monster.h"

AlphaIdleAI::AlphaIdleAI(shared_ptr<Monster> _Owner)
	: Super(_Owner)
{
}

AlphaIdleAI::~AlphaIdleAI()
{
}

void AlphaIdleAI::Enter()
{
	m_Owner->GetAnimationStateMachine()->ChangeState(AnimationStateType::Wait);
	m_Owner->SetMonsterState(MonsterState::IDLE);

	wcout << L"Alpha Enter Idle AI\n";
}

void AlphaIdleAI::Update()
{
}

void AlphaIdleAI::Exit()
{
}
