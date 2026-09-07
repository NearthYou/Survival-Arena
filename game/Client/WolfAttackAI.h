#pragma once
#include "AI.h"
class Player;
class WolfAttackAI :
    public AI
{
    using Super = AI;
public:
	WolfAttackAI(shared_ptr<Monster> _Owner);
	virtual ~WolfAttackAI();

public:
    virtual void Enter() override;
    virtual void Update() override;
    virtual void Exit() override;

private:
    float m_RecogRange = 15.f;
    Vec3 m_enterPos;

    bool returnEnterPos = false;
    float attackElapsedTime = 0.f;

    shared_ptr<Player> m_target;
};

