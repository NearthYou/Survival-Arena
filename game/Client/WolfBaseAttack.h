#pragma once
#include "MonsterBehaviour.h"

class WolfBaseAttack 
    : public MonsterBehaviour
{
    using Super = MonsterBehaviour;
public:
    WolfBaseAttack();
    virtual ~WolfBaseAttack() = default;

    virtual void Start() override;
    virtual void Update() override;

    void StartAttack();
    void StopAttack();
    bool IsAttackComplete() const { return m_isAttackComplete; }


    void CalcDir(Vec3 otherPos, Vec3 wolfPos);

private:
    void UpdateAttackLogic();
    void CheckAttackDistance();
    void PlayAttackAnimation();

private:
    bool m_isAttacking = false;
    bool m_isAttackComplete = false;
    bool m_playAtk1 = true; // 공격 애니메이션 토글
    float m_attackTimer = 0.f;
    float m_attackDuration = (36.f / 25.f);

    // 설정값들
    float m_attackRange = 1.2f;
    float m_traceRange = 10.0f;
    float m_giveUpRange = 50.0f;
};

