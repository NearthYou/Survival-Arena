// Recipe.h
#pragma once

#include "ItemSlot.h"

struct RecipeIngredient {
    int32 itemID;
    int32 quantity = 1;

    RecipeIngredient(int32 id, int32 qty = 1) : itemID(id), quantity(qty) {}
};

class Recipe
{
public:
    Recipe(int32 resultItemID, const RecipeIngredient& ingredient1, const RecipeIngredient& ingredient2);
    ~Recipe();

    // 조합 결과 아이템 ID
    int32 GetResultItemID() const { return m_resultItemID; }

    // 필요한 재료들 (항상 2개)
    const array<RecipeIngredient, 2>& GetIngredients() const { return m_ingredients; }

    // 인벤토리 슬롯 기반 조합 가능 여부 확인
    bool CanCraftFromSlots(const vector<shared_ptr<ItemSlot>>& inventorySlots) const;

    // 인벤토리 슬롯에서 조합 실행
    bool ExecuteCraftFromSlots(vector<shared_ptr<ItemSlot>>& inventorySlots) const;

    // 특정 두 슬롯으로 조합 가능한지 확인
    bool CanCraftWithSlots(shared_ptr<ItemSlot> slot1, shared_ptr<ItemSlot> slot2) const;

    // 특정 두 슬롯으로 조합 실행
    bool ExecuteCraftWithSlots(shared_ptr<ItemSlot> slot1, shared_ptr<ItemSlot> slot2) const;

    // 디버그용 - 레시피 정보 출력
    wstring GetRecipeInfo() const;

private:
    int32 m_resultItemID;
    array<RecipeIngredient, 2> m_ingredients; // 항상 2개의 재료

    // 빈 슬롯 찾기
    shared_ptr<ItemSlot> FindEmptySlot(const vector<shared_ptr<ItemSlot>>& inventorySlots) const;
};
