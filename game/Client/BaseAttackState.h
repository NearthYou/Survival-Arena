#pragma once

#include "PlayerStateMachine.h"

class BaseAttackState : public PlayerState
{
protected:
    BaseAttackState(PlayerStateType type, shared_ptr<ModelAnimator> modelAnimator, shared_ptr<GameObject> player);

public:
    virtual void Enter() override;
    virtual void Update() override;
    virtual void Exit() override;
    virtual bool CanTransitionTo(PlayerStateType newState) override;

    // 각 캐릭터별로 오버라이드할 함수들
    virtual float GetAttackRange() const = 0;
    virtual float GetAttackCooldown() const = 0;
    virtual void PlayAttackAnimation() = 0;
    virtual void DealDamage() = 0;

protected:
    void UpdateMovementToTarget();
    void UpdateAttackLogic();
    bool IsInAttackRange() const;
    void RotateToTarget();

protected:
    shared_ptr<GameObject> m_player;
    shared_ptr<ModelAnimator> m_modelAnimator;

    float m_attackTime = 0.0f;
    bool m_isMovingToTarget = true;
    bool m_hasDealtDamage = false;
    bool m_isAttackComplete = false;
    bool m_alternateAttack = false; // 공격 모션 번갈아가기용

    static float s_pathUpdateInterval;
    float m_pathUpdateTimer = 0.0f;
};