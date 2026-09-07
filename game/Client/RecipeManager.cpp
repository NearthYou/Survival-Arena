// RecipeManager.cpp
#include "pch.h"
#include "RecipeManager.h"
#include "ItemManager.h"
#include "ItemSlot.h"
#include "InventoryManager.h"

RecipeManager::~RecipeManager()
{
}

void RecipeManager::Initialize()
{
    SetupDefaultRecipes();
}

void RecipeManager::RegisterRecipe(shared_ptr<Recipe> recipe)
{
    if (!recipe) return;

    m_recipes[recipe->GetResultItemID()] = recipe;
    AddRecipeToIngredientMap(recipe);
}

void RecipeManager::AddRecipeToIngredientMap(shared_ptr<Recipe> recipe)
{
    for (const auto& ingredient : recipe->GetIngredients())
    {
        m_recipesByIngredient.insert({ ingredient.itemID, recipe });
    }
}

shared_ptr<Recipe> RecipeManager::FindRecipeByResult(int32 resultItemID) const
{
    auto it = m_recipes.find(resultItemID);
    return (it != m_recipes.end()) ? it->second : nullptr;
}

shared_ptr<Recipe> RecipeManager::FindRecipeByIngredients(int32 ingredient1ID, int32 ingredient2ID) const
{
    for (const auto& pair : m_recipes)
    {
        const auto& ingredients = pair.second->GetIngredients();

        // 순서 상관없이 두 재료가 일치하는지 확인
        bool match1 = (ingredients[0].itemID == ingredient1ID && ingredients[1].itemID == ingredient2ID);
        bool match2 = (ingredients[0].itemID == ingredient2ID && ingredients[1].itemID == ingredient1ID);

        if (match1 || match2)
        {
            return pair.second;
        }
    }
    return nullptr;
}

shared_ptr<Recipe> RecipeManager::FindRecipeBySlots(shared_ptr<ItemSlot> slot1, shared_ptr<ItemSlot> slot2) const
{
    if (!slot1 || !slot2 || slot1->IsEmpty() || slot2->IsEmpty())
        return nullptr;

    auto item1 = slot1->GetItem();
    auto item2 = slot2->GetItem();

    if (!item1 || !item2)
        return nullptr;

    return FindRecipeByIngredients(item1->GetItemID(), item2->GetItemID());
}

vector<shared_ptr<Recipe>> RecipeManager::GetCraftableRecipesFromSlots(const vector<shared_ptr<ItemSlot>>& inventorySlots) const
{
    vector<shared_ptr<Recipe>> craftableRecipes;

    for (const auto& recipe : m_recipes)
    {
        if (recipe.second->CanCraftFromSlots(inventorySlots))
        {
            craftableRecipes.push_back(recipe.second);
        }
    }

    return craftableRecipes;
}

vector<shared_ptr<Recipe>> RecipeManager::GetRecipesByIngredient(int32 ingredientID) const
{
    vector<shared_ptr<Recipe>> recipes;
    auto range = m_recipesByIngredient.equal_range(ingredientID);

    for (auto it = range.first; it != range.second; ++it)
    {
        recipes.push_back(it->second);
    }

    return recipes;
}

bool RecipeManager::TryCraftWithInventoryManager(shared_ptr<ItemSlot> slot1, shared_ptr<ItemSlot> slot2)
{
    auto recipe = FindRecipeBySlots(slot1, slot2);
    if (!recipe)
        return false;

    // 조합 실행
    bool success = recipe->ExecuteCraftWithSlots(slot1, slot2);

    if (success)
    { 
        wstring tmp = recipe->GetRecipeInfo();
        string tmp2(tmp.begin(), tmp.end());
        cout << "조합 성공: " << tmp2 << endl;
        InventoryManager::GetInstance()->NotifyInventoryChanged();
    }

    return success;
}

