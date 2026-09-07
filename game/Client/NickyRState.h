#pragma once
#include "PlayerStateMachine.h"
class NickyRState :
    public PlayerState
{
    using Super = PlayerState;

public:
    NickyRState(shared_ptr<ModelAnimator> modelAnimator);
    ~NickyRState();

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(PlayerStateType newState);
    bool IsMovable() const override { return false; }
    bool IsSkillComplete() const { return m_isSkillComplete; } // 새로 추가

private:
    float m_skillTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isSkillComplete = false;  // 추가: 스킬 완료 플래그
    float m_expectedDuration = 0.f;

    vector<float> m_sequenceDurations;
    shared_ptr<ModelAnimator> m_modelAnimator;

    friend class PlayerStateMachine;
};

