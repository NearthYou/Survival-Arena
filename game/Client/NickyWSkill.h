#pragma once
#include "BaseSkill.h"
class NickyWSkill :
    public BaseSkill
{
    using Super = BaseSkill;

public:
    NickyWSkill(shared_ptr<Player> _player);
    virtual ~NickyWSkill();

public:
    virtual void PlaySkill() override;
    virtual void Update() override;

private:

    float m_skillTimer = 0.f;
    float m_skillDuration = (40.f / 25.f) / 2.f;

    shared_ptr<Shader> m_shader = nullptr;
    shared_ptr<Player> m_player = nullptr;
};

