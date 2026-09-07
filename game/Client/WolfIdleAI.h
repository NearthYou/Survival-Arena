#pragma once
#include "AI.h"
class WolfIdleAI :
    public AI
{
    using Super = AI;
public:
    WolfIdleAI(shared_ptr<Monster> _Owner);
    virtual ~WolfIdleAI();

public:
    virtual void Enter() override;
    virtual void Update() override;
    virtual void Exit() override;
};

