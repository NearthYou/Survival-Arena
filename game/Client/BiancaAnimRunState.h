#pragma once
#include "AnimationState.h"
class BiancaAnimRunState :
    public AnimationState
{
public:
    BiancaAnimRunState();
    ~BiancaAnimRunState() = default;

    void Enter(shared_ptr<ModelAnimator> animator) override;
    void Update(shared_ptr<ModelAnimator> animator) override;
    void Exit(shared_ptr<ModelAnimator> animator) override;
    bool CanTransitionTo(AnimationStateType nextState) override;

private:
    float m_playSpeed = 2.f;

};

