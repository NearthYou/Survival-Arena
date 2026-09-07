#pragma once
#include "BaseSkill.h"

class Player;
class NickyERange;

class NickyESkill :
    public BaseSkill
{
    using Super = BaseSkill;

public:
    NickyESkill(shared_ptr<Player> _player);
    virtual ~NickyESkill();

public:
    virtual void PlaySkill() override;
    virtual void Update() override;

    void PlayAttackSound();
    void UpdateColliderPosition();
    void CalculateSkillDirection();

private:
    shared_ptr<Shader> m_shader = nullptr;
    shared_ptr<NickyERange> m_skillRange = nullptr;

    bool m_bskillStart = false;
    float m_duration = 0.f;

    int soundCount = 2;
};

