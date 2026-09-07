#include "pch.h"
#include "AlphaTrace.h"
#include "GameObject.h"
#include "NavMeshAgent.h"
#include "MonsterStateMachine.h"
#include "AnimationStateMachine.h"

AlphaTrace::AlphaTrace()
{
}

void AlphaTrace::Start()
{
	m_navAgent = m_owner->GetNavMeshAgent();
}

void AlphaTrace::Update()
{
    if (!m_target || !m_owner || !m_navAgent)
        return;

    Vec3 otherObjPos = m_target->GetTransform()->GetPosition();
    Vec3 wolfPos = m_owner->GetTransform()->GetPosition();

    CalcDir(otherObjPos, wolfPos);

    m_updateTimer += DT;
    if (m_updateTimer >= m_pathUpdateInterval)
    {
        m_navAgent->SetDestination(m_target->GetTransform()->GetPosition());
        m_navAgent->SetSpeed(m_traceSpeed);
        m_updateTimer = 0.0f;
    }
}

void AlphaTrace::StartTrace()
{
    SetActive(true);
    m_updateTimer = 0.0f;
}

void AlphaTrace::StopTrace()
{
    SetActive(false);
    if (m_navAgent)
        m_navAgent->Stop();
}

void AlphaTrace::CalcDir(Vec3 otherPos, Vec3 wolfPos)
{
    Vec3 dir = otherPos - wolfPos;
    dir.Normalize();

    // 회전 계산 및 적용
    float targetYaw = atan2(dir.x, dir.z) + 3.141592f;
    Vec3 currentRotation = m_owner->GetTransform()->GetLocalRotation();
    Vec3 newRotation = Vec3(currentRotation.x, (targetYaw * 180.0f / 3.14159f), currentRotation.z);

    m_owner->GetTransform()->SetLocalRotation(newRotation);
}

bool AlphaTrace::IsInAttackRange()
{
    if (!m_target || !m_owner) return false;

    float distance = Vec3::Distance(
        m_owner->GetTransform()->GetPosition(),
        m_target->GetTransform()->GetPosition()
    );
    return distance < m_attackRange;
}
