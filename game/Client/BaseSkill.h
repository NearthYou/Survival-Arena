#pragma once

#include "ISkill.h"

class Player;
class BaseSkill : public ISkill
{
public:
	BaseSkill(shared_ptr<Player> _player, int skillIndex);
	virtual ~BaseSkill();

    // ISkillExecutor 구현
    virtual bool CanExecuteSkill() const override;

    virtual void ExecuteSkill() override;

    virtual float GetCurrentCooldown() const override {
        return m_skillcurCooldown;
    }

    virtual float GetMaxCooldown() const override;
    int GetStaminaCost() const override;
    float GetDamageMultiplier() const override;
    void ConfigureProgression(int staminaCost);

    virtual bool IsOnCooldown() const override {
        return m_skillcurCooldown > 0.0f;
    }

    virtual void StartCooldown() override {
        SkillEnd();
    }

    virtual void UpdateCooldown(float deltaTime) override {
        if (m_skillcurCooldown > 0.0f) {
            m_skillcurCooldown -= deltaTime;
            if (m_skillcurCooldown < 0.0f) {
                m_skillcurCooldown = 0.0f;
            }
        }
    }

    virtual int GetSkillIndex() const override {
        return m_skillIndex;
    }

    virtual const wstring& GetSkillName() const override {
        return m_skillName;
    }
  
    virtual void SetMaxSkillLevel(int _maxLevel) { m_maxSkillLevel = _maxLevel; }
    virtual void SkillLevelUp();
    virtual int GetMaxSkillLevel() { return m_maxSkillLevel; }
    virtual int GetCurSkillLevel() { return m_curSkillLevel; }

public:
	virtual void Update() override;
	virtual void PlaySkill() override;
	void SkillEnd();
    void UpdateSkillCoolDown();

public:
	void SetSkillName(wstring& _name) { m_skillName = _name; }
	void SetSkillName(wstring&& _name) { m_skillName = move(_name); }
	wstring& GetSkillName() { return m_skillName; }

	void SetSkillDesc(wstring& _desc) { m_skillDesc = _desc; }
	void SetSkillDesc(wstring&& _desc) { m_skillDesc = move(_desc); }
	wstring& GetSkillDesc() { return m_skillDesc; }

	void SetSkillAnimsName(const vector<wstring>& _animsName) {m_skillAnimsName = _animsName;}
	vector<wstring> GetSkillAnimsName() { return m_skillAnimsName; }

	void SetSkillImage(shared_ptr<Texture> _image) { m_skillImage = _image; }
	shared_ptr<Texture> GetSkillImage() { return m_skillImage; }

	float GetCurCooldown() { return m_skillcurCooldown; }
	float GetCooldown() { return GetMaxCooldown(); }
	XMVECTOR ScreenToWorld(POINT _screenPos);

protected:
	bool m_isPassive = false;
	wstring m_skillName;
	wstring m_skillDesc;
	
	//스킬 사용 시 이 값으로 초기화. 
	float m_skillCooldown = 0.f;
	//현재 스킬 쿨다운, 
	float m_skillcurCooldown = 0.f;
    int m_skillIndex;
    int m_baseStaminaCost = 0;
    bool m_progressionEnabled = false;
    bool m_castInProgress = false;
    bool m_cooldownStarted = false;

	vector<wstring> m_skillAnimsName;
	shared_ptr<Texture> m_skillImage;

	shared_ptr<Player> m_playerObject;
};

