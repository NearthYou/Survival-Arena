#pragma once
#include "IPlayer.h"
#include "Player.h"

class PlayerInterface :
    public IPlayer
{
public:
    PlayerInterface(shared_ptr<Player> player) : m_player(player) {}

    // IPlayer 구현
    Vec3 GetPosition() const override
    {
        return m_player.lock()->GetTransform()->GetPosition();
    }

    Vec3 GetRotation() const override
    {
        return m_player.lock()->GetTransform()->GetRotation();
    }

    float GetMoveSpeed() const override 
    {
        return m_player.lock()->GetStatus().moveSpeed;
    }

    int GetLevel() const override 
    {
        return m_player.lock()->GetStatus().level;
    }

    int GetHP() const override 
    {
        return m_player.lock()->GetStatus().hp;
    }

    int GetMaxHP() const override 
    {
        return m_player.lock()->GetStatus().max_HP;
    }

    void SetPosition(const Vec3& pos) override
    {
        m_player.lock()->GetTransform()->SetPosition(pos);
    }

    void SetRotation(const Vec3& rot) override 
    {
        m_player.lock()->GetTransform()->SetRotation(rot);
    }

    void TakeDamage(int damage) override
    {
        //m_player.lock()->Damaged(damage);
    }

    void AddExp(int exp) override
    {
        m_player.lock()->SetCurExp(m_player.lock()->GetStatus().curExp + exp);
    }

    bool CanUseSkill(int skillIndex) const override
    {
        ISkill* skill = m_player.lock()->GetSkill(skillIndex);
        int skillLevel = skill->GetCurSkillLevel();
        if (skillLevel >= 1)
            return skill->CanExecuteSkill();
        
        // 스킬 사용 가능 여부 확인 로직
        return false; // 구현 필요
    }

    void UseSkill(int skillIndex) override
    {
        // 스킬 사용 로직
    }

    float GetMaxSkillCooldown(int skillIndex) const override;
    float GetCurSkillCooldown(int skillIndex) const override;
private:
    weak_ptr<Player> m_player;
};

