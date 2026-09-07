#include "pch.h"
#include "AlphaAppearAI.h"
#include "Player.h"
#include "Monster.h"

AlphaAppearAI::AlphaAppearAI(shared_ptr<Monster> _Owner)
	: Super(_Owner)
{
}

AlphaAppearAI::~AlphaAppearAI()
{
}

void AlphaAppearAI::Enter()
{
	m_Owner->GetAnimationStateMachine()->ChangeState(AnimationStateType::Appear);
	m_Owner->SetMonsterState(MonsterState::APPEAR);

	cout << "Alpha Enter Appear AI\n";

	m_AppearAnimDuration = m_Owner->GetModelAnimator()->GetAnimationDuration(L"Appear") / 2.f;
}

void AlphaAppearAI::Update()
{
	// 시간 기반 체크 대신 시퀀스 완료 상태만 확인
	if (m_AppearAnimElapsedTime >= m_AppearAnimDuration)
	{
		m_Owner->ChangeState(L"Idle");
		return;
	}
	m_AppearAnimElapsedTime += DT;
}

void AlphaAppearAI::Exit()
{
	cout << "Alpha Exit Appear AI\n";
	m_AppearAnimElapsedTime = 0.f;
}
