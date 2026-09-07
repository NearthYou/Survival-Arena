#pragma once
#include "AnimationState.h"

class NickyRunState : public AnimationState
{
public:
    NickyRunState();
    ~NickyRunState() = default;

    void Enter(shared_ptr<ModelAnimator> animator) override;
    void Update(shared_ptr<ModelAnimator> animator) override;
    void Exit(shared_ptr<ModelAnimator> animator) override;
    bool CanTransitionTo(AnimationStateType nextState) override;

private:
    float m_moveTime = 0.0f;
    bool m_isAnimationStarted = false;
};
