#pragma once
#include "AnimationState.h"
class NickyRSkillState :
    public AnimationState
{
public:
    NickyRSkillState();
    ~NickyRSkillState() = default;

    void Enter(shared_ptr<ModelAnimator> animator) override;
    void Update(shared_ptr<ModelAnimator> animator) override;
    void Exit(shared_ptr<ModelAnimator> animator) override;
    bool CanTransitionTo(AnimationStateType nextState) override;

private:
    float m_skillTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isAnimationStarted = false;
    bool m_isSkillComplete = false;  // 추가: 스킬 완료 플래그
    shared_ptr<ModelAnimator> m_cachedAnimator;  // 추가: 애니메이터 캐싱


private:
    bool m_isRushAnimationActive = false;
public:
    bool IsRushAnimationActive() const { return m_isRushAnimationActive; }

private:
    vector<wstring> m_skillAnimations = {
        L"Skill_04_Ready",
        L"Skill_04_Start",
        L"Skill_01_Rush",
        L"Skill_04_Attack"
    };
    uint32 m_currentAnimIndex = 0;
    float m_currentAnimTime = 0.0f;
    float m_currentAnimDuration = 0.0f;

    vector<float> m_sequenceDurations;
    float m_playSpeed = 2.f;
};

