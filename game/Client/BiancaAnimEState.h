#pragma once
#include "AnimationState.h"

enum class BiancaESkillChargeState
{
    ChargingWait,    // 차징 중 (정지 상태 - Wait)
    ChargingRun,     // 차징 중 (이동 상태 - Run)
    Releasing,       // 스킬 발동 중 (Skill_3_2)
    Ending,          // 스킬 마무리 (Skill_3_3)
    Complete         // 스킬 완료
};

class BiancaAnimEState :
    public AnimationState
{
public:
    BiancaAnimEState();
    ~BiancaAnimEState() = default;

    void Enter(shared_ptr<ModelAnimator> animator) override;
    void Update(shared_ptr<ModelAnimator> animator) override;
    void Exit(shared_ptr<ModelAnimator> animator) override;
    bool CanTransitionTo(AnimationStateType nextState) override;

    // 상태 조회
    bool IsCharging() const;
    bool IsComplete() const;
    float GetChargeTime() const { return m_chargeTime; }
    void ForceEndAnimation();

private:
    void UpdateCharging();
    void UpdateReleasing();
    void UpdateEnding();
    void HandleSkillInput();
    void HandleMovementInput();
    void ReleaseSkill();
    void TransitionToSkillState(BiancaESkillChargeState newState);

private:
    shared_ptr<GameObject> GetGameObject() const;

private:
    BiancaESkillChargeState m_skillState = BiancaESkillChargeState::ChargingWait;
    float m_chargeTime = 0.0f;
    float m_skillTime = 0.0f;
    float m_maxChargeTime = 5.0f;  // 최대 차징 시간

    bool m_isMoving = false;
    bool m_isCharging = true;
    bool m_isReleasing = false;
    bool m_isEnding = false;
    bool m_isComplete = false;
    bool m_isForcedEnd = false;  // 강제 종료 플래그

    shared_ptr<ModelAnimator> m_cachedAnimator;
    float m_playSpeed = 2.f;
};

