#pragma once
#include "BaseSkill.h"

class Player;

class NickyRSkill :
    public BaseSkill
{
    using Super = BaseSkill;

public:
    NickyRSkill(shared_ptr<Player> _player);
    ~NickyRSkill();

public:
    virtual void PlaySkill() override;
    virtual void Update() override;

    void SetTarget(shared_ptr<GameObject> _target) { m_target = _target; }

    void CalculateSkillDirection();
    bool IsRushAnimationPlaying();
    void SetRushDuration(float duration);
    void UpdateTargetPosition();
    void UpdateEffectPosition();

private:
    shared_ptr<Shader> m_shader = nullptr;
    shared_ptr<GameObject> m_target = nullptr;

    shared_ptr<GameObject> m_effect1 = nullptr;
    shared_ptr<GameObject> m_effect2 = nullptr;


    Vec3 m_startPos, m_targetPos;

    float m_maxRange = 5.f;
    bool m_bskillStart = false;
    float m_duration = 0.f;
    float m_speed = 15.f;
    bool m_moveFlag = false;

    float m_moveDuration = 0.f;
    float m_moveElapsedTime = 0.f;

    float m_rushSoundDuration = 0.f;

    // 기존 변수들...
    bool m_isDynamicTracking = true;  // 동적 추적 활성화 여부
    float m_trackingUpdateInterval = 0.2f;  // 추적 업데이트 간격 (50FPS)
    float m_trackingTimer = 0.f;  // 추적 타이머
    Vec3 m_lastTargetPos;  // 이전 타겟 위치 (변화 감지용)

    int soundCount = 2;


    float m_effectTime = 0.f;
    float m_effectDuration = 0.f;
};

