#pragma once
#include "BaseSkill.h"
class BiancaQProjectile;
class BiancaQCone;
class BiancaQSkill :
    public BaseSkill
{
    using Super = BaseSkill;
public:
    BiancaQSkill(shared_ptr<Player> _player);
    virtual ~BiancaQSkill();

public:
    virtual void PlaySkill() override;
    virtual void Update() override;
   
private:
    void UpdateAnimation();


    float m_skillRange = 10.f;
    shared_ptr<BiancaQProjectile> m_Projectile;
    shared_ptr<BiancaQCone> m_Cone;
};

