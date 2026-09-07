#include "pch.h"
#include "WolfAttackState.h"

#include "GameObject.h"

#include "MonsterStateMachine.h"
#include "Player.h"

#include "WolfBaseAttack.h"
#include "Monster.h"

WolfAttackState::WolfAttackState(shared_ptr<GameObject> wolf)
	:Super(MonsterStateType::Attack)
	, m_wolf(wolf)
{

}

void WolfAttackState::Enter()
{
	m_animTime = 0.f;
	m_isAnimationStarted = true;

	// WolfBaseAttack 컴포넌트 활성화
	auto attackScript = m_wolf->GetComponent<WolfBaseAttack>();
	if (attackScript) {
		attackScript->SetTarget(m_target);
		attackScript->StartAttack();
	}

	cout << "늑대 Attack State 진입\n";
}

void WolfAttackState::Update()
{
	m_animTime += DT;

	if (!m_isAttackComplete && m_animTime >= (36.f / 25.f) )
	{
		Vec3 otherObjPos = m_target->GetTransform()->GetPosition();
		Vec3 wolfPos = m_wolf->GetTransform()->GetPosition();
		float distance = Vec3::Distance(wolfPos, otherObjPos);

		// 공격 범위 내에 있으면 연속 공격
		if (distance <= 3.0f) // 공격 범위
		{
			cout << "연속 공격 실행" << endl;
			m_animTime = 0.f; // 타이머 리셋

			// 다음 공격 애니메이션 요청 (번갈아가며 재생)
			auto animSM = m_wolf->GetAnimationStateMachine();
			if (animSM) {
				animSM->RequestStateChange(AnimationStateType::BaseAttack);
			}
		}
		else
		{
			// 공격 범위를 벗어나면 완료 (MonsterStateMachine에서 상태 전환)
			m_isAttackComplete = true;
		}
	}
}

void WolfAttackState::Exit()
{
	m_animTime = 0.f;
	m_isAnimationStarted = false;
	m_isAttackComplete = false;
	auto attackScript = m_wolf->GetComponent<WolfBaseAttack>();
	if (attackScript) {
		attackScript->StopAttack();
	}

	cout << "늑대 Attack State 종료\n";
}

bool WolfAttackState::CanTransitionTo(MonsterStateType newState)
{
	if (newState == MonsterStateType::Trace || newState==MonsterStateType::Death)
		return true;

	if (m_isAttackComplete)
	{
		switch (newState)
		{
		case MonsterStateType::Wait:
		case MonsterStateType::Death:
			return true;
		default:
			return false;
		}
	}
}
