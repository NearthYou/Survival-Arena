#pragma once
#include "PlayerStateMachine.h"
class BiancaWaitState :
    public PlayerState
{
    using Super = PlayerState;

public:
    BiancaWaitState();
    ~BiancaWaitState();

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(PlayerStateType newState);
    bool IsMovable() const override { return true; }
};

