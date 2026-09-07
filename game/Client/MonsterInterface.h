#pragma once

#include "IMonster.h"

#include "Monster.h"

class MonsterInterface :
    public IMonster
{
public:
    MonsterInterface(shared_ptr<Monster> monster) : m_monster(monster) {}


    Vec3 GetPosition() const override
    {
        return m_monster.lock()->GetTransform()->GetPosition();
    }

    Vec3 GetRotation() const override
    {
        return m_monster.lock()->GetTransform()->GetRotation();
    }

    float GetMoveSpeed() const override
    {
        return m_monster.lock()->GetMonsterStatus().moveSpeed;
    }
    int GetLevel() const override
    {
        return m_monster.lock()->GetMonsterStatus().level;
    }
    int GetHP() const override
    {
        return m_monster.lock()->GetMonsterStatus().hp;
    }
    int GetMaxHP() const override
    {
        return m_monster.lock()->GetMonsterStatus().maxHp;
    }
    bool IsAttacked() const override
    {
        return m_monster.lock()->IsAttacked();
    }


    // Monster 상태 변경
    void SetPosition(const Vec3& pos) override
    {
        m_monster.lock()->GetTransform()->SetPosition(pos);
    }

    void SetRotation(const Vec3& rot) override
    {
        m_monster.lock()->GetTransform()->SetRotation(rot);
    }

    void SetAttacked(bool _attacked) override
    {
        m_monster.lock()->SetAttacked(_attacked);
    }

    /*void TakeDamage(int damage) override
    {
        m_monster.lock()->Damaged(damage);
    }*/

private:
    weak_ptr<Monster> m_monster;
};

