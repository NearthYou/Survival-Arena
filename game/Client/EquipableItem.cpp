#include "pch.h"
#include "EquipableItem.h"
#include "Player.h"
#include "Item.h"

EquipableItem::EquipableItem()
{

}

EquipableItem::~EquipableItem()
{

}

bool EquipableItem::Use()
{
	return false;
}

bool EquipableItem::CanUse() const
{
	return false;
}

Item* EquipableItem::Clone() const
{
	return nullptr;
}
