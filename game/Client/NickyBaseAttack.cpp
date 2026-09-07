#include "pch.h"
#include "NickyBaseAttack.h"

#include "PlayerStateMachine.h"

#include "Player.h"
#include "Monster.h"

NickyBaseAttack::NickyBaseAttack()
{

}

void NickyBaseAttack::Start()
{
	m_animStateMachine = m_owner->GetAnimationStateMachine();
}

void NickyBaseAttack::Update()
{
	m_updateTimer += DT;

	Vec3 targetPos = m_target->GetTransform()->GetPosition();
	Vec3 playerPos = m_owner->GetTransform()->GetPosition();

	CalcDir(targetPos, playerPos);

	float distance = Vec3::Distance(targetPos, playerPos);

	if (distance >= m_attackRange && !m_isArriveToTarget)
	{
		if (m_updateTimer > 0.1f)
		{
			m_updateTimer = 0.f;
			m_owner->GetNavMeshAgent()->SetDestination(targetPos);
		}
	}
	else
	{
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
			static_pointer_cast<Monster>(m_target)->Damaged(m_owner, static_pointer_cast<Player>(m_owner)->GetStatus().hitAttack);


			m_updateTimer = 0.f;
			
			if (m_motionChange)
			{
				SOUND->PlaySound(L"Nicky/Nicky_atk01.wav", 0, 0.5f);
				SOUND->PlaySound(L"Nicky/Nicky_atk_hit.wav", 1, 0.5f);
				m_motionChange = !m_motionChange;
			}
			else
			{
				SOUND->PlaySound(L"Nicky/Nicky_atk02.wav", 0, 0.5f);
				SOUND->PlaySound(L"Nicky/Nicky_atk_hit.wav", 1, 0.5f);
				m_motionChange = !m_motionChange;
			}
		}
	}
}

void NickyBaseAttack::StartBaseAttack()
{
	SetActive(true);
	m_updateTimer = 0.f;
}

void NickyBaseAttack::StopBaseAttack()
{
	SetActive(false);
	m_updateTimer = 0.f;

}

void NickyBaseAttack::CalcDir(Vec3 otherPos, Vec3 wolfPos)
{
	Vec3 dir = otherPos - wolfPos;
	dir.Normalize();

	// 회전 계산 및 적용
	float targetYaw = atan2(dir.x, dir.z) + 3.141592f;
	Vec3 currentRotation = m_owner->GetTransform()->GetLocalRotation();
	Vec3 newRotation = Vec3(currentRotation.x, (targetYaw * 180.0f / 3.14159f), currentRotation.z);

	m_owner->GetTransform()->SetLocalRotation(newRotation);
}

bool NickyBaseAttack::IsInAttackRange()
{
	return true;
}