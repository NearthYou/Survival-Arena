// ItemSlot.cpp
#include "pch.h"
#include "ItemSlot.h"
#include "Material.h"

ItemSlot::ItemSlot(shared_ptr<Item> item, bool isNeedToShowSlotIndex) 
    : Super(ComponentType::Custom)
    , m_isNeedToShowSlotIndex(isNeedToShowSlotIndex)
{
    SetItem(item);

}

ItemSlot::~ItemSlot()
{
}

void ItemSlot::CreateSlot(Vec2 localPos, Vec2 size, int slotIndex)
{
    m_slotIndex = slotIndex;
    m_slotSize = size;

    // 기본 슬롯 패널 생성
    //m_slotObject = make_shared<GameObject>();
    //m_slotObject->SetName(L"ItemSlot_" + to_wstring(slotIndex));

    //m_slotObject->AddComponent(make_shared<UIPanel>()); 

    //m_slotPanel = m_slotObject->GetUIPanel();

    m_slotPanel = m_parentPanel->GetUIPanel()->AddPanel(localPos, size, nullptr, L"SlotPanel" + to_wstring(slotIndex));

   /* if(m_slotType == SLOTTYPE::INVENTORY)
         m_slotPanel->Create(localPos, size, Vec4(0.f, 0.f, 0.f, 0.5f), nullptr);
    else
         m_slotPanel->Create(localPos, size, Vec4(0.f), RESOURCES->Get<Material>(L"Img_Item_Slot_Common")->Clone());*/

    //m_slotPanel->Create(localPos, size, Vec4(0.f, 0.f, 0.f, 0.5f), nullptr);

    if (m_isNeedToShowSlotIndex)
    {
        m_slotPanel->AddD2DText(
            Vec2(27, 24),
            to_wstring(slotIndex),
            10.f,
            Vec4(1.f),
            1.f,
            Vec4(0.f),
            0.f,
            L"SlotIndex",
            TextAlignment::Left
        );
    }

    //Vec3 panelPos = m_slotPanel->GetGameObject()->GetTransform()->GetPosition();
    //panelPos.z -= 0.01;
    //m_slotPanel->GetGameObject()->GetTransform()->SetPosition(panelPos);

    //// Scene에 추가
    //CURSCENE->AddUIObject(m_slotObject, true);
    //CURSCENE->RegisterUIChild(m_slotObject);

    // ★ 중요: 부모 UIPanel의 자식으로 Transform 설정
    //if (m_parentPanel) {
    //    //m_slotObject->GetTransform()->SetParent(m_parentPanel->GetTransform());

    //    // 부모 UIPanel의 자식 컨테이너에도 등록
    //    auto& childElements = const_cast<vector<weak_ptr<GameObject>>&>(m_parentPanel->GetUIPanel()->GetChildElements());
    //    childElements.push_back(m_slotObject);

    //    // Scene에는 자식으로 등록
    //    CURSCENE->AddUIObject(m_slotObject, false);  // false = 자식
    //    CURSCENE->RegisterUIChild(m_slotObject);
    //}
    //else {
    //    // 기존 방식
    //    CURSCENE->AddUIObject(m_slotObject, true);
    //    CURSCENE->RegisterUIChild(m_slotObject);
    //}

    UpdateSlotUI();
}

void ItemSlot::SetItem(shared_ptr<Item> item)
{
    m_item = item;
    UpdateSlotUI();
}

void ItemSlot::ClearItem()
{
    m_item = nullptr;
    UpdateSlotUI();
}

void ItemSlot::UpdateSlotUI()
{
    // 기존 UI 요소들 정리 (중요!)
    if (m_slotPanel) {
        m_slotPanel->RemoveUIElementSafely(L"Button");
        m_slotPanel->RemoveUIElementSafely(L"ImageUI");
        m_slotPanel->SetVisible(false);
        m_slotButton.reset();
        m_iconImageUI.reset();
    }

    if (m_item)
    {
      
        UpdatePanel(); // 새로운 아이템 UI 생성
    }
    else
    {
        if (m_slotPanel)
        {
            if (m_slotType == SLOTTYPE::INVENTORY)
            {
                HidePanel(); // 인벤에서 장비 없으면 정보 숨김
            }
            else if (m_slotType == SLOTTYPE::EQUIPMENT)
            {
                UpdateEquipmentSlot(); // 장비 슬롯 기본 아이콘 표시
            }
        }
    }
}

