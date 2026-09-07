#pragma once
#include "PlayerStateMachine.h"
class PlayerRunState :
    public PlayerState
{
    using Super = PlayerState;

public:
    PlayerRunState();
    ~PlayerRunState();

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(PlayerStateType newState);
};

