// ItemSlot.cpp
#include "pch.h"
#include "ItemSlot.h"
#include "Material.h"

ItemSlot::ItemSlot() : Super(ComponentType::ItemSlot)
{
}

ItemSlot::~ItemSlot()
{
}

void ItemSlot::CreateSlot(Vec2 localPos, Vec2 size, int slotIndex)
{
    m_slotIndex = slotIndex;
    m_slotSize = size;

    // 기본 슬롯 버튼 생성
    m_slotObject = make_shared<GameObject>();
    m_slotObject->SetName(L"ItemSlot_" + to_wstring(slotIndex));

    m_slotButton = make_shared<Button>();
    m_slotObject->AddComponent(m_slotButton);

    // 슬롯 배경 머티리얼 설정
    auto slotMaterial = RESOURCES->Get<Material>(L"ItemSlotCommon")->Clone();
    m_slotButton->Create(localPos, size, slotMaterial, 1);
    m_slotButton->SetLocalPosition(localPos);

    // 클릭 이벤트 등록
    m_slotButton->OnClick += [this]() {
        OnSlotClicked();
        };

    // 아이템 아이콘용 ImageUI 생성
    m_iconObject = make_shared<GameObject>();
    m_iconObject->SetName(L"ItemIcon_" + to_wstring(slotIndex));

    m_iconImageUI = make_shared<ImageUI>();
    m_iconObject->AddComponent(m_iconImageUI);
    m_iconImageUI->SetLocalPosition(localPos);

    // 아이콘 위치 설정 (슬롯 중앙)
    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();
    float x = localPos.x;
    float y = localPos.y;

    m_iconObject->GetTransform()->SetPosition(Vec3(x, y, 0.3f)); // 슬롯보다 앞쪽
    m_iconObject->SetLayerIndex(LAYER_UI);



    // Scene에 추가
    CURSCENE->AddUIObject(m_slotObject, false);
    CURSCENE->RegisterUIChild(m_slotObject);

    CURSCENE->AddUIObject(m_iconObject, false);
    CURSCENE->RegisterUIChild(m_iconObject);

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
    if (m_item)
    {
        UpdateItemIcon();
    }
    else
    {
        // 아이템이 없으면 아이콘과 텍스트 숨김
        if (m_iconImageUI)
        {
            m_iconImageUI->ClearAllLayers();
        }
    }

    UpdateSlotBackground();
}

void ItemSlot::UpdateItemIcon()
{
    if (!m_item || !m_iconImageUI) return;

    auto itemTexture = m_item->GetImage();
    if (itemTexture)
    {
        // 아이템 아이콘 머티리얼 생성
        auto iconMaterial = make_shared<Material>();
        auto shader = make_shared<Shader>(L"ImageShader.fx");
        iconMaterial->SetShader(shader);
        iconMaterial->SetDiffuseMap(itemTexture);
        iconMaterial->SetRenderQueue(RenderQueue::Transparent);
        iconMaterial->SetTransparent(true);
        iconMaterial->SetRenderingMode(RenderingMode::Forward);

        MaterialDesc& desc = iconMaterial->GetMaterialDesc();
        desc.ambient = Vec4(1.f);
        desc.diffuse = Vec4(1.f);
        desc.specular = Vec4(1.f);

        // 아이템 등급에 따른 색상 적용
        switch (m_item->GetItemGrade())
        {
        case ITEMGRADE::COMMON:
            desc.diffuse = Vec4(1.f, 1.f, 1.f, 1.f);
            break;
        case ITEMGRADE::UNCOMMON:
            desc.diffuse = Vec4(0.5f, 1.f, 0.5f, 1.f);
            break;
        case ITEMGRADE::RARE:
            desc.diffuse = Vec4(0.5f, 0.5f, 1.f, 1.f);
            break;
        case ITEMGRADE::EPIC:
            desc.diffuse = Vec4(0.8f, 0.5f, 1.f, 1.f);
            break;
        case ITEMGRADE::LEGENDARY:
            desc.diffuse = Vec4(1.f, 0.6f, 0.2f, 1.f);
            break;
        }

        Vec2 iconSize = Vec2(m_slotSize.x - 4, m_slotSize.y - 4);
        m_iconImageUI->AddImageLayer(0, Vec2(0, 0), iconSize, iconMaterial, 1);
    }
}



void ItemSlot::UpdateSlotBackground()
{
    if (!m_slotButton) return;

    if (m_isSelected)
    {
        // 선택된 상태 - 골드 테두리
        auto selectedMaterial = RESOURCES->Get<Material>(L"ItemSlotCommon")->Clone();
        auto& desc = selectedMaterial->GetMaterialDesc();
        desc.diffuse = Vec4(1.f, 1.f, 0.5f, 1.f);
        m_slotButton->SetMaterial(ButtonState::Normal, selectedMaterial);
    }
    else
    {
        // 기본 상태
        auto normalMaterial = RESOURCES->Get<Material>(L"ItemSlotCommon")->Clone();
        m_slotButton->SetMaterial(ButtonState::Normal, normalMaterial);
    }
}

void ItemSlot::OnSlotClicked()
{
    if (m_onClickCallback)
    {
        m_onClickCallback(m_slotIndex);
    }
}

void ItemSlot::OnSlotRightClicked()
{
    if (m_onRightClickCallback)
    {
        m_onRightClickCallback(m_slotIndex);
    }
}

void ItemSlot::Update()
{
    Super::Update();

    // 우클릭 처리
    if (INPUT->GetButtonDown(KEY_TYPE::RBUTTON))
    {
        if (m_slotButton && m_slotButton->Picked(INPUT->GetMousePos()))
        {
            OnSlotRightClicked();
        }
    }
}

void ItemSlot::SetSelected(bool selected)
{
    if (m_isSelected != selected)
    {
        m_isSelected = selected;
        UpdateSlotBackground();
    }
}
