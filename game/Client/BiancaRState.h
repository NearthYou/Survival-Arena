#pragma once
#include "PlayerStateMachine.h"
class BiancaRState :
    public PlayerState
{
    using Super = PlayerState;

public:
    BiancaRState(shared_ptr<ModelAnimator> modelAnimator);
    ~BiancaRState();

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(PlayerStateType newState);
    virtual bool IsMovable() const override { return true; }

    bool IsSkillComplete() const { return m_isSkillComplete; } // 새로 추가

private:
    float m_skillTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isAnimationStarted = false;
    bool m_isSkillComplete = false;  // 추가: 스킬 완료 플래그

    shared_ptr<ModelAnimator> m_modelAnimator;

    friend class PlayerStateMachine;
};

