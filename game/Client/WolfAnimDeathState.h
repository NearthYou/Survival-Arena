#pragma once
#include "AnimationState.h"
class WolfAnimDeathState :
    public AnimationState
{
public:
    WolfAnimDeathState();
    virtual ~WolfAnimDeathState() = default;

    void Enter(shared_ptr<ModelAnimator> _animator) override;
    void Update(shared_ptr<ModelAnimator> _animator) override;
    void Exit(shared_ptr<ModelAnimator> _animator) override;
    bool CanTransitionTo(AnimationStateType _nextState) override;

private:
    float m_deathTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isDeathComplete = false;  // 추가: 스킬 완료 플래그
    float m_playSpeed = 2.f;
};

