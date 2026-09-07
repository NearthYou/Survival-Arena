#pragma once
#include "PlayerStateMachine.h"

class BiancaQProjectile;
class BiancaBaseAttackState :
    public PlayerState
{
public:
    BiancaBaseAttackState(shared_ptr<ModelAnimator> modelAnimator, shared_ptr<GameObject> player);
    ~BiancaBaseAttackState() = default;

    virtual void Enter() override;
    virtual void Update() override;
    virtual void Exit() override;
    virtual bool CanTransitionTo(PlayerStateType newState) override;
    bool IsMovable() const override { return true; }
private:
    shared_ptr<GameObject> m_player;
    shared_ptr<ModelAnimator> m_modelAnimator;

    float m_attackTime = 0.0f;
    float m_attackCooldown = (38.f / 25.f) / 2.f;  // 기본 공격 쿨타임
    bool m_isMovingToTarget = true;
    bool m_hasDealtDamage = false;
    bool m_isAttackComplete = false;
    bool m_requestRunAnimation = false; //추적하는 동안 Run 애니메이션 재생

    // 연속 공격을 위한 추가 변수들
    bool m_shouldContinueAttacking = true;  // 계속 공격할지 여부
    float m_pathUpdateTimer = 0.0f;
    static constexpr float PATH_UPDATE_INTERVAL = 0.1f;
    static constexpr float ATTACK_RANGE = 10.f;

    void UpdateMovementToTarget();
    void UpdateAttackLogic();
    bool IsInAttackRange() const;
    void RotateToTarget();
    void DealDamage();
    void CheckForContinuousAttack();


private:
    shared_ptr<BiancaQProjectile> m_Projectile;
    float m_speed = 35.f;

};

