#pragma once
#include "PlayerStateMachine.h"
class NickyWaitState :
    public PlayerState
{
    using Super = PlayerState;

public:
    NickyWaitState();
    ~NickyWaitState();

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(PlayerStateType newState);
    bool IsMovable() const override { return true; }
};

