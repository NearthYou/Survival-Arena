#pragma once
#include "SkillObject.h"

class Player;

class NickyREffect :
    public SkillObject
{
    using Super = SkillObject;
public:
    NickyREffect(shared_ptr<Player> _player);
    ~NickyREffect();

public:
    virtual void Start() override;
    virtual void Update() override;
    virtual void OnCollisionEnter(shared_ptr<GameObject> _other) override;
    virtual void OnCollision(shared_ptr<GameObject> _other) override;

    void Reset();

private:

    // 공통 데미지 처리 함수
    void HandleDamage(shared_ptr<GameObject> _other);

    float m_timer = 0.f;
    float m_lifeTime = 1.5f;
    shared_ptr<Player> m_player;

    // 전역 플래그 대신 몬스터별 관리
    std::set<shared_ptr<GameObject>> m_damagedMonsters; // 이미 데미지를 준 몬스터들 기록

    friend class NickyRSkill;
};

