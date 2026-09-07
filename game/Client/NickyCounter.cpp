#include "pch.h"
#include "NickyCounter.h"

#include "PlayerStateMachine.h"

#include "Player.h"
#include "Monster.h"
#include "BaseSkill.h"
#include "NickyWSkill.h"

NickyCounter::NickyCounter()
{

}

void NickyCounter::Start()
{
	m_animStateMachine = m_owner->GetAnimationStateMachine();
}

void NickyCounter::Update()
{
	m_updateTimer += DT;

	if (m_updateTimer >= m_attackDuration)
	{		
		return;
	}
}

void NickyCounter::StartCounter()
{
	SetActive(true);
	static_pointer_cast<Monster>(m_target)->Damaged(m_owner, static_pointer_cast<Player>(m_owner)->GetStatus().hitAttack * 1.5f * static_pointer_cast<Player>(m_owner)->GetSkill(1)->GetDamageMultiplier());
	CalcDir(m_target->GetTransform()->GetPosition(), m_owner->GetTransform()->GetPosition());
	
	

	SOUND->PlaySound(L"Nicky/Nicky_skill02_GuardSuccess.wav", 21, 0.5f);
	m_updateTimer = 0.f;
}

void NickyCounter::StopCounter()
{
	SetActive(false);	
	m_updateTimer = 0.f;
}

void NickyCounter::CalcDir(Vec3 otherPos, Vec3 wolfPos)
{
	Vec3 dir = otherPos - wolfPos;
	dir.Normalize();

	// 회전 계산 및 적용
	float targetYaw = atan2(dir.x, dir.z) + 3.141592f;
	Vec3 currentRotation = m_owner->GetTransform()->GetLocalRotation();
	Vec3 newRotation = Vec3(currentRotation.x, (targetYaw * 180.0f / 3.14159f), currentRotation.z);

	m_owner->GetTransform()->SetLocalRotation(newRotation);
}

bool NickyCounter::IsInAttackRange()
{
	return true;
}
