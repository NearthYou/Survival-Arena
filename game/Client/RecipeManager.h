// RecipeManager.h
#pragma once
#include "Recipe.h"

class ItemSlot;

class RecipeManager
{
    DECLARE_SINGLE(RecipeManager);
    ~RecipeManager();

public:
    void Initialize();

    // 레시피 등록
    void RegisterRecipe(shared_ptr<Recipe> recipe);

    // ID로 레시피 찾기
    shared_ptr<Recipe> FindRecipeByResult(int32 resultItemID) const;

    // 두 재료로 만들 수 있는 레시피 찾기
    shared_ptr<Recipe> FindRecipeByIngredients(int32 ingredient1ID, int32 ingredient2ID) const;

    // 두 슬롯으로 조합 가능한 레시피 찾기
    shared_ptr<Recipe> FindRecipeBySlots(shared_ptr<ItemSlot> slot1, shared_ptr<ItemSlot> slot2) const;

    // 인벤토리 슬롯들에서 조합 가능한 레시피들 찾기
    vector<shared_ptr<Recipe>> GetCraftableRecipesFromSlots(const vector<shared_ptr<ItemSlot>>& inventorySlots) const;

    // 특정 재료가 포함된 레시피들 찾기
    vector<shared_ptr<Recipe>> GetRecipesByIngredient(int32 ingredientID) const;

    // 인벤토리 매니저와 연동한 조합 실행
    bool TryCraftWithInventoryManager(shared_ptr<ItemSlot> slot1, shared_ptr<ItemSlot> slot2);

    // 모든 레시피 가져오기
    const map<int32, shared_ptr<Recipe>>& GetAllRecipes() const { return m_recipes; }

private:
    map<int32, shared_ptr<Recipe>> m_recipes; // resultItemID -> Recipe
    multimap<int32, shared_ptr<Recipe>> m_recipesByIngredient; // ingredientID -> Recipe

    void SetupDefaultRecipes();
    void AddRecipeToIngredientMap(shared_ptr<Recipe> recipe);
};
