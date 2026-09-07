
#include "pch.h"
#include "BiancaBaseAttackState.h"
#include "Player.h"
#include "Monster.h"
#include "BiancaQProjectile.h"

BiancaBaseAttackState::BiancaBaseAttackState(shared_ptr<ModelAnimator> modelAnimator, shared_ptr<GameObject> player)
    : PlayerState(PlayerStateType::BaseAttack)
    , m_player(player)
    , m_modelAnimator(modelAnimator)
{
    m_Projectile = make_shared<BiancaQProjectile>(player);
    m_Projectile->SetName(L"Bianca_Q_Projectile");
    m_Projectile->SetActive(false);
    m_Projectile->SetBaseAttack(true);
    m_Projectile->AddComponent(make_shared<MeshRenderer>());
    m_Projectile->AddComponent(make_shared<SphereCollider>());
    {
        auto mesh = RESOURCES->Get<Mesh>(L"Sphere");
        m_Projectile->GetMeshRenderer()->SetMesh(mesh);
        m_Projectile->GetMeshRenderer()->SetPass(0);
        m_Projectile->GetMeshRenderer()->SetMaterial(RESOURCES->Get<Material>(L"default"));
    }
    CURSCENE->Add(m_Projectile);
    
}

void BiancaBaseAttackState::Enter()
{
    cout << "기본공격 상태 진입" << endl;

    m_attackTime = 100.0f;
    m_isMovingToTarget = true;
    m_hasDealtDamage = false;
    m_isAttackComplete = false;
    m_shouldContinueAttacking = true;
    m_pathUpdateTimer = 0.0f;

    if (!m_target)
    {
        cout << "공격 타겟이 없습니다!" << endl;
        m_isAttackComplete = true;
        m_shouldContinueAttacking = false;
        return;
    }
}

