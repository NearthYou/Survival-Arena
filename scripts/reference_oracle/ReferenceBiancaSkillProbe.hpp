#pragma once

#include "Wolf.h"
#include "Player.h"
#include "PlayerStateMachine.h"
#include "ModelAnimator.h"
#include "Camera.h"
#include "Viewport.h"
#include "InventoryManager.h"
#include "ReferenceProbeInput.hpp"
#include "ReferenceFrameCapture.hpp"
#include <cfloat>
#include <cmath>
#include <fstream>

inline void ReferenceBiancaSkillProbe(const std::shared_ptr<Wolf>& wolf)
{
    static std::weak_ptr<Wolf> observer;
    if (observer.expired()) observer = wolf;
    if (observer.lock() != wolf) return;
    static std::ofstream trace("bianca-skills.csv");
    static std::shared_ptr<Wolf> target;
    static float elapsed = 0, stageAt = 0, eAt = 0, defense = 0, innerScale = 0;
    static int stage = 0, hpBefore = 200, innerHp = 200, frames = 0;
    static Vec3 eStart{};
    static bool finished = false, killed = false, leveled = false;
    static bool qState = false, qProjectile = false, qAnimation = false, qHit = false;
    static bool wState = false, coffinSeen = false, wDefense = false, wRestored = false, wAnimation = false;
    static bool eStarted = false, eState = false, eCharged = false, eReleased = false, eMoved = false, eHit = false, eAnimation = false;
    static bool rState = false, rAnimation = false, outerSeen = false, drainSeen = false;
    static bool innerSeen = false, innerGrew = false, outerHit = false, innerHit = false;
    auto& input = ReferenceProbeInput();
    input.keys.fill(false);
    if (finished) return;
    elapsed += DT;
    const auto player = InventoryManager::GetInstance()->GetPlayer();
    const auto finish = [&](bool passed, const char* reason) {
        finished = true;
        std::ofstream output("bianca-skills.json");
        output << "{\"passed\":" << (passed ? "true" : "false") << ",\"reason\":\"" << reason
               << "\",\"seconds\":" << elapsed << ",\"stage\":" << stage
               << ",\"q_state\":" << qState << ",\"q_projectile\":" << qProjectile << ",\"q_animation\":" << qAnimation << ",\"q_hit\":" << qHit
               << ",\"wolf_killed\":" << killed << ",\"earned_level\":" << leveled
               << ",\"w_state\":" << wState << ",\"w_coffin\":" << coffinSeen << ",\"w_defense\":" << wDefense << ",\"w_restored\":" << wRestored << ",\"w_animation\":" << wAnimation
               << ",\"e_state\":" << eState << ",\"e_charged\":" << eCharged << ",\"e_released\":" << eReleased << ",\"e_moved\":" << eMoved << ",\"e_hit\":" << eHit << ",\"e_animation\":" << eAnimation
               << ",\"r_state\":" << rState << ",\"r_animation\":" << rAnimation << ",\"r_outer\":" << outerSeen << ",\"r_drain\":" << drainSeen
               << ",\"r_inner\":" << innerSeen << ",\"r_growth\":" << innerGrew << ",\"r_outer_hit\":" << outerHit << ",\"r_inner_hit\":" << innerHit
               << ",\"player_hp\":" << (player ? player->GetStatus().hp : -1) << "}";
        output.flush(); trace.flush();
        ReferenceQueueFrame(passed ? L"oracle-bianca-skills-final.bmp" : L"oracle-bianca-skills-failed.bmp");
        PostQuitMessage(passed ? 0 : 1);
    };
    if (elapsed > 80) { finish(false, "Bianca skill game-time deadline"); return; }
    if (!player || elapsed < 2 || InventoryManager::GetInstance()->GetInventorySlots().size() != 10) return;
    if (player->GetName() != L"Bianca" || player->GetStatus().hp <= 0) { finish(false, "wrong or dead player"); return; }
    const auto states = player->GetPlayerStateMachine();
    const auto state = states->GetCurrentState();
    const auto animation = player->GetModelAnimator()->GetCurrentAnimationTag();
    const auto findObject = [&](const wchar_t* name) -> std::shared_ptr<GameObject> {
        for (const auto& object : CURSCENE->GetObjects()) if (object->GetName() == name) return object;
        return nullptr;
    };
    const auto nearest = [&]() {
        std::shared_ptr<Wolf> found;
        float closest = FLT_MAX;
        for (const auto& object : CURSCENE->GetObjects())
            if (const auto candidate = std::dynamic_pointer_cast<Wolf>(object))
                if (!candidate->IsDead() && candidate->GetMonsterStatus().hp > 0)
                {
                    const auto distance = Vec3::Distance(player->GetTransform()->GetPosition(), candidate->GetTransform()->GetPosition());
                    if (distance < closest) { closest = distance; found = candidate; }
                }
        return found;
    };
    const auto pointAt = [&](const Vec3& position) {
        const auto camera = CURSCENE->GetMainCamera()->GetCamera();
        auto viewport = GRAPHICS->GetViewport();
        const auto screen = viewport.Project(position, Matrix::Identity, camera->GetViewMatrix(), camera->GetProjectionMatrix());
        input.mouse = POINT{static_cast<LONG>(screen.x), static_cast<LONG>(screen.y)};
    };
    const auto advance = [&](int next) { stage = next; stageAt = elapsed; input.keys.fill(false); };
    const auto projectile = findObject(L"Bianca_Q_Projectile");
    // Basic attacks and Q intentionally share this source object name. Observe
    // active instances, not an arbitrary inactive prototype from the set.
    bool projectileActive = false;
    for (const auto& object : CURSCENE->GetObjects())
        projectileActive = projectileActive || (object->GetName() == L"Bianca_Q_Projectile" && object->GetActive());
    const auto cone = findObject(L"Bianca_Q_Cone");
    const auto coffin = findObject(L"Bianca_Coffin");
    const auto outer = findObject(L"Bianca_Outer_Circle");
    const auto inner = findObject(L"Bianca_Inner_Circle");
    const auto drain = findObject(L"Bianca_Gather_Blood");
    if (!projectile || !cone || !coffin || !outer || !inner || !drain) { finish(false, "source skill objects missing"); return; }
    if (stage == 0)
    {
        target = nearest();
        if (!target) { finish(false, "source wolf missing"); return; }
        trace << "seconds,stage,state,animation,player_hp,target_hp,x,z,defense,coffin,outer,inner,inner_scale\n";
        advance(1);
    }
    if (target) pointAt(target->GetTransform()->GetPosition() + Vec3(0, 1, 0));
    const float age = elapsed - stageAt;
    if (stage == 1 || stage == 4 || stage == 6 || stage == 9)
    {
        const int index = stage == 1 ? 0 : stage == 4 ? 1 : stage == 6 ? 2 : 3;
        const int key = stage == 1 ? 'Q' : stage == 4 ? 'W' : stage == 6 ? 'E' : 'R';
        if (age < 0.12F) input.keys[VK_LCONTROL] = true;
        if (age >= 0.04F && age < 0.10F) input.keys[key] = true;
        if (age > 0.16F)
        {
            if (player->GetSkill(index)->GetCurSkillLevel() != 1) { finish(false, "Ctrl skill learning failed"); return; }
            hpBefore = target->GetMonsterStatus().hp;
            if (stage == 4) defense = player->GetStatus().defense;
            advance(stage + 1);
        }
    }
    else if (stage == 2)
    {
        if (age < 0.06F) input.keys['Q'] = true;
        qState = qState || state == PlayerStateType::Skill_1;
        qAnimation = qAnimation || animation == L"Skill_1";
        if (projectileActive && !qProjectile) { qProjectile = true; ReferenceQueueFrame(L"oracle-bianca-q.bmp"); }
        qHit = qHit || target->GetMonsterStatus().hp < hpBefore;
        if (qHit && state == PlayerStateType::Wait && age > 0.2F)
        {
            if (!qState || !qAnimation || !qProjectile) { finish(false, "Q hit without expected animation and projectile"); return; }
            advance(3);
        }
        else if (age > 6) { finish(false, "Q failed to hit the first wolf"); return; }
    }
    else if (stage == 3)
    {
        if (!target->IsDead() && age < 0.06F) input.keys[VK_RBUTTON] = true;
        // Bianca marks the attack complete on a dead target but keeps the
        // BaseAttack state until another command. Check the legal next action.
        if (target->IsDead())
        {
            killed = true; leveled = player->GetStatus().level > 1;
            // Death and reward callbacks may be processed in different updates.
            if (leveled && player->GetStatus().availableSkillPoints >= 3 && states->CanChangeState(PlayerStateType::Skill_2))
            {
                target = nearest();
                if (!target) { finish(false, "second wolf missing"); return; }
                advance(4);
            }
        }
        if (stage == 3 && age > 10) { finish(false, "right-click kill, reward or legal follow-up was not observed"); return; }
    }
    else if (stage == 5)
    {
        if (age < 0.06F) input.keys['W'] = true;
        wState = wState || state == PlayerStateType::Skill_2;
        wAnimation = wAnimation || animation == L"Skill_2";
        if (coffin->GetActive() && !coffinSeen) { coffinSeen = true; ReferenceQueueFrame(L"oracle-bianca-w.bmp"); }
        wDefense = wDefense || (coffin->GetActive() && player->GetStatus().defense >= defense + 49.9F);
        if (coffinSeen && !coffin->GetActive() && state == PlayerStateType::Wait)
        {
            wRestored = std::abs(player->GetStatus().defense - defense) < 0.1F;
            if (!wState || !wAnimation || !wDefense || !wRestored) { finish(false, "W coffin or defense lifecycle failed"); return; }
            advance(6);
        }
        else if (age > 8) { finish(false, "W did not finish"); return; }
    }
    else if (stage == 7)
    {
        if (!eStarted && !projectileActive && !cone->GetActive() && state == PlayerStateType::Wait)
        { eStarted = true; eAt = elapsed; eStart = player->GetTransform()->GetPosition(); hpBefore = target->GetMonsterStatus().hp; }
        if (eStarted)
        {
            const auto eAge = elapsed - eAt;
            if (eAge < 0.8F) input.keys['E'] = true;
            if (state == PlayerStateType::Skill_3 && !eState) { eState = true; ReferenceQueueFrame(L"oracle-bianca-e-charge.bmp"); }
            eCharged = eCharged || (state == PlayerStateType::Skill_3 && INPUT->GetButton(KEY_TYPE::E) && eAge > 0.2F);
            eReleased = eReleased || INPUT->GetButtonUp(KEY_TYPE::E);
            eAnimation = eAnimation || animation.find(L"Skill_3_") == 0;
            if (eReleased && Vec3::Distance(player->GetTransform()->GetPosition(), eStart) > 1 && !eMoved)
            { eMoved = true; ReferenceQueueFrame(L"oracle-bianca-e-rush.bmp"); }
            eHit = eHit || target->GetMonsterStatus().hp < hpBefore;
            if (eReleased && state == PlayerStateType::Wait && eAge > 0.8F)
            {
                if (!eState || !eCharged || !eAnimation || !eMoved || !eHit) { finish(false, "E charge/release/dash/damage failed"); return; }
                advance(8);
            }
        }
        if (age > 15) { finish(false, "E did not complete after previous effects ended"); return; }
    }
    else if (stage == 8)
    {
        if (!target || target->IsDead()) target = nearest();
        if (!target) { finish(false, "no live target for R phases"); return; }
        Vec3 direction = target->GetTransform()->GetPosition() - player->GetTransform()->GetPosition();
        const float distance = direction.Length();
        if (distance <= 6) advance(9);
        else
        {
            direction.Normalize();
            pointAt(player->GetTransform()->GetPosition() + direction * 5.0F);
            if (std::fmod(age, 0.5F) < 0.05F) input.keys[VK_RBUTTON] = true;
        }
        if (age > 25) { finish(false, "could not navigate to a live R target"); return; }
    }
    else if (stage == 10)
    {
        if (age < 0.06F) input.keys['R'] = true;
        rState = rState || state == PlayerStateType::Skill_4;
        rAnimation = rAnimation || animation.find(L"Skill_4_") == 0;
        if (outer->GetActive() && !outerSeen) { outerSeen = true; ReferenceQueueFrame(L"oracle-bianca-r-outer.bmp"); }
        drainSeen = drainSeen || drain->GetActive();
        if (inner->GetActive() && !innerSeen)
        {
            innerSeen = true; innerHp = target->GetMonsterStatus().hp; innerScale = inner->GetTransform()->GetScale().x;
            outerHit = innerHp < hpBefore;
            ReferenceQueueFrame(L"oracle-bianca-r-inner.bmp");
        }
        innerGrew = innerGrew || (innerSeen && inner->GetTransform()->GetScale().x > innerScale + 1.0F);
        if (innerSeen && !inner->GetActive() && !outer->GetActive() && state == PlayerStateType::Wait)
        {
            innerHit = target->GetMonsterStatus().hp < innerHp;
            if (!rState || !rAnimation || !outerSeen || !drainSeen || !outerHit || !innerGrew || !innerHit)
            { finish(false, "R did not complete both damaging phases"); return; }
            finish(true, "earned all skills; Q projectile hit; W defense lifecycle; E charge/dash/hit; both living-health R phases");
            return;
        }
        if (age > 10) { finish(false, "R did not end"); return; }
    }
    const auto position = player->GetTransform()->GetPosition();
    trace << elapsed << ',' << stage << ',' << static_cast<int>(state) << ',' << std::string(animation.begin(), animation.end())
          << ',' << player->GetStatus().hp << ',' << (target ? target->GetMonsterStatus().hp : -1) << ',' << position.x << ',' << position.z
          << ',' << player->GetStatus().defense << ',' << coffin->GetActive() << ',' << outer->GetActive() << ',' << inner->GetActive()
          << ',' << inner->GetTransform()->GetScale().x << '\n';
    if (++frames % 25 == 0) trace.flush();
}
