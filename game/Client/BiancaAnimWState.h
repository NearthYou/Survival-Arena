#pragma once
#include "AnimationState.h"
class BiancaAnimWState :
    public AnimationState
{
public:
    BiancaAnimWState();
    ~BiancaAnimWState() = default;

    void Enter(shared_ptr<ModelAnimator> animator) override;
    void Update(shared_ptr<ModelAnimator> animator) override;
    void Exit(shared_ptr<ModelAnimator> animator) override;
    bool CanTransitionTo(AnimationStateType nextState) override;

private:
    float m_skillTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isAnimationStarted = false;
    bool m_isSkillComplete = false;  // 추가: 스킬 완료 플래그
    float m_expectedDuration = 0.f;
    shared_ptr<ModelAnimator> m_cachedAnimator;  // 추가: 애니메이터 캐싱
};

