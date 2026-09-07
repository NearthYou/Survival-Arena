// InventoryManager.h
#pragma once
#include "Component.h"
#include "ItemSlot.h"

#include "EquipableItem.h"

class Player;
class Recipe;

class InventoryManager
{
    DECLARE_SINGLE(InventoryManager);

public:
    virtual ~InventoryManager();


    // 슬롯 등록
    void RegisterInventorySlots(const vector<shared_ptr<ItemSlot>>& inventorySlots);
    void RegisterEquipmentSlots(const vector<shared_ptr<ItemSlot>>& equipmentSlots);

    // 아이템 착용/해제
    bool EquipItem(int inventorySlotIndex);
    bool UnequipItem(int equipmentSlotIndex);

    // 슬롯 간 아이템 이동
    bool MoveItem(int fromSlot, int toSlot, SLOTTYPE fromType, SLOTTYPE toType);

    // 장비 타입별 슬롯 인덱스 가져오기
    int GetEquipmentSlotIndex(EquipmentType equipType);

    // 이벤트 처리
    void OnInventorySlotClicked(int slotIndex);
    void OnEquipmentSlotClicked(int slotIndex);

    void Update();

    void SetPlayer(shared_ptr<Player> player) { m_player = player; }
    shared_ptr<Player> GetPlayer() { return m_player; }

    vector<shared_ptr<ItemSlot>>& GetInventorySlots() { return m_inventorySlots; }

    // 조합 관련 함수들
    bool TryCraftItems(int slot1Index, int slot2Index);
    vector<shared_ptr<Recipe>> GetAvailableRecipes() const;
    void OnSlotRightClicked(int slotIndex); // 우클릭으로 조합 모드 활성화

    //아이템 추가
    bool PushItem(shared_ptr<Item> inputItem);
    bool IsEmpty();

public:
    // 인벤토리 변화 알림 델리게이트
    Delegate::Delegate<> OnInventoryChanged;
    void NotifyInventoryChanged();

private:
    // 내부 함수들
    bool CanEquipItem(shared_ptr<EquipableItem> equipItem);
    void SwapItems(shared_ptr<ItemSlot> slot1, shared_ptr<ItemSlot> slot2);

private:
    // 슬롯 참조
    vector<shared_ptr<ItemSlot>> m_inventorySlots;
    vector<shared_ptr<ItemSlot>> m_equipmentSlots;

    //Player
    shared_ptr<Player> m_player;

    // 상태 관리
    shared_ptr<ItemSlot> m_selectedSlot;
    int m_selectedSlotIndex = -1;
    SLOTTYPE m_selectedSlotType;

    bool m_craftingMode = false;
    shared_ptr<ItemSlot> m_craftingSlot1;
    shared_ptr<ItemSlot> m_craftingSlot2;
};
