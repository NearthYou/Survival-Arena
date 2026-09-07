#pragma once
#include "Item.h"

enum class EquipmentType {
    DEFAULT = -1,
    WEAPON = 0,
    CHEST,      // 상의
    HEAD,       // 머리
    ARM,        // 팔
    LEG        // 다리
};

struct ItemStatus 
{
    int32 attackPower = 0;
    int32 defense = 0;
    int32 maxHP = 0;
    int32 maxSP = 0;
    float attackSpeed = 0;
    float moveSpeed = 0;
    float hpRegen = 0;
    float spRegen = 0;
    float lifeSteal = 0;
    float cooldownReduction = 0;
};

class EquipableItem : public Item
{
public:
    EquipableItem();
    virtual ~EquipableItem();

    // Item 인터페이스 구현
    bool Use() override;
    bool CanUse() const override;
    Item* Clone() const override;

    // 장비 타입
    void SetEquipType(EquipmentType _type) { m_equipType = _type; }
    EquipmentType GetEquipType() const { return m_equipType; }

    void SetStatus(const ItemStatus& _status) { m_status = _status; }
    const ItemStatus& GetStatus() const { return m_status; }

private:
    EquipmentType m_equipType;
    ItemStatus m_status;

};
