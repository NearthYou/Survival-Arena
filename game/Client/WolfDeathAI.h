#pragma once
#include "AI.h"
class WolfDeathAI :
    public AI
{
    using Super = AI;
public:
	WolfDeathAI(shared_ptr<Monster> _Owner);
	virtual ~WolfDeathAI();

public:
    virtual void Enter() override;
    virtual void Update() override;
    virtual void Exit() override;


private:
    float m_DeathAnimElapsedTime = 0.f;
    float m_DeathAnimDuration = 1.3f / 2.f;
};

