#pragma once
#include "BaseSkill.h"

class BiancaESkillCircle;
class BiancaESkill :
    public BaseSkill
{
    using Super = BaseSkill;
public:
    BiancaESkill(shared_ptr<Player> _player);
    virtual ~BiancaESkill();

public:
    virtual void PlaySkill() override;
    virtual void Update() override;

    void ForceEndSkill();

private:
    bool m_pushSkill = false;
    bool m_moveFlag = false;
    bool m_endFlag = false;

    float m_circleSizeDuration = 1.f;
    float m_circleSizeElapedTime = 0.f;
    float m_circleKeepElapedTime = 0.f;

    float m_eSkillEndElapsedTime = 0.f;

    float m_maxRange = 15.0f;
    float m_speed = 30.f;

    float m_moveDuration = 0.f;
    float m_moveElapsedTime = 0.f;
    Vec3 m_startPos, m_targetPos;

    bool m_isForceEnded = false;

private:
    shared_ptr<SphereCollider> m_collider;
    shared_ptr<BiancaESkillCircle> m_circle;
    shared_ptr<Shader> m_shader;
};

