// ItemSlot.h
#pragma once
#include "Component.h"
#include "Button.h"
#include "ImageUI.h"
#include "D2DText.h"
#include "Item.h"
#include "Delegate.h"

enum class SLOTTYPE
{
    EQUIPMENT,
    INGREDIENTS,
    INVENTORY
};

class ItemSlot : public Component
{
public:
    ItemSlot(shared_ptr<Item> item, bool isNeedToShowSlotIndex);
    virtual ~ItemSlot();
    using Super = Component;

    //슬롯 타입 설정
    void SetSlotType(SLOTTYPE slotType) { m_slotType = slotType; }
    SLOTTYPE GetSlotType() { return m_slotType; }

    // 아이템 설정/해제
    void SetItem(shared_ptr<Item> item);
    shared_ptr<Item> GetItem() const { return m_item; }
    void ClearItem();

    // 슬롯 생성
    void CreateSlot(Vec2 localPos, Vec2 size, int slotIndex);

    // 상태 관리
    void SetSelected(bool selected);
    bool IsEmpty() const { return m_item == nullptr; }
    int GetSlotIndex() const { return m_slotIndex; }

    // UI 업데이트
    void UpdateSlotUI();

    virtual void Update() override;

    //부모 패널 세팅
    void SetParentPanel(shared_ptr<GameObject> _parent) { m_parentPanel = _parent; }

    //로컬 좌표 세팅
    void SetLocalPosition(const Vec2& localPos) { m_localPosition = localPos; }
    const Vec2& GetLocalPosition() const { return m_localPosition; }

    // 델리게이트 선언 (슬롯 인덱스와 타입을 전달)
    Delegate::Delegate<int, SLOTTYPE> OnSlotClicked;
    Delegate::Delegate<int, SLOTTYPE> OnSlotRightClicked;


    void UpdatePositionFromParent();

private:
    void UpdatePanel();
    void HidePanel();
    void UpdateEquipmentSlot();
  

private:
    Vec2 m_localPosition = Vec2::Zero;

    SLOTTYPE m_slotType;
    shared_ptr<Item> m_item;
    int m_slotIndex = -1;
    bool m_isNeedToShowSlotIndex = false;
    bool m_isSelected = false;

    //이 아이템슬롯을 담는 더 상위 Panel 참고
    shared_ptr<GameObject> m_parentPanel;

    // UI 컴포넌트들
    shared_ptr<UIPanel> m_slotPanel;
    shared_ptr<Button> m_slotButton;
    shared_ptr<ImageUI> m_iconImageUI;

    // UI 참조
    shared_ptr<GameObject> m_slotObject;
   
    // 크기 정보
    Vec2 m_slotSize;
};
