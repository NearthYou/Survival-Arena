#pragma once
#include "BaseSkill.h"
class NickyQSkill :
    public BaseSkill
{
    using Super = BaseSkill;

public:
    NickyQSkill(shared_ptr<Player> _player);
    ~NickyQSkill();

public:
    virtual void PlaySkill() override;
    virtual void Update() override;

    void CalculateSkillDirection();
    bool IsFirstAnimationPlaying();

    // 차징 시간 설정을 위한 함수 추가
    void SetChargeTime(float chargeTime) { m_chargeTime = chargeTime; }
    void ForceEndSkill();  // 강제 종료 함수 추가
    void EndSkillNaturally();
private:
    shared_ptr<Shader> m_shader = nullptr;

    shared_ptr<GameObject> m_chargingEffect = nullptr;

    Vec3 m_startPos, m_targetPos;

    float m_maxRange = 5.f;
    float m_baseRange = 3.f;        // 기본 이동 거리
    float m_maxChargeRange = 15.f;   // 최대 차징 이동 거리
    float m_chargeTime = 0.f;        // 차징된 시간 저장

    bool m_bskillStart = false;
    float m_duration = 0.f;
    float m_speed = 15.f;
    bool m_moveFlag = false;

    float m_moveDuration = 0.f;
    float m_moveElapsedTime = 0.f;

    int soundCount = 2;

    bool m_skillFlag = false;
    bool m_isForceEnded = false;  // 강제 종료 플래그 추가
};
