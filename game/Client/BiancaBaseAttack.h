#pragma once
#include "PlayerMonoBehaviour.h"
class BiancaBaseAttack :
    public PlayerMonoBehaviour
{
    using Super = PlayerMonoBehaviour;

public:
    BiancaBaseAttack();
    ~BiancaBaseAttack() = default;

    virtual void Start() override;
    virtual void Update() override;

public:
    void StartBaseAttack();
    void StopBaseAttack();

    void CalcDir(Vec3 otherPos, Vec3 wolfPos);
    bool IsInAttackRange();

private:
    float m_attackRange = 10.f;
    float m_pathUpdateInterval = 0.1f;
    float m_updateTimer = 0.0f;

    float m_attackDuration = (38.f / 25.f) / 2.f;
    bool m_isArriveToTarget = false;
};