void BiancaBaseAttackState::Update()
{
    m_attackTime += DT;
    m_pathUpdateTimer += DT;

    // 외부 입력 체크 (이동, 스킬 등)
    if (INPUT->GetButtonDown(KEY_TYPE::RBUTTON) ||
        INPUT->GetButtonDown(KEY_TYPE::Q) ||
        INPUT->GetButtonDown(KEY_TYPE::W) ||
        INPUT->GetButtonDown(KEY_TYPE::E) ||
        INPUT->GetButtonDown(KEY_TYPE::R) ||
        INPUT->GetButtonDown(KEY_TYPE::Z))
    {
        cout << "외부 입력 감지 - 평타 중단" << endl;
        m_shouldContinueAttacking = false;
        m_isAttackComplete = true;
        return;
    }

    if (!m_target)
    {
        cout << "타겟이 사라졌습니다." << endl;
        m_isAttackComplete = true;
        m_shouldContinueAttacking = false;
        return;
    }

    // 몬스터가 죽었는지 확인
    auto monster = static_pointer_cast<Monster>(m_target);
    if (monster && monster->IsDead())
    {
        cout << "타겟이 죽었습니다." << endl;
        m_isAttackComplete = true;
        m_shouldContinueAttacking = false;
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

void BiancaBaseAttackState::Exit()
{
    cout << "기본공격 상태 종료" << endl;

    // NavMeshAgent 정지
    if (m_player && m_player->GetNavMeshAgent())
    {
        //m_player->GetNavMeshAgent()->Stop();
    }
}

bool BiancaBaseAttackState::CanTransitionTo(PlayerStateType newState)
{
    switch (newState)
    {
    case PlayerStateType::Wait:
        // 더 이상 공격하지 않거나 공격이 완료된 경우에만
        return !m_shouldContinueAttacking || m_isAttackComplete;

    case PlayerStateType::Run:
    case PlayerStateType::Skill_1:
    case PlayerStateType::Skill_2:
    case PlayerStateType::Skill_3:
    case PlayerStateType::Skill_4:
    case PlayerStateType::Craft:
        // 외부 입력이나 강제 전환의 경우 항상 허용
        return true;

    default:
        return false;
    }
}

void BiancaBaseAttackState::UpdateMovementToTarget()
{
    if (!IsInAttackRange())
    {
        // 주기적으로 경로 업데이트
        if (m_pathUpdateTimer >= PATH_UPDATE_INTERVAL)
        {
            m_pathUpdateTimer = 0.0f;
            Vec3 targetPos = m_target->GetTransform()->GetPosition();
            m_player->GetNavMeshAgent()->SetDestination(targetPos);

            if (!m_requestRunAnimation)
            {
                m_requestRunAnimation = true;
                m_player->GetAnimationStateMachine()->RequestStateChange(AnimationStateType::Run);
            }
        }
    }
    else
    {
        m_requestRunAnimation = false;
        // 공격 범위 내 도착
        m_player->GetNavMeshAgent()->Stop();
        m_isMovingToTarget = false;
        m_attackTime = 100.0f; // 공격 시간 리셋

        RotateToTarget();
        m_player->GetAnimationStateMachine()->RequestStateChange(AnimationStateType::BaseAttack);
        // 첫 공격 시작 - 애니메이션 상태 변경은 PlayerStateMachine에서 처리됨
        cout << "공격 범위 도달 - 평타 시작" << endl;
    }
}

void BiancaBaseAttackState::UpdateAttackLogic()
{
    // 공격 범위를 벗어났는지 체크
    if (!IsInAttackRange())
    {
        cout << "타겟이 공격 범위를 벗어남 - 다시 추격" << endl;
        m_isMovingToTarget = true;
        m_hasDealtDamage = false;
        m_attackTime = 100.0f;
        return;
    }

    if (m_Projectile->GetActive() && m_Projectile->GetArrive()) {
        m_Projectile->SetArrive(false);
        DealDamage();
    }

    // 공격 쿨타임 체크
    if (m_attackTime >= m_attackCooldown)
    {
        if (!m_hasDealtDamage)
        {
            DealDamage();
            m_hasDealtDamage = true;

            // 다음 공격 준비
            //투사체 던지기.
            Vec3 startPos = m_player->GetTransform()->GetPosition();
            Vec3 endPos = m_target->GetTransform()->GetPosition();
            float distance = Vec3::Distance(startPos, endPos);
            float flightTime = distance / m_speed;
            m_Projectile->SetMoveTarget(startPos, endPos, flightTime);
            SOUND->PlaySound(L"Bianca/Bianca_Arcana_Attack.wav", 1, 0.5f);

            CheckForContinuousAttack();
        }

        // 다음 공격 사이클 준비 (약간의 여유시간 후)
        if (m_attackTime >= m_attackCooldown * 1.0f)
        {
            if (m_shouldContinueAttacking && m_target && !static_pointer_cast<Monster>(m_target)->IsDead())
            {
                cout << "연속 평타 실행" << endl;
                // 다음 공격 사이클로 리셋
                m_attackTime = 0.0f;
                m_hasDealtDamage = false;
                // 애니메이션 상태는 NickyAnimBaseAttackState에서 계속 번갈아가며 처리

                m_player->GetAnimationStateMachine()->RequestStateChange(AnimationStateType::BaseAttack);
            }
            else
            {
                // 연속 공격 조건이 맞지 않으면 종료
                m_isAttackComplete = true;
            }
        }
    }
}

void BiancaBaseAttackState::CheckForContinuousAttack()
{
    // 여기서 연속 공격 조건을 체크
    // 1. 타겟이 여전히 유효한가?
    // 2. 타겟이 공격 범위 내에 있는가?
    // 3. 외부 입력이 없는가?

    if (!m_target || static_pointer_cast<Monster>(m_target)->IsDead())
    {
        m_shouldContinueAttacking = false;
        return;
    }

    if (!IsInAttackRange())
    {
        // 범위를 벗어났지만 추격은 계속
        m_shouldContinueAttacking = true;
        m_isMovingToTarget = true;
        return;
    }

    // 모든 조건이 만족하면 연속 공격 계속
    m_shouldContinueAttacking = true;
}

bool BiancaBaseAttackState::IsInAttackRange() const
{
    if (!m_target) return false;

    Vec3 playerPos = m_player->GetTransform()->GetPosition();
    Vec3 targetPos = m_target->GetTransform()->GetPosition();

    float distance = Vec3::Distance(playerPos, targetPos);
    return distance <= ATTACK_RANGE;
}

void BiancaBaseAttackState::RotateToTarget()
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

void BiancaBaseAttackState::DealDamage()
{
    if (!m_target) return;

    auto monster = static_pointer_cast<Monster>(m_target);
    if (monster)
    {
        auto player = static_pointer_cast<Player>(m_player);
        if (player)
        {
            monster->Damaged(m_player, player->GetStatus().hitAttack);
            SOUND->PlaySound(L"Bianca/Bianca_Arcana_Hit.wav", 1, 0.5f);

            //cout << "기본공격 데미지: " << player->GetStatus().hitAttack << endl;
        }
    }
}
