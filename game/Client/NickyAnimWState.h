#pragma once
#include "AnimationState.h"

class NickyAnimWState :
    public AnimationState
{
public:
    NickyAnimWState();
    ~NickyAnimWState() = default;

    void Enter(shared_ptr<ModelAnimator> animator) override;
    void Update(shared_ptr<ModelAnimator> animator) override;
    void Exit(shared_ptr<ModelAnimator> animator) override;
    bool CanTransitionTo(AnimationStateType nextState) override;

private:
    float m_skillTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isAnimationStarted = false;
    float m_expectedDuration = 0.f;
    bool m_isSkillComplete = false;  // 추가: 스킬 완료 플래그
    shared_ptr<ModelAnimator> m_cachedAnimator;  // 추가: 애니메이터 캐싱
    vector<float> m_sequenceDurations;
    float m_playSpeed = 2.f;
};

