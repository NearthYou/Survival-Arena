#pragma once
#include "PlayerStateMachine.h"
class NickyRunState :
    public PlayerState
{
    using Super = PlayerState;

public:
    NickyRunState();
    ~NickyRunState();

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(PlayerStateType newState);
    bool IsMovable() const override { return true; }
private:
    float m_stepSoundTime = 0.f;
    bool leftStep = false;
};