void ItemSlot::UpdatePanel()
{
    if (!m_item || !m_slotPanel) return;
    m_slotPanel->SetVisible(true);
    // 기존 버튼이 있으면 이벤트 정리
    if (m_slotButton) {
        m_slotButton->OnClick.Reset(); // 모든 이벤트 제거
    }

    // 아이템 등급에 따른 슬롯 배경 Material 선택
    ITEMGRADE itemGrade = m_item->GetItemGrade();
    wstring btnMaterialTag = L"Img_Item_Slot_";
    switch (itemGrade)
    {
    case ITEMGRADE::COMMON:
        btnMaterialTag += L"Common";
        break;
    case ITEMGRADE::UNCOMMON:
        btnMaterialTag += L"Uncommon";
        break;
    case ITEMGRADE::RARE:
        btnMaterialTag += L"Rare";
        break;
    case ITEMGRADE::EPIC:
        btnMaterialTag += L"Epic";
        break;
    case ITEMGRADE::LEGENDARY:
        btnMaterialTag += L"Legendary";
        break;
    }

    // 새로운 버튼 생성
    Vec2 panelSize = m_slotPanel->GetSize();
    m_slotButton = m_slotPanel->AddButton(
        panelSize / 2.f,
        m_slotSize * (1 / RESOLUTION_CONSTANT),
        RESOURCES->Get<Material>(btnMaterialTag)->Clone(),
        L"Button"
    );

    // 클릭 이벤트 재등록 (중요!)
    m_slotButton->OnClick += [this]() {
        //cout << "슬롯 " << m_slotIndex << " 클릭됨!" << endl;
        OnSlotClicked(m_slotIndex, m_slotType);
        //OnSlotRightClicked(m_slotIndex, m_slotType);
    };
    m_slotButton->OnRightClick += [this]() {
        //cout << "슬롯 " << m_slotIndex << " 클릭됨!" << endl;
        //OnSlotClicked(m_slotIndex, m_slotType);
        OnSlotRightClicked(m_slotIndex, m_slotType);
    };

    // 새로운 아이템 아이콘 생성
    int32 itemID = m_item->GetItemID();
    wstring imgMaterialTag = L"ItemIcon_" + to_wstring(itemID);

    m_iconImageUI = m_slotPanel->AddImageUI(Vec2(0.f), L"ImageUI");
    m_iconImageUI->AddImageLayer(
        0,
        panelSize / 2.f,
        m_slotSize * (1 / RESOLUTION_CONSTANT),
        RESOURCES->Get<Material>(imgMaterialTag)->Clone(),
        1
    );
}

void ItemSlot::HidePanel()
{
    m_slotPanel->RemoveUIElementSafely(L"Button");
    m_slotPanel->RemoveUIElementSafely(L"ImageUI");

    m_slotButton.reset();
    m_iconImageUI.reset();
}

void ItemSlot::UpdateEquipmentSlot()
{
    m_slotPanel->SetBackgroundMaterial(RESOURCES->Get<Material>(L"Img_Item_Slot_Common")->Clone());
    Vec2 panelSize = m_slotPanel->GetSize();
    auto imageUI = m_slotPanel->AddImageUI(Vec2(0.f), L"ImageUI");
    
    wstring iconTag = L"Ico_Status_";
    switch (m_slotIndex)
    {
    case 0 :
        iconTag += L"Weapon";
        break;
    case 1:
        iconTag += L"Armor";
        break;
    case 2:
        iconTag += L"Head";
        break;
    case 3:
        iconTag += L"Arm";
        break;
    case 4:
        iconTag += L"Leg";
        break;
    }
    shared_ptr<Material> iconMaterial = RESOURCES->Get<Material>(iconTag)->Clone();
    Vec2 size = iconMaterial->GetDiffuseMap()->GetSize();
    imageUI->AddImageLayer(0, panelSize / 2.f, size * 0.5, iconMaterial, 1);
}

void ItemSlot::Update()
{
    Super::Update();

    UpdatePositionFromParent();
}

void ItemSlot::SetSelected(bool selected)
{
    if (m_isSelected != selected)
    {
        m_isSelected = selected;
    }
}

void ItemSlot::UpdatePositionFromParent()
{
    if (!m_parentPanel || !m_slotPanel) return;

    // 부모 패널의 월드 위치를 기반으로 새로운 위치 계산
    Vec2 newWorldPos = m_parentPanel->GetUIPanel()->LocalToWorldPosition(m_localPosition);
    m_slotPanel->SetPosition(newWorldPos);
}