#pragma once
#include "GameObject.h"
#include "MonsterState.h"

class Player;
class Item;
class HealthBar;
class ItemBox;
class AI;
class MonsterInterface;

struct MonsterStatus {
    int level = 1;
    int32 maxHp = 200;
    int32 hp = 200;

    float adPower = 50;
    float hitRange = 2.5;
    float hitSpeed = 0.3f;

    float moveSpeed = 1.56;
};

class Monster :
    public GameObject
{
    using Super = GameObject;
public:
    Monster(shared_ptr<Shader> _shader);
    virtual ~Monster();

public:
    virtual void Start() override;
    virtual void Update() override;
    virtual void LateUpdate() override;
    virtual void FixedUpdate() override;

    //Collision 관련
    virtual void OnCollision(shared_ptr<GameObject> _other) = 0;
    virtual void OnCollisionEnter(shared_ptr<GameObject> _other) = 0;
    virtual void OnCollisionExit(shared_ptr<GameObject> _other) = 0;

public:
    MonsterStatus& GetMonsterStatus() { return m_monsterStatus; }
    shared_ptr<Player> GetTarget() { return m_targetPlayer; }
    bool IsAttacked() { return m_isAttacked; }
    void SetAttacked(bool _isAttacked) { m_isAttacked = _isAttacked; }

    bool IsStun() { return m_isStun > 0.f ? true : false; }
    void SetLevel(int _value) { m_monsterStatus.level = _value; }
    void SetMaxHP(int32 _value) { m_monsterStatus.maxHp = _value; }
    void SetHP(int32 _value) { m_monsterStatus.hp = _value; }
    void SetAD(float _value) { m_monsterStatus.adPower = _value; }
    void SetHitRange(float _value) { m_monsterStatus.hitRange = _value; }
    void SetHitSpeed(float _value) { m_monsterStatus.hitSpeed = _value; }
    void SetMoveSpeed(float _value) { m_monsterStatus.moveSpeed = _value; }

    void ChangeState(shared_ptr<AI> _nextAI);
    void ChangeState(wstring&& _key);

    void Damaged(DamageInfo _damage);
    void Damaged(shared_ptr<GameObject> _attacker, int _damage);

    void SetDead(bool _dead) { m_isDead = _dead; }
    bool IsDead() { return m_isDead; }

    void Death(shared_ptr<GameObject> killer);

    float CalculateExpReward();
    shared_ptr<ItemBox> GetItemBox() { return m_itembox; }

protected:
    //아이템 보유 가능. 죽을 시 열어볼 수 있음. 
    array<shared_ptr<Item>, 8> m_inventory;

    //모델, 애니메이터
    shared_ptr<Model> m_model;
    shared_ptr<Rigidbody> m_rigidbody;
    shared_ptr<SphereCollider> m_collider;
    shared_ptr<NavMeshAgent> m_navAgent;
    shared_ptr<Shader> m_defaultShader;
    shared_ptr<MonsterStateMachine> m_monsterStateMachine;
    shared_ptr<AnimationStateMachine> m_animationStateMachine;
    shared_ptr<ItemBox> m_itembox;

    MonsterStatus m_monsterStatus;

    shared_ptr<MonsterInterface> m_monsterInterface;

  
    shared_ptr<Player> m_targetPlayer;
    shared_ptr<HealthBar> m_healthBar;

    shared_ptr<AI> m_curAI;
    unordered_map<wstring, shared_ptr<AI>> m_AIMap;

private:
    float m_isStun = 0.f;
    bool m_isDead = false;

    bool m_isAttacked = false;
};

