#include "pch.h"
#include "WolfAppearAI.h"
#include "Monster.h"

WolfAppearAI::WolfAppearAI(shared_ptr<Monster> _Owner)
	: Super(_Owner)
{
}

WolfAppearAI::~WolfAppearAI()
{
}

void WolfAppearAI::Enter()
{
	//늑대 Appear는 처음 자동 실행. AnimationStateType::Appear
	//m_Owner->GetAnimationStateMachine()->ChangeState(AnimationStateType::Appear);
	//m_Owner->SetMonsterState(MonsterState::APPEAR);

	wcout << L"Enter Appear AI\n";
}

void WolfAppearAI::Update()
{
	if (m_AppearAnimElapsedTime >= m_AppearAnimDuration) {
		m_Owner->ChangeState(L"Idle");
	}
	m_AppearAnimElapsedTime += DT;
}

void WolfAppearAI::Exit()
{
	m_AppearAnimElapsedTime = 0.f;
}
