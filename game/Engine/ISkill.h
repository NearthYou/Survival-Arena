#pragma once
class ISkill
{
public:
    virtual ~ISkill() = default;

    virtual void Update() = 0;

    // 스킬 실행 관련
    virtual bool CanExecuteSkill() const = 0;
    virtual void ExecuteSkill() = 0;

    // 쿨다운 관련
    virtual float GetCurrentCooldown() const = 0;
    virtual float GetMaxCooldown() const = 0;
    virtual bool IsOnCooldown() const = 0;
    virtual void StartCooldown() = 0;

    virtual void PlaySkill() = 0;

    // 업데이트
    virtual void UpdateCooldown(float deltaTime) = 0;

    // 스킬 정보
    virtual int GetSkillIndex() const = 0;
    virtual const wstring& GetSkillName() const = 0;

    //스킬 레벨 정보
    virtual void SetMaxSkillLevel(int _maxLevel) = 0;
    virtual void SkillLevelUp() = 0;
    virtual int GetMaxSkillLevel() = 0;
    virtual int GetCurSkillLevel() = 0;
    virtual int GetStaminaCost() const { return 0; }
    virtual float GetDamageMultiplier() const { return 1.f; }

protected:
    int m_curSkillLevel = 0;
    int m_maxSkillLevel = 5;
};

