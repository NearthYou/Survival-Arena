#pragma once

#include "Wolf.h"
#include "Player.h"
#include "PlayerStateMachine.h"
#include "AnimationStateMachine.h"
#include "ModelAnimator.h"
#include "InventoryManager.h"
#include "ItemManager.h"
#include "ItemSlot.h"
#include "Recipe.h"
#include "ReferenceFrameCapture.hpp"
#include <fstream>

// Private integration test, not desktop input. Seed real catalog ingredients,
// repeat the original craft/equip flow and observe animation timing and recovery.
inline void ReferenceCraftProbe(const std::shared_ptr<Wolf>& wolf)
{
    static std::weak_ptr<Wolf> subject;
    if (subject.expired()) subject = wolf;
    if (subject.lock() != wolf) return;
    static std::ofstream trace("craft-probe.csv");
    static float elapsed = 0.0F;
    static float started = 0.0F;
    static float completedAt = 0.0F, idleTime = 0.0F;
    static float firstCraftSeconds = 0.0F, secondCraftSeconds = 0.0F;
    static int phase = 0;
    static int crafted = 0, idleCycles = 0;
    static int resultId = -1;
    static bool sawCraft = false;
    static bool sawAnimation = false;
    static bool captured = false;
    static bool finished = false;
    static bool protectedDuration = false;
    if (finished) return;
    elapsed += DT;
    const auto inventory = InventoryManager::GetInstance();
    const auto player = inventory->GetPlayer();
    const auto finish = [&](bool passed, const char* reason) {
        finished = true;
        const auto playerName = player ? player->GetName() : std::wstring{};
        std::ofstream result("craft-probe.json");
        result << "{\"passed\":" << (passed ? "true" : "false")
               << ",\"reason\":\"" << reason << "\",\"elapsed\":" << elapsed
               << ",\"first_craft_seconds\":" << firstCraftSeconds
               << ",\"second_craft_seconds\":" << secondCraftSeconds
               << ",\"craft_count\":" << crafted << ",\"idle_cycles\":" << idleCycles
               << ",\"protected_duration\":" << (protectedDuration ? "true" : "false")
               << ",\"saw_craft_state\":" << (sawCraft ? "true" : "false")
               << ",\"saw_craft_animation\":" << (sawAnimation ? "true" : "false")
               << ",\"player_name\":\"" << std::string(playerName.begin(), playerName.end()) << "\""
               << ",\"player_hp\":" << (player ? player->GetStatus().hp : -1)
               << ",\"result_item_id\":" << resultId << "}";
        result.flush();
        trace.flush();
        ReferenceQueueFrame(passed ? L"oracle-craft-final.bmp" : L"oracle-craft-failed.bmp");
        PostQuitMessage(passed ? 0 : 1);
    };
    if (elapsed > 20.0F) { finish(false, "craft flow timed out"); return; }
    if (!player || inventory->GetInventorySlots().size() != 10 || elapsed < 2.0F) return;
    if (player->GetStatus().hp <= 0) { finish(false, "player died during craft verification"); return; }
    const auto states = player->GetPlayerStateMachine();
    const auto items = ItemManager::GetInstance();
    const auto shoes = items->GetItem(L"\uC6B4\uB3D9\uD654");
    const auto steelBall = items->GetItem(L"\uC1E0\uAD6C\uC2AC");
    if (!shoes || !steelBall) { finish(false, "source recipe ingredients missing"); return; }
    if (phase == 0)
    {
        if (crafted == 0) trace << "seconds,phase,state,animation,item_count\n";
        if (!inventory->PushItem(shoes)) { finish(false, "could not seed first ingredient"); return; }
        bool possible = false;
        states->OnTryCraft(possible);
        if (possible || !inventory->GetAvailableRecipes().empty())
        { finish(false, "one ingredient incorrectly permits crafting"); return; }
        if (!inventory->PushItem(steelBall)) { finish(false, "could not seed second ingredient"); return; }
        const auto recipes = inventory->GetAvailableRecipes();
        if (recipes.size() != 1) { finish(false, "expected one source recipe"); return; }
        resultId = recipes.front()->GetResultItemID();
        if (resultId != 204204) { finish(false, "source ingredients resolved to the wrong result"); return; }
        states->OnTryCraft(possible);
        if (!possible) { finish(false, "source craft delegate rejected valid recipe"); return; }
        states->RequestStateChange(PlayerStateType::Craft);
        player->GetAnimationStateMachine()->RequestStateChange(AnimationStateType::Craft);
        started = elapsed;
        sawCraft = false;
        sawAnimation = false;
        captured = false;
        protectedDuration = false;
        phase = 1;
    }
    sawCraft = sawCraft || states->GetCurrentState() == PlayerStateType::Craft;
    const auto animation = player->GetModelAnimator()->GetCurrentAnimationTag();
    sawAnimation = sawAnimation || animation == L"Craft";
    if (phase == 1 && elapsed - started > 0.5F && elapsed - started < 2.8F)
    {
        const auto animations = player->GetAnimationStateMachine();
        if (states->GetCurrentState() != PlayerStateType::Craft || !animations->IsInState(AnimationStateType::Craft))
        { finish(false, "craft state ended before its source recipe duration"); return; }
        if (animations->GetState(AnimationStateType::Craft)->CanTransitionTo(AnimationStateType::Wait))
        { finish(false, "craft animation permits completion before the prepared duration"); return; }
        protectedDuration = true;
    }
    int ingredientCount = 0;
    int resultCount = 0;
    int occupied = 0;
    int resultSlot = -1;
    const auto& slots = inventory->GetInventorySlots();
    for (int index = 0; index < static_cast<int>(slots.size()); ++index)
        if (const auto item = slots[index]->GetItem())
        {
            ++occupied;
            if (item->GetItemID() == shoes->GetItemID() || item->GetItemID() == steelBall->GetItemID()) ++ingredientCount;
            if (item->GetItemID() == resultId) { ++resultCount; resultSlot = index; }
        }
    trace << elapsed << ',' << phase << ',' << static_cast<int>(states->GetCurrentState())
          << ',' << std::string(animation.begin(), animation.end()) << ',' << occupied << '\n';
    if (!captured && elapsed - started >= 0.5F)
    {
        captured = true;
        ReferenceQueueFrame(crafted == 0 ? L"oracle-craft-active.bmp" : L"oracle-craft-repeat-active.bmp");
    }
    if (phase == 1 && resultCount == 1 && states->GetCurrentState() == PlayerStateType::Wait)
    {
        if (!sawCraft || !sawAnimation || !protectedDuration || ingredientCount != 0 || occupied != 1)
        { finish(false, "craft did not consume two ingredients into one result"); return; }
        const float craftSeconds = elapsed - started;
        if (craftSeconds < 3.0F || craftSeconds > 3.2F)
        { finish(false, "source three-second recipe timing changed"); return; }
        if (crafted == 0) firstCraftSeconds = craftSeconds;
        else secondCraftSeconds = craftSeconds;
        ++crafted;
        inventory->OnInventorySlotClicked(resultSlot);
        if (crafted == 1 && !slots[resultSlot]->IsEmpty())
        { finish(false, "crafted equipment did not equip"); return; }
        if (crafted == 2 && (slots[resultSlot]->IsEmpty() || slots[resultSlot]->GetItem()->GetItemID() != resultId))
        { finish(false, "replacing crafted equipment did not return the old item"); return; }
        completedAt = elapsed;
        idleTime = 0.0F;
        phase = 2;
    }
    else if (phase == 2)
    {
        if (occupied != crafted - 1) { finish(false, "equipping lost or duplicated a crafted item"); return; }
        if (states->GetCurrentState() == PlayerStateType::Wait && animation == L"Wait" &&
            player->GetAnimationStateMachine()->IsInState(AnimationStateType::Wait))
            idleTime += DT;
        else idleTime = 0.0F;
        if (elapsed - completedAt > 3.0F)
        { finish(false, "craft animation did not settle back to idle"); return; }
        if (idleTime >= 0.5F)
        {
            ++idleCycles;
            if (crafted == 1)
            {
                ReferenceQueueFrame(L"oracle-craft-first-idle.bmp");
                phase = 0;
            }
            else finish(true, "two prepared crafts retained source timing, returned to idle and equipped without item loss");
        }
    }
}
