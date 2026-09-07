#include "pch.h"
#include "ItemManager.h"
#include "ItemBox.h"
#include "ItemSlot.h"
#include "Item.h"

ItemBox::ItemBox()
{

}

ItemBox::~ItemBox()
{
}

void ItemBox::Start()
{
	//m_boxInventory[0] = ItemManager::GetInstance()->GetItem(L"¸ÁÄ¡");
	//m_boxInventory[1] = ItemManager::GetInstance()->GetItem(L"¸ÁÄ¡");
	//m_boxInventory[2] = ItemManager::GetInstance()->GetItem(L"¸ÁÄ¡");
	//m_boxInventory[3] = ItemManager::GetInstance()->GetItem(L"¸ÁÄ¡");
	//m_boxInventory[4] = ItemManager::GetInstance()->GetItem(L"¸ÁÄ¡");
	Super::Start();
}

void ItemBox::Update()
{
	Super::Update();
}

void ItemBox::LateUpdate()
{
	Super::LateUpdate();
}


shared_ptr<Item> ItemBox::InsertItem(int _index, shared_ptr<Item> _item)
{
	if (m_boxInventory[_index] != nullptr) {
		auto item = m_boxInventory[_index];
		m_boxInventory[_index] = _item;
		return item;
	}
	else {
		return nullptr;
	}
}

shared_ptr<Item> ItemBox::DeleteItem(int _index)
{
	if (m_boxInventory[_index] == nullptr) return nullptr;
	auto item = m_boxInventory[_index];
	m_boxInventory[_index] = nullptr;
	return item;
}

bool ItemBox::PushItem(shared_ptr<Item> _item)
{
	for (auto item : m_boxInventory) {
		if (item == nullptr) {
			item = _item;
			return true;
		}
	}
	return false;
}


