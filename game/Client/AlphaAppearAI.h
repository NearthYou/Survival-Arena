#pragma once
#include "AI.h"
class AlphaAppearAI :
    public AI
{
    using Super = AI;
public:
    AlphaAppearAI(shared_ptr<Monster> _Owner);
    virtual ~AlphaAppearAI();

public:
    virtual void Enter() override;
    virtual void Update() override;
    virtual void Exit() override;

private:
    float m_AppearAnimElapsedTime = 0.f;
    float m_AppearAnimDuration = 6.0f ;
};

