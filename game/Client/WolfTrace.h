#pragma once
#include "MonsterBehaviour.h"
class WolfTrace :
    public MonsterBehaviour
{
    using Super = MonsterBehaviour;
public:
    WolfTrace();
    virtual ~WolfTrace() = default;
    virtual void Start() override;
    virtual void Update() override;

public:
    void StartTrace();
    void StopTrace();

    void CalcDir(Vec3 otherPos, Vec3 wolfPos);
    bool IsInAttackRange();

private:
    float m_traceSpeed = 2.0f;
    float m_attackRange = 3.f;
    float m_pathUpdateInterval = 0.5f;
    float m_updateTimer = 0.0f;
};

