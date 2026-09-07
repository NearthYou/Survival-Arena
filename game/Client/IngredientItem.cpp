#include "pch.h"
#include "IngredientItem.h"

IngredientItem::IngredientItem()
{

}

IngredientItem::~IngredientItem()
{

}

bool IngredientItem::Use()
{
	return false;
}

bool IngredientItem::CanUse() const
{
	return false;
}

Item* IngredientItem::Clone() const
{
	return nullptr;
}
