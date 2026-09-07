#pragma once
#include "GameObject.h"
#include "ISkill.h"
#include "EquipableItem.h"

class Item;
class EquipableItem;
class BaseSkill;
class PlayerStateMachine;
class PlayerInterface;
class UIManager;
class HealthBar;
class Monster;

struct DamageInfo;
struct ItemStatus;

struct PlayerStatus {
    int level = 1;
    int curExpLimit = 50;
    int curExp = 0;

    int32 max_HP = 500;
    int32 hp = 500;
    int32 max_Stamina = 300;
    int32 stamina = 300;
    
    float hitAttack = 50;
    float hitRange = 15;
    float hitSpeed = 0.54f;
    float defense = 30;
    float cooldownReduction = 0.f;
    float moveSpeed = 3.14;

    //0.25초마다 회복되는 양. 
    float healing = 7.2;
    float healing_Stamina = 11.4;

    int availableSkillPoints = 1;
};


struct PlayerGrowStatus {
    int ExpLimit = 25;

    int32 hp = 78;
    int32 stamina = 46;

    float HitAttack = 5;
    float hitSpeed = 0.04f;
    float defense = 7;

    float healing = 0.56;
    float healing_Stamina = 0.62;
};
class Player :
    public GameObject
{
    using Super = GameObject;
public:
    Player();
    virtual ~Player();
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
    void WearEquipment(shared_ptr<EquipableItem> _item);
    void TakeOffEquipment(int _index);
    void StartCraftAnimation();

    void LevelUp();
    virtual void Birth() = 0;
    virtual void Death() = 0;
    virtual void MakeItem() = 0;
    virtual void MakeFood() = 0;

public:
    //Helper Function.
    bool isStun() { return m_isStun > 0.f ? true : false; }
    PlayerStatus& GetStatus() { return m_status; }
    shared_ptr<Shader> GetShader() { return m_defaultShader; }

    void Damaged(DamageInfo _damage);
    void Damaged(shared_ptr<Monster> _attacker, int _damage);

    void SetLevel(int _value);
    void SetCurExpLimit(int _value) { m_status.curExpLimit = _value; }
    void SetCurExp(int _value);
    void SetMaxHP(int32 _value) { m_status.max_HP = _value; }
    void SetHP(int32 _value);
    void SetMaxStamina(int32 _value) { m_status.max_Stamina = _value; }
    void SetStamina(int32 _value);

    void SetHitAttack(float _value) { m_status.hitAttack = _value; }
    void SetHitRange(float _value) { m_status.hitRange = _value; }
    void SetHitSpeed(float _value) { m_status.hitSpeed = _value; }
    void SetDefense(float _value) { m_status.defense = _value; }
    void SetCooldownReduction(float _value) { m_status.cooldownReduction = _value; }
    void SetMoveSpeed(float _value) { m_status.moveSpeed = _value; }
    void SetHealing(float _value) { m_status.healing = _value; }
    void SetHealingStamina(float _value) { m_status.healing_Stamina = _value; }

    void SetAppearanceTint(const Vec4& tint);
    Vec4 GetAppearanceTint() const { return m_appearanceTint; }
    void SetUIManager(weak_ptr<UIManager> _manager) { m_uiManager = _manager; }

    void SetIsAttacked(bool _isAttacked) { m_isAttacked = _isAttacked; }
    bool IsAttacked() { return m_isAttacked; }
   
public:
    void AddSkillPoint(int points = 1) { m_status.availableSkillPoints += points; }
    bool HasSkillPoints() const { return m_status.availableSkillPoints > 0; }
    //bool TryLevelUpSkill(int skillIndex);

private:
    void ApplyEquipStatus(const ItemStatus& _Equipstatus);
    void ReleaseEquipStatus(const ItemStatus& _Equipstatus);
    

protected:
    Vec4 m_appearanceTint = Vec4(1.f,1.f,1.f,1.f);
    PlayerStatus m_status;
    PlayerGrowStatus m_growStatus;

    //Q,W,E,R 스킬.
    //array<shared_ptr<ISkillExecutor>, 4> m_skillExecutors;
    array<unique_ptr<ISkill>, 4> m_skills;

    //무기, 상의, 머리, 팔, 다리 순서. 
    array<shared_ptr<EquipableItem>, 5> m_curEquipment;

    //모든 아이템 전부 보유 가능.(기타, 소비, 장비)
    array<shared_ptr<Item>, 10> m_inventory;

    //모델, 애니메이터
    shared_ptr<Model> m_model;
    shared_ptr<Rigidbody> m_rigidbody;
    shared_ptr<SphereCollider> m_collider;
    shared_ptr<NavMeshAgent> m_navAgent;
    shared_ptr<PlayerStateMachine> m_playerStateMachine;
    shared_ptr<Shader> m_defaultShader;
    
    //PlayerInterface
    shared_ptr<PlayerInterface> m_playerInterface;

    //그 이외에 UI들(체력, 경험치 등등) 
    //연동 위해서 필요함. (따로 UI클래스들 만들어야함)
    shared_ptr<HealthBar> m_healthBar;
   
    bool m_isAttacked = false;

private:
    float m_isStun = 0.f;
    float m_healingCoolTime = 0.f;

    weak_ptr<UIManager> m_uiManager;

public:
    ISkill* GetSkill(int index) const
    {
        if (index >= 0 && index < 4 && m_skills[index])
            return m_skills[index].get();
        return nullptr;
    }
   
    friend class PlayerInterface;

};

