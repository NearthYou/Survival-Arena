#pragma once
#include "AnimationState.h"
class NickyAnimBaseAttackState :
    public AnimationState
{
public:
    NickyAnimBaseAttackState();
    ~NickyAnimBaseAttackState() = default;

    void Enter(shared_ptr<ModelAnimator> animator) override;
    void Update(shared_ptr<ModelAnimator> animator) override;
    void Exit(shared_ptr<ModelAnimator> animator) override;
    bool CanTransitionTo(AnimationStateType nextState) override;

private:
    // 모든 인스턴스가 공유하는 static 변수로 변경!
    bool m_motionChange;

    float m_skillTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isSkillComplete = false;  // 추가: 스킬 완료 플래그
    shared_ptr<ModelAnimator> m_cachedAnimator;  // 추가: 애니메이터 캐싱

    float m_playSpeed = 2.f;
};

