#include "pch.h"
#include "EquipmentPanelUI.h"

#include "ItemSlot.h"

EquipmentPanelUI::EquipmentPanelUI()
{

}
EquipmentPanelUI::~EquipmentPanelUI()
{

}

void EquipmentPanelUI::Update()
{

}

void EquipmentPanelUI::Initialize()
{
	CreatePanels();
}

void EquipmentPanelUI::SetVisible(bool visible)
{
	m_isVisible = visible;
	m_panel->GetUIPanel()->SetVisible(visible);
}

void EquipmentPanelUI::Cleanup()
{

}

void EquipmentPanelUI::CreatePanels()
{
	m_panel = make_shared<GameObject>();
	m_panel->SetName(L"CharEquipPanel");

	auto panel = make_shared<UIPanel>();
	m_panel->AddComponent(panel);

	panel->Create(Vec2(419.f, 768 - 57), Vec2(38, 115), Vec4(0.075f,0.105f,0.12f,0.97f), nullptr);
	m_panel->SetLayerIndex(LAYER_UI);

	CURSCENE->AddUIObject(m_panel, true);
	CURSCENE->RegisterUIParent(m_panel);

	CreateEquipmentSlots();
}
//0.12 , 0.15, 0.18
void EquipmentPanelUI::CreateEquipmentSlots()
{
	m_equipmentSlots.clear();

	// 5x2 그리드로 10개 슬롯 생성
	int slotsX = 1;
	int slotsY = 5;
	Vec2 slotSize = Vec2(34, 22);
	Vec2 spacing = Vec2(0, 1);

	//Vec2 startPos = Vec2(380.f - (38 / 2.f) + 19, (768 - 57) - (115 / 2.f) + 13); // 패널 내 시작 위치
	Vec2 startPos = Vec2(17 + 1, 11 + 1);
	Vec2 panelSize = Vec2(38, 115);

	for (int row = 0; row < slotsY; row++)
	{
		for (int col = 0; col < slotsX; col++)
		{
			int slotIndex = row * slotsX + col;

			// ItemSlot 생성
			auto itemSlot = make_shared<ItemSlot>(nullptr, false);
			itemSlot->SetSlotType(SLOTTYPE::EQUIPMENT);
			Vec2 slotPos = Vec2(
				startPos.x + col * (slotSize.x + spacing.x),
				startPos.y + row * (slotSize.y + spacing.y)
			);
			itemSlot->SetParentPanel(m_panel);
			itemSlot->CreateSlot(slotPos, slotSize, slotIndex);

			m_equipmentSlots.push_back(itemSlot);
		}
	}
}

void EquipmentPanelUI::RegisterUIObject(shared_ptr<GameObject> uiObject)
{

}
