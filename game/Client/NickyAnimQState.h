#pragma once

#include "AnimationState.h"

enum class QSkillChargeState
{
    Default = -1,
    ChargingFromWait = 0,       // Wait 상태에서 차징 시작
    ChargingFromRun,        // Run 상태에서 차징 시작
    ChargingWaitLoop,       // Wait 상태에서 차징 루프
    ChargingRunLoop,        // Run 상태에서 차징 루프
    Releasing,              // 스킬 발동 중
    Complete                // 스킬 완료
};

class NickyAnimQState :
    public AnimationState
{
public:
    NickyAnimQState();
    ~NickyAnimQState() = default;

    void Enter(shared_ptr<ModelAnimator> animator) override;
    void Update(shared_ptr<ModelAnimator> animator) override;
    void Exit(shared_ptr<ModelAnimator> animator) override;
    bool CanTransitionTo(AnimationStateType nextState) override;

    // 외부에서 초기 상태 설정
    void SetInitialMovementState(bool wasMoving);

    // 상태 조회
    bool IsCharging() const;
    bool IsComplete() const;
    float GetChargeTime() const { return m_chargeTime; }
    void ForceEndAnimation();  // 강제 애니메이션 종료 함수 추가

private:
    // 내부 로직 처리
    void UpdateCharging();
    void UpdateReleasing();
    void HandleMovementInput();
    void HandleSkillInput();
    void TransitionToChargeState(QSkillChargeState newState);
    void PlayAppropriateAnimation();
    bool IsStartAnimationComplete();
    void ReleaseSkill();

    
private:
    shared_ptr<GameObject> GetGameObject() const;

private:
    QSkillChargeState m_chargeState = QSkillChargeState::Default;
    float m_chargeTime = 0.0f;
    float m_skillTime = 0.0f;
    float m_maxChargeTime = 5.0f;

    bool m_isMoving = false;
    bool m_wasMoving = false;
    bool m_isReleasing = false;
    bool m_isComplete = false;
    bool m_isChargingActive = true;  // 차징 활성 상태

    // 애니메이션 상태
    bool m_isStartAnimationPlaying = false;
    float m_startAnimationTime = 0.0f;
    float m_playSpeed = 2.f;
    bool m_isForcedEnd = false;  // 강제 종료 플래그
    shared_ptr<ModelAnimator> m_cachedAnimator;
  
private:
    bool m_isFirstAnimationActive = false;
public:
    bool IsFirstAnimationActive() const { return m_isFirstAnimationActive; }

};

