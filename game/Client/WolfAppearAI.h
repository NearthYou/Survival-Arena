#pragma once
#include "AI.h"
class WolfAppearAI :
    public AI
{
    using Super = AI;
public:
    WolfAppearAI(shared_ptr<Monster> _Owner);
    virtual ~WolfAppearAI();

public:
    virtual void Enter() override;
    virtual void Update() override;
    virtual void Exit() override;

private:
    float m_AppearAnimElapsedTime = 0.f;
    float m_AppearAnimDuration = 1.5f;

};

