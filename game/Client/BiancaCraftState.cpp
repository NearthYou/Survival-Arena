#include "pch.h"
#include "BiancaCraftState.h"
#include "ModelAnimator.h"

#include "Item.h"
#include "Recipe.h"
#include "InventoryManager.h"
#include "ItemManager.h"


BiancaCraftState::BiancaCraftState(shared_ptr<ModelAnimator> modelAnimator)
    :Super(PlayerStateType::Craft)
    , m_modelAnimator(modelAnimator)
{

}

BiancaCraftState::~BiancaCraftState()
{

}

void BiancaCraftState::Enter()
{
    m_skillTime = 0.0f;
    m_isAnimationStarted = true;
    m_isSkillComplete = false;

    auto inventoryManager = InventoryManager::GetInstance();
    auto& inventorySlots = inventoryManager->GetInventorySlots();
    vector<shared_ptr<Recipe>> craftableRecipes = InventoryManager::GetInstance()->GetAvailableRecipes();
    // 결과 아이템의 등급 확인
    auto resultItem = ItemManager::GetInstance()->GetItem(craftableRecipes[m_recipeIndex]->GetResultItemID());
    SetCraftTimeByGrade(resultItem->GetItemGrade());


    SOUND->PlaySound(L"SFX/character_Craft_Tool_8sec.wav", 3, 0.5f);
    cout << "BiancaCraftState진입\n";
}

void BiancaCraftState::Update()
{
    UpdateNormalSkill();
}

void BiancaCraftState::Exit()
{
    // 상태 종료 시 정리
    m_skillTime = 0.0f;
    m_isAnimationStarted = false;
    m_isSkillComplete = false;
    SOUND->StopSound(3);
    cout << "BiancaCraftState종료\n";
}

bool BiancaCraftState::CanTransitionTo(PlayerStateType newState)
{
    // 스킬이 완료되었을 때만 Wait 상태로 전환 가능
    if (m_isSkillComplete && newState == PlayerStateType::Wait)
    {
        return true;
    }
    return false;
}

void BiancaCraftState::UpdateNormalSkill()
{
    m_skillTime += DT;

    if (m_isSkillComplete)
        return;

    // 제작 시간이 완료되면 실제 아이템 조합 실행
    if (m_skillTime >= m_craftingTime)
    {
        auto inventoryManager = InventoryManager::GetInstance();
        auto& inventorySlots = inventoryManager->GetInventorySlots();
        vector<shared_ptr<Recipe>> craftableRecipes = InventoryManager::GetInstance()->GetAvailableRecipes();

        if (craftableRecipes.size() > m_recipeIndex &&
            craftableRecipes[m_recipeIndex]->ExecuteCraftFromSlots(inventorySlots))
        {
            cout << "제작 완료!" << endl;
        }

        m_isSkillComplete = true; // Engine에서 이 상태를 확인할 수 있도록

        // 더 이상 여기서 상태 전환하지 않음 (Engine에서 처리)
        cout << "제작 완료 - Engine에서 상태 전환 처리 예정" << endl;
    }
}

void BiancaCraftState::SetCraftTimeByGrade(ITEMGRADE grade)
{
    m_craftingItemGrade = grade;
    switch (grade) {
    case ITEMGRADE::COMMON:    m_craftingTime = 1.0f; break;
    case ITEMGRADE::UNCOMMON:  m_craftingTime = 3.0f; break;
    case ITEMGRADE::RARE:      m_craftingTime = 5.0f; break;
    case ITEMGRADE::EPIC:      m_craftingTime = 7.0f; break;
    case ITEMGRADE::LEGENDARY: m_craftingTime = 9.0f; break;
    default:                   m_craftingTime = 11.0f; break;
    }
}
