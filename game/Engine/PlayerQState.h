#pragma once
#include "PlayerStateMachine.h"
class PlayerQState :
    public PlayerState
{
    using Super = PlayerState;

public:
    PlayerQState(shared_ptr<ModelAnimator> modelAnimator, bool isChargingSkill);
    ~PlayerQState();

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(PlayerStateType newState);

    void UpdateChargingSkill();
    void UpdateCharging();
    void ReleaseSkill();

    void UpdateNormalSkill();

private:
    float m_skillTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isAnimationStarted = false;
    bool m_isSkillComplete = false;  // 추가: 스킬 완료 플래그

    bool m_isChargingSkill;
    bool m_isCharging;
    bool m_isReleasing;
    float m_chargeTime = 0;
    float m_durationTime = 0.f;

    shared_ptr<ModelAnimator> m_modelAnimator;

    friend class PlayerStateMachine;
};

