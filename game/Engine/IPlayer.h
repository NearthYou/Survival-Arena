#pragma once
#include "pch.h"

class IPlayer
{
public:
    virtual ~IPlayer() = default;

    // Player 정보 조회
    virtual Vec3 GetPosition() const = 0;
    virtual Vec3 GetRotation() const = 0;
    virtual float GetMoveSpeed() const = 0;
    virtual int GetLevel() const = 0;
    virtual int GetHP() const = 0;
    virtual int GetMaxHP() const = 0;

    // Player 상태 변경
    virtual void SetPosition(const Vec3& pos) = 0;
    virtual void SetRotation(const Vec3& rot) = 0;
    virtual void TakeDamage(int damage) = 0;
    virtual void AddExp(int exp) = 0;

    // 스킬 관련
    virtual bool CanUseSkill(int skillIndex) const = 0;
    virtual void UseSkill(int skillIndex) = 0;
    virtual float GetMaxSkillCooldown(int skillIndex) const = 0;
    virtual float GetCurSkillCooldown(int skillIndex) const = 0;
};

