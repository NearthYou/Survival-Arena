#pragma once
#include "AnimationState.h"
class WolfAnimDyingState :
    public AnimationState
{
public:
    WolfAnimDyingState();
    virtual ~WolfAnimDyingState() = default;

    void Enter(shared_ptr<ModelAnimator> _animator) override;
    void Update(shared_ptr<ModelAnimator> _animator) override;
    void Exit(shared_ptr<ModelAnimator> _animator) override;
    bool CanTransitionTo(AnimationStateType _nextState) override;

private:
    float m_dyingTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isAnimationStarted = false;
    bool m_isDyingComplete = false;  // 추가: 스킬 완료 플래그
    float m_playSpeed = 2.f;
};

