#pragma once
class IMonster
{
public:
    virtual ~IMonster() = default;

    // Monster 정보 조회
    virtual Vec3 GetPosition() const = 0;
    virtual Vec3 GetRotation() const = 0;
    virtual float GetMoveSpeed() const = 0;
    virtual int GetLevel() const = 0;
    virtual int GetHP() const = 0;
    virtual int GetMaxHP() const = 0;
    virtual bool IsAttacked() const = 0;

    // Monster 상태 변경
    virtual void SetPosition(const Vec3& pos) = 0;
    virtual void SetRotation(const Vec3& rot) = 0;
    virtual void SetAttacked(bool _attacked) = 0;
    //virtual void TakeDamage(int damage) = 0;

    // 스킬 관련
  
};

