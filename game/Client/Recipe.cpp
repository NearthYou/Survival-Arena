// Recipe.cpp
#include "pch.h"
#include "Recipe.h"
#include "ItemManager.h"
#include "ItemSlot.h"
#include "InventoryManager.h"

Recipe::Recipe(int32 resultItemID, const RecipeIngredient& ingredient1, const RecipeIngredient& ingredient2)
    : m_resultItemID(resultItemID)
    , m_ingredients{ ingredient1, ingredient2 } 
{
    
}

Recipe::~Recipe()
{
}

bool Recipe::CanCraftFromSlots(const vector<shared_ptr<ItemSlot>>& inventorySlots) const
{
    // 필요한 재료 개수 체크
    map<int32, int32> requiredItems;
    for (const auto& ingredient : m_ingredients)
    {
        requiredItems[ingredient.itemID] += ingredient.quantity;
    }

    // 인벤토리에서 아이템 개수 확인
    map<int32, int32> availableItems;
    int32 emptySlotCount = 0;

    for (const auto& slot : inventorySlots)
    {
        if (slot->IsEmpty())
        {
            emptySlotCount++;
        }
        else
        {
            auto item = slot->GetItem();
            if (item)
            {
                availableItems[item->GetItemID()]++;
            }
        }
    }

    // 필요한 재료가 충분한지 확인
    int32 totalRequiredSlots = 0;
    for (const auto& required : requiredItems)
    {
        if (availableItems[required.first] < required.second)
        {
            return false;
        }
        totalRequiredSlots += required.second;
    }

    // 조합 후 공간 계산: 사용되는 재료 슬롯 수 - 생성되는 결과 아이템 수 = 늘어나는 빈 공간
    // 2개 재료 -> 1개 결과물이므로 빈 공간이 1개 늘어남
    // 따라서 현재 빈 공간이 0개여도 조합 가능
    int32 slotsAfterCraft = emptySlotCount + (totalRequiredSlots - 1); // 2 - 1 = 1개 공간 증가

    return slotsAfterCraft >= 0; // 음수가 아니면 조합 가능
}

bool Recipe::ExecuteCraftFromSlots(vector<shared_ptr<ItemSlot>>& inventorySlots) const
{
    if (!CanCraftFromSlots(inventorySlots))
        return false;

    // 재료 소모할 슬롯들을 먼저 찾기
    vector<shared_ptr<ItemSlot>> slotsToConsume;
    map<int32, int32> toConsume;

    for (const auto& ingredient : m_ingredients)
    {
        toConsume[ingredient.itemID] = ingredient.quantity;
    }

    // 사용할 재료 슬롯들 찾기
    for (auto& slot : inventorySlots)
    {
        if (!slot->IsEmpty())
        {
            auto item = slot->GetItem();
            if (item && toConsume[item->GetItemID()] > 0)
            {
                slotsToConsume.push_back(slot);
                toConsume[item->GetItemID()]--;

                // 모든 재료를 찾았는지 확인
                bool allFound = true;
                for (const auto& consume : toConsume)
                {
                    if (consume.second > 0)
                    {
                        allFound = false;
                        break;
                    }
                }
                if (allFound) break;
            }
        }
    }

    // 재료가 충분하지 않으면 실패
    if (slotsToConsume.size() < 2)
        return false;

    // 결과 아이템 생성
    auto resultItem = ItemManager::GetInstance()->GetItem(m_resultItemID);
    if (!resultItem)
        return false;

    // 첫 번째 재료 슬롯에 결과 아이템 배치
    slotsToConsume[0]->SetItem(resultItem);

    // 나머지 재료 슬롯들 비우기
    for (int i = 1; i < slotsToConsume.size(); i++)
    {
        slotsToConsume[i]->ClearItem();
    }

    // 인벤토리 변화 알림 (InventoryManager를 통해)
    InventoryManager::GetInstance()->NotifyInventoryChanged();

    return true;
}

bool Recipe::CanCraftWithSlots(shared_ptr<ItemSlot> slot1, shared_ptr<ItemSlot> slot2) const
{
    if (!slot1 || !slot2 || slot1->IsEmpty() || slot2->IsEmpty())
        return false;

    auto item1 = slot1->GetItem();
    auto item2 = slot2->GetItem();

    if (!item1 || !item2)
        return false;

    // 두 재료가 레시피와 일치하는지 확인 (순서 무관)
    bool match1 = (item1->GetItemID() == m_ingredients[0].itemID &&
        item2->GetItemID() == m_ingredients[1].itemID);
    bool match2 = (item1->GetItemID() == m_ingredients[1].itemID &&
        item2->GetItemID() == m_ingredients[0].itemID);

    return match1 || match2;
}

bool Recipe::ExecuteCraftWithSlots(shared_ptr<ItemSlot> slot1, shared_ptr<ItemSlot> slot2) const
{
    if (!CanCraftWithSlots(slot1, slot2))
        return false;

    // 결과 아이템 생성
    auto resultItem = ItemManager::GetInstance()->GetItem(m_resultItemID);
    if (!resultItem)
        return false;

    // 첫 번째 슬롯에 결과 아이템 배치, 두 번째 슬롯 비우기
    slot1->SetItem(resultItem);
    slot2->ClearItem();

    return true;
}

shared_ptr<ItemSlot> Recipe::FindEmptySlot(const vector<shared_ptr<ItemSlot>>& inventorySlots) const
{
    for (const auto& slot : inventorySlots)
    {
        if (slot->IsEmpty())
        {
            return slot;
        }
    }
    return nullptr;
}

wstring Recipe::GetRecipeInfo() const
{
    wstring info = L"Recipe: ";

    // 재료 정보
    for (int i = 0; i < 2; i++)
    {
        auto item = ItemManager::GetInstance()->GetItem(m_ingredients[i].itemID);
        if (item)
        {
            info += item->GetName();
            if (m_ingredients[i].quantity > 1)
            {
                info += L"(" + to_wstring(m_ingredients[i].quantity) + L")";
            }
            if (i == 0) info += L" + ";
        }
    }

    // 결과 아이템
    auto resultItem = ItemManager::GetInstance()->GetItem(m_resultItemID);
    if (resultItem)
    {
        info += L" = " + resultItem->GetName();
    }

    return info;
}
