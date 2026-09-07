#include "pch.h"
#include "BaseAttackState.h"
#include "Player.h"
#include "Monster.h"

float BaseAttackState::s_pathUpdateInterval = 0.1f;

BaseAttackState::BaseAttackState(PlayerStateType type, shared_ptr<ModelAnimator> modelAnimator, shared_ptr<GameObject> player)
    : PlayerState(type)
    , m_modelAnimator(modelAnimator)
    , m_player(player)
{
}

void BaseAttackState::Enter()
{
    cout << "기본공격 상태 진입" << endl;

    m_attackTime = 0.0f;
    m_isMovingToTarget = true;
    m_hasDealtDamage = false;
    m_isAttackComplete = false;
    m_pathUpdateTimer = 0.0f;

    if (!m_target)
    {
        cout << "공격 타겟이 없습니다!" << endl;
        m_isAttackComplete = true;
        return;
    }
}

void BaseAttackState::Update()
{
    m_attackTime += DT;
    m_pathUpdateTimer += DT;

    if (!m_target)
    {
        cout << "타겟이 사라졌습니다." << endl;
        m_isAttackComplete = true;
        return;
    }

    // 몬스터가 죽었는지 확인
    auto monster = static_pointer_cast<Monster>(m_target);
    if (monster && monster->IsDead())
    {
        cout << "타겟이 죽었습니다." << endl;
        m_isAttackComplete = true;
        return;
    }

    if (m_isMovingToTarget)
    {
        UpdateMovementToTarget();
    }
    else
    {
        UpdateAttackLogic();
    }
}

void BaseAttackState::Exit()
{
    cout << "기본공격 상태 종료" << endl;

    // NavMeshAgent 정지
    if (m_player && m_player->GetNavMeshAgent())
    {
        m_player->GetNavMeshAgent()->Stop();
    }
}

bool BaseAttackState::CanTransitionTo(PlayerStateType newState)
{
    switch (newState)
    {
    case PlayerStateType::Wait: 
    case PlayerStateType::Run:
        return true;
  
    case PlayerStateType::Skill_1:
    case PlayerStateType::Skill_2:
    case PlayerStateType::Skill_3:
    case PlayerStateType::Skill_4:
        // 공격 완료 후에만 다른 액션 가능
        return m_isAttackComplete;
    default:
        return false;
    }
}

void BaseAttackState::UpdateMovementToTarget()
{
    if (!IsInAttackRange())
    {
        // 주기적으로 경로 업데이트
        if (m_pathUpdateTimer >= s_pathUpdateInterval)
        {
            m_pathUpdateTimer = 0.0f;
            Vec3 targetPos = m_target->GetTransform()->GetPosition();
            m_player->GetNavMeshAgent()->SetDestination(targetPos);
        }
    }
    else
    {
        // 공격 범위 내 도착
        m_player->GetNavMeshAgent()->Stop();
        m_isMovingToTarget = false;
        m_attackTime = 0.0f; // 공격 시간 리셋

        RotateToTarget();
        PlayAttackAnimation();
    }
}

void BaseAttackState::UpdateAttackLogic()
{
    // 공격 범위를 벗어났는지 체크
    if (!IsInAttackRange())
    {
        cout << "타겟이 공격 범위를 벗어남 - 다시 추격" << endl;
        m_isMovingToTarget = true;
        m_hasDealtDamage = false;
        return;
    }

    // 공격 쿨타임 체크
    if (m_attackTime >= GetAttackCooldown())
    {
        if (!m_hasDealtDamage)
        {
            DealDamage();
            m_hasDealtDamage = true;
        }

        // 다음 공격 준비 또는 완료
        if (m_attackTime >= GetAttackCooldown() * 1.5f) // 약간의 여유시간
        {
            m_isAttackComplete = true;
        }
    }
}

bool BaseAttackState::IsInAttackRange() const
{
    if (!m_target) return false;

    Vec3 playerPos = m_player->GetTransform()->GetPosition();
    Vec3 targetPos = m_target->GetTransform()->GetPosition();

    float distance = Vec3::Distance(playerPos, targetPos);
    return distance <= GetAttackRange();
}

void BaseAttackState::RotateToTarget()
{
    if (!m_target) return;

    Vec3 playerPos = m_player->GetTransform()->GetPosition();
    Vec3 targetPos = m_target->GetTransform()->GetPosition();
    Vec3 direction = targetPos - playerPos;
    direction.Normalize();

    float targetYaw = atan2(direction.x, direction.z) + 3.141592f;
    Vec3 currentRotation = m_player->GetTransform()->GetLocalRotation();
    Vec3 newRotation = Vec3(currentRotation.x, targetYaw * 180.0f / 3.14159f, currentRotation.z);

    m_player->GetTransform()->SetLocalRotation(newRotation);
}
