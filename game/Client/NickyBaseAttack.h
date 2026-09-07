#pragma once
#include "PlayerMonoBehaviour.h"
class NickyBaseAttack :
    public PlayerMonoBehaviour
{
    using Super = PlayerMonoBehaviour;

public:
    NickyBaseAttack();
    ~NickyBaseAttack() = default;

    virtual void Start() override;
    virtual void Update() override;

public:
    void StartBaseAttack();
    void StopBaseAttack();

    void CalcDir(Vec3 otherPos, Vec3 wolfPos);
    bool IsInAttackRange();

private:
    bool m_motionChange = true;
    float m_attackRange = 4.f;
    float m_pathUpdateInterval = 0.1f;
    float m_updateTimer = 0.0f;

    float m_attackDuration = (38.f / 25.f) / 2.f;
    bool m_isArriveToTarget = false;

};

