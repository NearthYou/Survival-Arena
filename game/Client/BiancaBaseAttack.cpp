#include "pch.h"
#include "BiancaBaseAttack.h"

#include "PlayerStateMachine.h"

#include "Player.h"
#include "Monster.h"

BiancaBaseAttack::BiancaBaseAttack()
{

}

void BiancaBaseAttack::Start()
{
	m_animStateMachine = m_owner->GetAnimationStateMachine();
}

void BiancaBaseAttack::Update()
{
	m_updateTimer += DT;

	Vec3 targetPos = m_target->GetTransform()->GetPosition();
	Vec3 playerPos = m_owner->GetTransform()->GetPosition();

	float distance = Vec3::Distance(targetPos, playerPos);

	CalcDir(targetPos, playerPos);

	//if (m_updateTimer >= m_pathUpdateInterval && distance > m_attackRange)
	//{
	//	m_owner->GetNavMeshAgent()->SetDestination(targetPos);
	//	if(m_animStateMachine->GetCurrentState() != AnimationStateType::Run)
	//		m_animStateMachine->ChangeState(AnimationStateType::Run);
	//}

	//if (!m_isAttackCompleted && distance <= m_attackRange)
	//{
	//	m_isAttackCompleted = true;
	//	m_owner->GetNavMeshAgent()->Stop();
	//	m_animStateMachine->ChangeState(AnimationStateType::BaseAttack);
	//}

	if (distance >= m_attackRange && !m_isArriveToTarget)
	{
		if (m_updateTimer > 0.1f)
		{
			m_updateTimer = 0.f;
			m_owner->GetNavMeshAgent()->SetDestination(targetPos);
			//if (m_animStateMachine->GetCurrentState() != AnimationStateT
			// ype::Run)
				//m_animStateMachine->ChangeState(AnimationStateType::Run);
		}
	}
	else
	{
		CalcDir(targetPos, playerPos);
		m_owner->GetNavMeshAgent()->Stop();
		if (distance >= m_attackRange)
		{
			m_isArriveToTarget = false;
			return;
		}
		else
			m_isArriveToTarget = true;

		if (m_updateTimer >= m_attackDuration || m_owner->GetAnimationStateMachine()->GetCurrentState() != AnimationStateType::BaseAttack)
		{
			static_pointer_cast<Monster>(m_owner)->Damaged(m_owner, static_pointer_cast<Player>(m_owner)->GetStatus().hitAttack);

			m_updateTimer = 0.f;
			//m_owner->GetAnimationStateMachine()->ChangeState(AnimationStateType::BaseAttack);
		}
	}

}

void BiancaBaseAttack::StartBaseAttack()
{
	SetActive(true);
	m_updateTimer = 0.f;
	//m_isAttackCompleted = false;
}

void BiancaBaseAttack::StopBaseAttack()
{
	SetActive(false);
	m_updateTimer = 0.f;
	//m_isAttackCompleted = false;


}

void BiancaBaseAttack::CalcDir(Vec3 otherPos, Vec3 wolfPos)
{
	Vec3 dir = otherPos - wolfPos;
	dir.Normalize();

	// 회전 계산 및 적용
	float targetYaw = atan2(dir.x, dir.z) + 3.141592f;
	Vec3 currentRotation = m_owner->GetTransform()->GetLocalRotation();
	Vec3 newRotation = Vec3(currentRotation.x, (targetYaw * 180.0f / 3.14159f), currentRotation.z);

	m_owner->GetTransform()->SetLocalRotation(newRotation);
}


bool BiancaBaseAttack::IsInAttackRange()
{
	return true;
}