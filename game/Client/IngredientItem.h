#pragma once
#include "Item.h"
class IngredientItem :
    public Item
{
public:
    IngredientItem();
    virtual ~IngredientItem();

    // Item 인터페이스 구현
    bool Use() override;
    bool CanUse() const override;
    Item* Clone() const override;
};

