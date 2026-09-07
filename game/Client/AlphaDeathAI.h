#pragma once
#include "AI.h"
class AlphaDeathAI :
    public AI
{
    using Super = AI;
public:
    AlphaDeathAI(shared_ptr<Monster> _Owner);
    virtual ~AlphaDeathAI();

public:
    virtual void Enter() override;
    virtual void Update() override;
    virtual void Exit() override;

private:
    float m_DeathAnimElapsedTime = 0.f;
    float m_DeathAnimDuration = 3.3f / 2.f;
};

