#pragma once
#include "PlayerStateMachine.h"
class PlayerWaitState :
    public PlayerState
{
    using Super = PlayerState;

public:
    PlayerWaitState();
    ~PlayerWaitState();

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(PlayerStateType newState);
};

