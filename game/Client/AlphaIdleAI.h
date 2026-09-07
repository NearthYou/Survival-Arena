#pragma once
#include "AI.h"
class AlphaIdleAI :
    public AI
{
    using Super = AI;
public:
    AlphaIdleAI(shared_ptr<Monster> _Owner);
    virtual ~AlphaIdleAI();

public:
    virtual void Enter() override;
    virtual void Update() override;
    virtual void Exit() override;
};

