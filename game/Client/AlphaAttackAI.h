#pragma once
#include "AI.h"
class Player;
class AlphaAttackAI :
    public AI
{
    using Super = AI;
public:
    AlphaAttackAI(shared_ptr<Monster> _Owner);
    virtual ~AlphaAttackAI();

public:
    virtual void Enter() override;
    virtual void Update() override;
    virtual void Exit() override;

private:
    float m_RecogRange = 15.f;
    float m_SkillRange = 7.5f;
    Vec3 m_enterPos;

    bool returnEnterPos = false;
    float attackElapsedTime = 0.f;

    float skillCoolTime = 15.f;

    float skillElapsedTime = 0.f;
    float skillDuration = 4.5f;

    shared_ptr<Player> m_target;
};

