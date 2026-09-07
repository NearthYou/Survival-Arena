#pragma once
#include "BaseSkill.h"

class BiancaESkillCircle;
class BiancaRSkill :
    public BaseSkill
{
    using Super = BaseSkill;
public:
    BiancaRSkill(shared_ptr<Player> _player);
    virtual ~BiancaRSkill();

public:
    virtual void PlaySkill() override;
    virtual void Update() override;


private:
    shared_ptr<BiancaESkillCircle> m_outerCircle;
    shared_ptr<SphereCollider> m_outerCircleCollider;

    shared_ptr<BiancaESkillCircle> m_innerCircle;
    shared_ptr<SphereCollider> m_innerCircleCollider;

    shared_ptr<GameObject> m_drainCircle;
    shared_ptr<GameObject> m_drainBlood;

private:
    float m_drainEffectDuration = 2.f;
    float m_drainEffectElapsedTime = 0.f;

    float m_innerCircleDuration = 2.5f;
    float m_innerCircleElapsedTime = 0.f;

    bool skillFlag = false;
    bool phaseTwo = false;


    bool m_circleFollowBianca = true;
};

