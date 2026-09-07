#pragma once
#include "PlayerStateMachine.h"
class BiancaEState :
    public PlayerState
{
    using Super = PlayerState;

public:
    BiancaEState(shared_ptr<ModelAnimator> modelAnimator);
    ~BiancaEState();

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(PlayerStateType newState);

    // 가상 함수 오버라이드
    virtual bool IsCharging() const override { return m_isCharging; }
    virtual bool IsReleasing() const override { return m_isReleasing; }
    virtual bool IsMovable() const override { return m_isCharging; } // 차징 중일 때만 이동 가능

    void UpdateChargingSkill();
    void UpdateCharging();
    void ReleaseSkill();

    bool IsSkillComplete() const { return m_isSkillComplete; } // 새로 추가

    void ForceEnd();
private:
    float m_skillTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isAnimationStarted = false;
    bool m_isSkillComplete = false;  // 추가: 스킬 완료 플래그

    bool m_isCharging = false;
    bool m_isReleasing = false;
    float m_chargeTime = 0;
    float m_durationTime = 0.f;
    bool m_isForcedEnd = false;  // 강제 종료 플래그 추가
    shared_ptr<ModelAnimator> m_modelAnimator;

    friend class PlayerStateMachine;
};

