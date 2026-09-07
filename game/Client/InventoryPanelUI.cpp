#include "pch.h"
#include "InventoryPanelUI.h"

#include "EquipableItem.h"
#include "Item.h"
#include "ItemSlot.h"
#include "ItemManager.h"

InventoryPanelUI::InventoryPanelUI()
{

}

InventoryPanelUI::~InventoryPanelUI()
{

}

void InventoryPanelUI::Initialize()
{
	CreatePanels();
}

void InventoryPanelUI::Update()
{

}

void InventoryPanelUI::SetVisible(bool visible)
{

}

void InventoryPanelUI::Cleanup()
{

}



void InventoryPanelUI::CreateBaseSlots()
{

}

void InventoryPanelUI::CreatePanels()
{
	m_panel = make_shared<GameObject>();
	m_panel->SetName(L"CharMainPanel");

	auto panel = make_shared<UIPanel>();
	m_panel->AddComponent(panel);
	//size y 56Ãß°¡. 
	panel->Create(Vec2(948.f, 768 - 56), Vec2(272, 118), Vec4(0.075f,0.105f,0.12f,0.97f), nullptr);
	m_panel->SetLayerIndex(LAYER_UI);

	CURSCENE->AddUIObject(m_panel, true);
	CURSCENE->RegisterUIParent(m_panel);

	CreateBaseSlots();
	CreateInventorySlots();
}

void InventoryPanelUI::RegisterUIObject(shared_ptr<GameObject> uiObject)
{

}

void InventoryPanelUI::CreateInventorySlots()
{
	m_inventorySlots.clear();

	// 5x2 ±×¸®µå·Î 10°³ ½½·Ô »ý¼º
	int slotsX = 5;
	int slotsY = 2;
	Vec2 slotSize = Vec2(46, 36);
	Vec2 spacing = Vec2(5, 5);
	//Vec2 startPos = Vec2(960.f - (252 / 2.f) + 23, (768 - 57) - (62 / 2.f) + 14); // ÆÐ³Î ³» ½ÃÀÛ À§Ä¡
	Vec2 startPos = Vec2(31 + 2, 34 + 2);
	Vec2 panelSize = Vec2(252, 62);

	for (int row = 0; row < slotsY; row++)
	{
		for (int col = 0; col < slotsX; col++)
		{
			int slotIndex = row * slotsX + col;

			// ItemSlot »ý¼º
			shared_ptr<ItemSlot> itemSlot = make_shared<ItemSlot>(nullptr, true);
			//shared_ptr<ItemSlot> itemSlot;
			//if		(row == 0 && col == 0) itemSlot = make_shared<ItemSlot>(ItemManager::GetInstance()->GetItem(L"¸ÁÄ¡"), true);
			//else if (row == 0 && col == 1) itemSlot = make_shared<ItemSlot>(ItemManager::GetInstance()->GetItem(L"¼è±¸½½"), true);
			//else if (row == 0 && col == 2) itemSlot = make_shared<ItemSlot>(ItemManager::GetInstance()->GetItem(L"¿îµ¿È­"), true);
			//else if (row == 0 && col == 3) itemSlot = make_shared<ItemSlot>(ItemManager::GetInstance()->GetItem(L"¿ø¼®"), true);
			//else if (row == 0 && col == 4) itemSlot = make_shared<ItemSlot>(ItemManager::GetInstance()->GetItem(L"µ¨Å¸ ·¹µå"), true);
			//
			//else if (row == 1 && col == 0) itemSlot = make_shared<ItemSlot>(ItemManager::GetInstance()->GetItem(L"ºØ´ë"), true);
			//else if (row == 1 && col == 1) itemSlot = make_shared<ItemSlot>(ItemManager::GetInstance()->GetItem(L"±êÅÐ"), true);
			//else if (row == 1 && col == 2) itemSlot = make_shared<ItemSlot>(ItemManager::GetInstance()->GetItem(L"²É"), true);
			//else if (row == 1 && col == 3) itemSlot = make_shared<ItemSlot>(ItemManager::GetInstance()->GetItem(L"µ¹¸æÀÌ"), true);
			//else if (row == 1 && col == 4) itemSlot = make_shared<ItemSlot>(ItemManager::GetInstance()->GetItem(L"µ¨Å¸ ·¹µå"), true);

			//else itemSlot =	make_shared<ItemSlot>(ItemManager::GetInstance()->GetItem(L"µ¹¸æÀÌ"), true);

			itemSlot->SetSlotType(SLOTTYPE::INVENTORY);
			Vec2 slotPos = Vec2(
				startPos.x + col * (slotSize.x + spacing.x),
				startPos.y + row * (slotSize.y + spacing.y)
			);
			itemSlot->SetParentPanel(m_panel);
			itemSlot->CreateSlot(slotPos, slotSize, slotIndex);

			m_inventorySlots.push_back(itemSlot);
		}
	}
}
