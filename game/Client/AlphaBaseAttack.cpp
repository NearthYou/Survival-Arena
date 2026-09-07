#include "pch.h"
#include "AlphaBaseAttack.h"
#include "MonsterStateMachine.h"
#include "Player.h"
#include "Monster.h"
#include "Alpha.h"

AlphaBaseAttack::AlphaBaseAttack()
{
}

void AlphaBaseAttack::Start()
{
	m_navAgent = m_owner->GetNavMeshAgent();
	m_animStateMachine = m_owner->GetAnimationStateMachine();
}

void AlphaBaseAttack::Update()
{
    if (!m_isAttacking || !m_target || !m_owner)
        return;

    Vec3 otherObjPos = m_target->GetTransform()->GetPosition();
    Vec3 wolfPos = m_owner->GetTransform()->GetPosition();

    CalcDir(otherObjPos, wolfPos);

    // 공격 타이머 갱신
    m_attackTimer += DT;
    m_skillCoolTime -= DT;
    float distance = Vec3::Distance(wolfPos, otherObjPos);

    if (!m_isAttackComplete && m_attackTimer >= m_attackDuration)
    {
        m_attackTimer = 0.f;

        if (m_skillCoolTime <= 0)
        {
            static_pointer_cast<Alpha>(m_owner)->PlaySkill(m_target);
            m_skillCoolTime = 12.5f;
        }

        static_pointer_cast<Player>(m_target)->Damaged(static_pointer_cast<Monster>(m_owner), static_pointer_cast<Monster>(m_owner)->GetMonsterStatus().adPower);
    }

}

void AlphaBaseAttack::StartAttack()
{
    SetActive(true);
    m_isAttacking = true;
    m_isAttackComplete = false;
    m_attackTimer = 0.0f;
}

void AlphaBaseAttack::StopAttack()
{
    SetActive(false);
    m_isAttacking = false;
    m_attackTimer = 0.0f;
}

void AlphaBaseAttack::CalcDir(Vec3 otherPos, Vec3 wolfPos)
{
    Vec3 dir = otherPos - wolfPos;
    dir.Normalize();

    // 회전 계산 및 적용
    float targetYaw = atan2(dir.x, dir.z) + 3.141592f;
    Vec3 currentRotation = m_owner->GetTransform()->GetLocalRotation();
    Vec3 newRotation = Vec3(currentRotation.x, (targetYaw * 180.0f / 3.14159f), currentRotation.z);

    m_owner->GetTransform()->SetLocalRotation(newRotation);
}

void AlphaBaseAttack::UpdateAttackLogic()
{
}

void AlphaBaseAttack::CheckAttackDistance()
{
}

void AlphaBaseAttack::PlayAttackAnimation()
{

}
