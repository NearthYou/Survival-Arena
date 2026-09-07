#pragma once
#include "AnimationState.h"
class WolfAnimAttackState :
    public AnimationState
{
public:
    WolfAnimAttackState();
    virtual ~WolfAnimAttackState() = default;

    void Enter(shared_ptr<ModelAnimator> _animator) override;
    void Update(shared_ptr<ModelAnimator> _animator) override;
    void Exit(shared_ptr<ModelAnimator> _animator) override;
    bool CanTransitionTo(AnimationStateType _nextState) override;

private:
    bool m_motionChange = false;
    vector<float> m_sequenceDurations;
    float m_animTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isAnimationStarted = false;
    bool m_isAttackComplete = false;  // 추가: 스킬 완료 플래그
    float m_playSpeed = 1.f;
};

