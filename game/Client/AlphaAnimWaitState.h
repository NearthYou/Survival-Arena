#pragma once
#include "AnimationState.h"
class AlphaAnimWaitState :
    public AnimationState
{
public:
    AlphaAnimWaitState();
    virtual ~AlphaAnimWaitState();

public:
    void Enter(shared_ptr<ModelAnimator> _animator) override;
    void Update(shared_ptr<ModelAnimator> _animator) override;
    void Exit(shared_ptr<ModelAnimator> _animator) override;
    bool CanTransitionTo(AnimationStateType _nextState) override;
private:
    float m_animTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isAnimationStarted = false;
    bool m_isAppearComplete = false;  // 추가: 스킬 완료 플래그
};