void RecipeManager::SetupDefaultRecipes()
{
    // 예시: 이터널리턴 스타일의 조합 레시피들

    ItemManager* itemManger = ItemManager::GetInstance();

    //=========================================디바인 피스트=========================================//
    // 아이언 너클(110202) = 목장갑(110102) + 고철(401106)
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"아이언 너클")->GetItemID(), 
            RecipeIngredient(itemManger->GetItem(L"목장갑")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"고철")->GetItemID()))
    );
    // 디바인 피스트(110406) = 아이언 너클(110202) + 십자가(205109)
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"디바인 피스트")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"아이언 너클")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"십자가")->GetItemID()))
    );
    //=========================================디바인 피스트=========================================//
    // 
    // 
    //=========================================제사장의 예복=========================================//
    // 사제복(202211) = 셔츠(202106) + 옷감(401113)
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"사제복")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"셔츠")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"옷감")->GetItemID()))
    );
    // 비파단도(205211) = 마패(401109) + 흑연(401124)
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"비파단도")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"마패")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"흑연")->GetItemID()))
    );
    // 제사장의 예복 = 사제복 + 비파단도
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"제사장의 예복")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"사제복")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"비파단도")->GetItemID()))
    );
    //=========================================제사장의 예복=========================================//



    //=========================================비질란테=========================================//
    // 안전모 = 자전거 헬멧 + 돌멩이
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"안전모")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"자전거 헬멧")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"돌멩이")->GetItemID()))
    );
    // 소방 헬멧 = 자전거 헬멧 + 돌멩이
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"소방 헬멧")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"안전모")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"흑연")->GetItemID()))
    );
    // 비질란테 = 소방 헬멧 + 화약
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"비질란테")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"소방 헬멧")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"화약")->GetItemID()))
    );
    //=========================================비질란테=========================================//

    //=========================================스포츠 시계=========================================//
    // 철사 = 피아노선 + 흑연
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"철사")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"피아노선")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"흑연")->GetItemID()))
    );
    // 고장난 시계 = 손목시계 + 화학품
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"고장난 시계")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"손목시계")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"화학품")->GetItemID()))
    );
    // 스포츠 시계 = 철사 + 고장난 시계
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"스포츠 시계")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"철사")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"고장난 시계")->GetItemID()))
    );
    //=========================================스포츠 시계=========================================//

    //=========================================타키온 브레이스=========================================//
    // 전자 부품 = 피아노선 + 배터리
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"전자 부품")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"피아노선")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"배터리")->GetItemID()))
    );
    // 모터 = 전자 부품 + 고철
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"모터")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"전자 부품")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"고철")->GetItemID()))
    );
    // 타키온 브레이스 = 모터 + 운동화
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"타키온 브레이스")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"모터")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"운동화")->GetItemID()))
    );
    //=========================================타키온 브레이스=========================================//

    //=========================================운명의 수레바퀴=========================================//
    // 얼음구슬 = 유리구슬 + 얼음
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"얼음구슬")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"유리구슬")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"얼음")->GetItemID()))
    );
    // 이성의 칼 = 얼음구슬 + 못
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"이성의 칼")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"얼음구슬")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"못")->GetItemID()))
    );
    // 운명의 수레바퀴 = 이성의 칼 + 종이
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"운명의 수레바퀴")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"이성의 칼")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"종이")->GetItemID()))
    );
    //=========================================운명의 수레바퀴=========================================//


    //=========================================어사의=========================================//
    // 덧댄 로브 = 셔츠 + 붕대
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"덧댄 로브")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"셔츠")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"붕대")->GetItemID()))
    );
    // 한복 = 덧댄 로브 + 꽃
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"한복")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"덧댄 로브")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"꽃")->GetItemID()))
    );
    // 어사의 = 한복 + 마패
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"어사의")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"한복")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"마패")->GetItemID()))
    );
    //=========================================어사의=========================================//


    //=========================================나이팅게일=========================================//
    // 운명의 꽃 = 꽃 + 원석
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"운명의 꽃")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"꽃")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"원석")->GetItemID()))
    );
    // 아이테르 깃털 = 운명의 꽃 + 깃털
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"아이테르 깃털")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"운명의 꽃")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"깃털")->GetItemID()))
    );
    // 나이팅게일 = 붕대 + 아이테르 깃털
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"나이팅게일")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"붕대")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"아이테르 깃털")->GetItemID()))
    );
    //=========================================나이팅게일=========================================//


    //=========================================델타 레드=========================================//
    // 힐리스 = 운동화 + 쇠구슬
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"힐리스")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"운동화")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"쇠구슬")->GetItemID()))
    );
    // 루비 = 망치 + 원석
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"루비")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"망치")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"원석")->GetItemID()))
    );
    // 델타 레드 = 루비 + 힐리스
    RegisterRecipe(
        make_shared<Recipe>(itemManger->GetItem(L"델타 레드")->GetItemID(),
            RecipeIngredient(itemManger->GetItem(L"루비")->GetItemID()), RecipeIngredient(itemManger->GetItem(L"힐리스")->GetItemID()))
    );
    //=========================================델타 레드=========================================//

    wcout << L"기본 레시피 " << m_recipes.size() << L"개가 등록되었습니다." << endl;
}
