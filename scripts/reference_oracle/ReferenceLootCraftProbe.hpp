#pragma once

#include "Wolf.h"
#include "Player.h"
#include "PlayerStateMachine.h"
#include "AnimationStateMachine.h"
#include "ModelAnimator.h"
#include "NavMesh.h"
#include "NavMeshAgent.h"
#include "Camera.h"
#include "Viewport.h"
#include "InventoryManager.h"
#include "ItemBox.h"
#include "ItemSlot.h"
#include "UIPanel.h"
#include "Button.h"
#include "SphereCollider.h"
#include "ReferenceProbeInput.hpp"
#include "ReferenceFrameCapture.hpp"
#include <cfloat>
#include <cmath>
#include <fstream>

// Input-only observer: no item/stat grants, actor setters or direct craft/loot
// callbacks. World/UI clicks and Z go through the original input consumers.
inline void ReferenceLootCraftProbe(const std::shared_ptr<Wolf>& wolf)
{
    static std::weak_ptr<Wolf> observer;
    if (observer.expired()) observer = wolf;
    if (observer.lock() != wolf) return;
    static std::ofstream trace("loot-craft.csv");
    static std::shared_ptr<Wolf> target;
    static std::shared_ptr<NavMesh> nav;
    static std::shared_ptr<Button> clickedButton;
    static float elapsed = 0, phaseAt = 0, commandAt = -1, idleTime = 0;
    static int phase = 0, kills = 0, wantedId = 0, boxIndex = -1, frames = 0;
    static bool initialized = false, finished = false, shoesTaken = false, steelTaken = false;
    static bool craftSeen = false, craftAnimation = false, equipped = false;
    static Vec3 groundPoint{};
    auto& input = ReferenceProbeInput();
    input.keys.fill(false);
    if (finished) return;
    elapsed += DT;
    const auto inventory = InventoryManager::GetInstance();
    const auto player = inventory->GetPlayer();
    const auto finish = [&](bool passed, const char* reason) {
        finished = true;
        const auto name = player ? player->GetName() : std::wstring{};
        std::ofstream output("loot-craft.json");
        output << "{\"passed\":" << (passed ? "true" : "false") << ",\"reason\":\"" << reason
            << "\",\"seconds\":" << elapsed << ",\"phase\":" << phase << ",\"kills\":" << kills
            << ",\"player_name\":\"" << std::string(name.begin(), name.end()) << "\",\"player_hp\":"
            << (player ? player->GetStatus().hp : -1) << ",\"shoes_looted\":" << shoesTaken
            << ",\"steel_looted\":" << steelTaken << ",\"craft_state\":" << craftSeen
            << ",\"craft_animation\":" << craftAnimation << ",\"equipped\":" << equipped
            << ",\"idle_seconds\":" << idleTime << "}\n";
        output.close(); trace.flush();
        ReferenceQueueFrame(passed ? L"oracle-loot-craft-final.bmp" : L"oracle-loot-craft-failed.bmp");
        PostQuitMessage(passed ? 0 : 1);
    };
    if (elapsed > 150) { finish(false, "natural loot/craft game-time deadline"); return; }
    if (!player || elapsed < 2 || inventory->GetInventorySlots().size() != 10) return;
    if (player->GetStatus().hp <= 0) { finish(false, "player died during natural loot/craft"); return; }
    const auto states = player->GetPlayerStateMachine();
    const auto state = states->GetCurrentState();
    const auto position = player->GetTransform()->GetPosition();
    const auto animation = player->GetModelAnimator()->GetCurrentAnimationTag();
    if (INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
    {
        std::ofstream picks("loot-picking.csv", std::ios::app);
        const auto mouse = INPUT->GetMousePos();
        picks << "click," << elapsed << ',' << phase << ',' << mouse.x << ',' << mouse.y
            << ",target_type," << (target ? static_cast<int>(target->GetType()) : -1)
            << ",target_hp," << (target ? target->GetMonsterStatus().hp : -1) << '\n';
        for (const auto& object : CURSCENE->GetUIObjects())
        {
            if (!object->GetActive()) continue;
            const auto ui = object->GetUIPanel();
            const auto button = object->GetButton();
            if ((ui && ui->Picked(mouse)) || (button && button->Picked(mouse)))
            {
                const auto name = object->GetName();
                picks << "ui_hit," << std::string(name.begin(), name.end()) << ",visible,"
                    << (ui ? ui->IsVisible() : true) << ",type," << static_cast<int>(object->GetType()) << '\n';
                break;
            }
        }
    }
    const auto flatDistance = [](Vec3 a, Vec3 b) { a.y = b.y = 0; return Vec3::Distance(a, b); };
    const auto advance = [&](int next) { phase = next; phaseAt = elapsed; commandAt = -1; clickedButton.reset(); };
    const auto panel = [&](const wchar_t* name) -> std::shared_ptr<GameObject> {
        for (const auto& object : CURSCENE->GetUIObjects())
            if (object->GetName() == name) return object;
        return nullptr;
    };
    const auto slotButton = [&](const wchar_t* name, int index) -> std::shared_ptr<Button> {
        const auto root = panel(name);
        if (!root || !root->GetUIPanel()) return nullptr;
        // The original corpse panel repeats some child names; its slot vector
        // and child creation order still agree. Do not use the name map here.
        int ordinal = 0;
        for (const auto& child : root->GetUIPanel()->GetChildElements())
            if (const auto object = child.lock())
                if (object->GetUIPanel() && object->GetName().find(L"SlotPanel") == 0)
                {
                    if (ordinal++ == index) return object->GetUIPanel()->GetButton(L"Button");
                }
        return nullptr;
    };
    const auto aim = [&](const Vec3& point) {
        const auto camera = CURSCENE->GetMainCamera()->GetCamera();
        auto viewport = GRAPHICS->GetViewport();
        const auto screen = viewport.Project(point, Matrix::Identity, camera->GetViewMatrix(), camera->GetProjectionMatrix());
        if (!std::isfinite(screen.x) || !std::isfinite(screen.y) || screen.z < 0 || screen.z > 1 ||
            screen.x < 20 || screen.x > viewport.GetWidth() - 20 || screen.y < 20 || screen.y > viewport.GetHeight() * 0.72F) return false;
        input.mouse = POINT{static_cast<LONG>(screen.x), static_cast<LONG>(screen.y)};
        return true;
    };
    const auto aimBody = [&]() {
        return aim(target->GetTransform()->GetPosition() + Vec3(0, 1, 0)) ||
            aim(target->GetTransform()->GetPosition() + Vec3(0, 2, 0));
    };
    const auto aimCorpse = [&]() {
        const auto collider = std::dynamic_pointer_cast<SphereCollider>(target->GetCollider());
        if (!collider) return false;
        const auto sphere = collider->GetBoundSphere();
        const auto camera = CURSCENE->GetMainCamera()->GetCamera();
        for (const auto offset : {Vec3(0, 0, 0), Vec3(0.65F, 0, 0), Vec3(-0.65F, 0, 0),
             Vec3(0, -0.65F, 0), Vec3(0, 0.65F, 0), Vec3(0, 0, 0.65F), Vec3(0, 0, -0.65F)})
        {
            if (!aim(Vec3(sphere.Center) + offset * sphere.Radius)) continue;
            bool covered = false;
            for (const auto& object : CURSCENE->GetUIObjects())
                if (object->GetActive() &&
                    ((object->GetUIPanel() && object->GetUIPanel()->Picked(input.mouse)) ||
                     (object->GetButton() && object->GetButton()->Picked(input.mouse))))
                { covered = true; break; }
            if (covered) continue;
            auto ray = CURSCENE->GetObjectManager()->CreateRayFromScreen(Vec2(input.mouse.x, input.mouse.y), camera);
            float closest = FLT_MAX;
            std::shared_ptr<GameObject> hit;
            for (const auto& candidate : CURSCENE->GetQuadTree()->Query(ray, camera))
            {
                if (!candidate->GetCollider() || camera->IsCulled(candidate->GetLayerIndex())) continue;
                float distance = 0;
                if (candidate->GetCollider()->Intersects(ray, distance) && distance < closest)
                { closest = distance; hit = candidate; }
            }
            if (hit == target) return true;
        }
        return false;
    };
    const auto aimButton = [&]() {
        if (!clickedButton) return false;
        const auto rect = clickedButton->GetRect();
        if (rect.right <= rect.left || rect.bottom <= rect.top) return false;
        input.mouse = POINT{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
        return clickedButton->Picked(input.mouse);
    };
    const auto countItem = [&](int id) {
        int count = 0;
        for (const auto& slot : inventory->GetInventorySlots())
            if (slot->GetItem() && slot->GetItem()->GetItemID() == id) ++count;
        return count;
    };
    int occupied = 0;
    for (const auto& slot : inventory->GetInventorySlots()) if (!slot->IsEmpty()) ++occupied;
    if (!initialized)
    {
        if (occupied != 0) { finish(false, "natural route must start with an empty inventory"); return; }
        for (const auto& object : CURSCENE->GetObjects())
            if (auto mesh = object->GetFixedComponent<NavMesh>(ComponentType::NavMesh)) nav = mesh;
        if (!nav) { finish(false, "source navigation is unavailable"); return; }
        initialized = true;
        trace << "seconds,phase,kills,state,animation,hp,x,z,inventory,wanted,mouse_x,mouse_y\n";
    }
    float age = elapsed - phaseAt;
    if (phase == 0)
    {
        if (shoesTaken && steelTaken) { advance(7); }
        else
        {
            target.reset(); float nearest = FLT_MAX;
            for (const auto& object : CURSCENE->GetObjects())
                if (auto candidate = std::dynamic_pointer_cast<Wolf>(object))
                    if (!candidate->IsDead() && candidate->GetMonsterStatus().hp > 0 &&
                        flatDistance(position, candidate->GetTransform()->GetPosition()) < nearest)
                    { target = candidate; nearest = flatDistance(position, candidate->GetTransform()->GetPosition()); }
            if (!target) { finish(false, "source wolves did not provide both ingredients"); return; }
            advance(1);
        }
    }
    else if (phase == 1)
    {
        if (flatDistance(position, target->GetTransform()->GetPosition()) < 7 && aimBody())
        { advance(2); }
        else if (elapsed - commandAt >= 0.6F)
        {
            std::vector<Vec3> path;
            nav->FindPath(position, nav->GetNearestPointOnNavMesh(target->GetTransform()->GetPosition()), path);
            if (path.empty()) { finish(false, "no route to source wolf"); return; }
            Vec3 destination = path.back();
            for (const auto& point : path)
                if (flatDistance(position, point) > 0.7F) { destination = point; break; }
            Vec3 direction = destination - position;
            if (direction.Length() > 6) { direction.Normalize(); direction *= 6; }
            if (!aim(nav->GetNearestPointOnNavMesh(position + direction))) { finish(false, "route point outside viewport"); return; }
            input.keys[VK_RBUTTON] = true; commandAt = elapsed;
        }
        if (age > 50) { finish(false, "could not approach source wolf"); return; }
    }
    else if (phase == 2)
    {
        if (age < 0.06F)
        {
            if (!aimBody()) { finish(false, "attack target outside viewport"); return; }
            input.keys[VK_RBUTTON] = true;
        }
        if (target->IsDead() && target->GetMonsterStatus().hp <= 0) { ++kills; advance(3); }
        else if (age > 22) { finish(false, "ordinary attack did not kill the wolf"); return; }
    }
    else if (phase == 3 && age >= 1)
    {
        wantedId = 0; boxIndex = -1;
        const auto box = target->GetItemBox();
        for (int index = 0; index < static_cast<int>(box->GetBoxInventory().size()); ++index)
            if (const auto item = box->GetBoxInventory()[index])
                if ((!shoesTaken && item->GetItemID() == 204102) || (!steelTaken && item->GetItemID() == 112103))
                { wantedId = item->GetItemID(); boxIndex = index; break; }
        if (boxIndex < 0) advance(0);
        else advance(4);
    }
    else if (phase == 4)
    {
        if (!aimCorpse()) { finish(false, "no exposed corpse point outside UI"); return; }
        if (age < 0.04F) input.keys[VK_LBUTTON] = true;
        const auto box = panel(L"ItemBoxPanel");
        if (age > 0.1F && box && box->GetActive())
        {
            advance(5);
            clickedButton = slotButton(L"ItemBoxPanel", boxIndex);
            if (!clickedButton) { finish(false, "source corpse item button not found"); return; }
            ReferenceQueueFrame(wantedId == 204102 ? L"oracle-shoes-corpse.bmp" : L"oracle-steel-corpse.bmp");
        }
        else if (age > 3) { finish(false, "left click did not open source corpse"); return; }
    }
    else if (phase == 5)
    {
        if (age < 0.1F)
        {
            if (!aimButton()) { finish(false, "corpse item button has invalid bounds"); return; }
            if (age < 0.04F) input.keys[VK_LBUTTON] = true;
        }
        if (countItem(wantedId) == 1)
        {
            if (target->GetItemBox()->GetBoxInventory()[boxIndex]) { finish(false, "loot duplicated the corpse item"); return; }
            if (wantedId == 204102) shoesTaken = true; else steelTaken = true;
            ReferenceQueueFrame(wantedId == 204102 ? L"oracle-shoes-looted.bmp" : L"oracle-steel-looted.bmp");
            advance(6);
        }
        else if (age > 3) { finish(false, "source item click did not transfer loot"); return; }
    }
    else if (phase == 6)
    {
        if (!aimCorpse()) { finish(false, "no exposed corpse point for closing its panel"); return; }
        if (age < 0.04F) input.keys[VK_LBUTTON] = true;
        const auto box = panel(L"ItemBoxPanel");
        if (age > 0.1F && box && !box->GetActive()) advance(0);
        else if (age > 3) { finish(false, "source corpse panel did not close"); return; }
    }
    else if (phase == 7)
    {
        if (commandAt < 0)
        {
            groundPoint = nav->GetNearestPointOnNavMesh(position + Vec3(-3, 0, -3));
            if (flatDistance(groundPoint, position) < 1 || !aim(groundPoint)) { finish(false, "no idle movement point"); return; }
            input.keys[VK_RBUTTON] = true; commandAt = elapsed;
        }
        if (age > 0.1F && (state == PlayerStateType::Wait || state == PlayerStateType::Run)) advance(8);
        else if (age > 4) { finish(false, "could not leave old attack before crafting"); return; }
    }
    else if (phase == 8)
    {
        if (countItem(204102) != 1 || countItem(112103) != 1 || occupied != 2)
        { finish(false, "natural ingredients were not retained before Z"); return; }
        if (age < 0.04F) input.keys['Z'] = true;
        if (age > 0.08F) advance(9);
    }
    else if (phase == 9)
    {
        if (state == PlayerStateType::Craft) craftSeen = true;
        craftAnimation = craftAnimation || animation == L"Craft";
        if (countItem(204204) == 1 && state == PlayerStateType::Wait)
        {
            if (!craftSeen || !craftAnimation || occupied != 1 || countItem(204102) || countItem(112103))
            { finish(false, "Z craft did not consume the looted ingredients"); return; }
            int resultSlot = -1;
            const auto& slots = inventory->GetInventorySlots();
            for (int index = 0; index < static_cast<int>(slots.size()); ++index)
                if (slots[index]->GetItem() && slots[index]->GetItem()->GetItemID() == 204204) resultSlot = index;
            ReferenceQueueFrame(L"oracle-natural-crafted.bmp");
            advance(10); clickedButton = slotButton(L"CharMainPanel", resultSlot);
            if (!clickedButton) { finish(false, "crafted inventory button not found"); return; }
        }
        else if (age > 7) { finish(false, "Z did not complete source crafting"); return; }
    }
    else if (phase == 10)
    {
        if (age < 0.1F)
        {
            if (!aimButton()) { finish(false, "inventory button has invalid bounds"); return; }
            if (age < 0.04F) input.keys[VK_LBUTTON] = true;
        }
        if (occupied == 0 && slotButton(L"CharEquipPanel", 4)) { equipped = true; advance(11); }
        else if (age > 3) { finish(false, "crafted item did not equip through its UI button"); return; }
    }
    else if (phase == 11)
    {
        if (state == PlayerStateType::Wait && animation == L"Wait") idleTime += DT;
        else idleTime = 0;
        if (idleTime >= 0.5F) { finish(true, "source wolf loot, real UI pickup, Z crafting and UI equip completed without grants"); return; }
        if (age > 4) { finish(false, "natural craft flow did not return to idle"); return; }
    }
    trace << elapsed << ',' << phase << ',' << kills << ',' << static_cast<int>(state) << ','
        << std::string(animation.begin(), animation.end()) << ',' << player->GetStatus().hp << ','
        << position.x << ',' << position.z << ',' << occupied << ',' << wantedId << ',' << input.mouse.x << ',' << input.mouse.y << '\n';
    if (++frames % 25 == 0) trace.flush();
}
