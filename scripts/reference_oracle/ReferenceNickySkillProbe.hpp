#pragma once

#include "Wolf.h"
#include "Player.h"
#include "BaseSkill.h"
#include "PlayerStateMachine.h"
#include "ModelAnimator.h"
#include "Camera.h"
#include "Viewport.h"
#include "InventoryManager.h"
#include "SceneObjectManager.h"
#include "BaseCollider.h"
#include "ReferenceProbeInput.hpp"
#include "ReferenceFrameCapture.hpp"
#include <cfloat>
#include <fstream>

inline void ReferenceNickySkillProbe(const std::shared_ptr<Wolf>& wolf)
{
    static std::weak_ptr<Wolf> observer;
    if (observer.expired()) observer = wolf;
    if (observer.lock() != wolf) return;
    static std::ofstream trace("nicky-skills.csv");
    static std::shared_ptr<Wolf> target;
    static float elapsed = 0.0F, stageStarted = 0.0F, incomingHitAt = 0.0F;
    static int stage = 0, previousHp = -1, targetHp = 200, frames = 0;
    static Vec3 rushStart{}, stopPoint{}, qStart{}, qMovePoint{}, qAimPoint{}, qReleaseOrigin{};
    static Vec3 maxQStart{}, maxQAim{};
    static float maxQDistance = 0;
    static bool maxQCharged = false, maxQReleased = false, maxQAutoAnimation = false;
    static bool qCooldownWithoutReplay = true, maxQIdle = false;
    static bool rState = false, rMoved = false, rHit = false, guard = false;
    static bool counter = false, counterHit = false, killed = false, leveled = false;
    static bool incoming = false, rushCaptured = false, finished = false;
    static bool eState = false, eAnimation = false, eHit = false, eNonCharging = false;
    static bool qCharging = false, qChargeAnimation = false, qChargeMoved = false, qReleased = false, qDashed = false, qTowardAim = false;
    auto& input = ReferenceProbeInput();
    input.keys.fill(false);
    if (finished) return;
    elapsed += DT;
    const auto player = InventoryManager::GetInstance()->GetPlayer();
    const auto finish = [&](bool passed, const char* reason) {
        finished = true;
        std::ofstream result("nicky-skills.json");
        result << "{\"passed\":" << (passed ? "true" : "false") << ",\"reason\":\"" << reason
               << "\",\"stage\":" << stage << ",\"seconds\":" << elapsed
               << ",\"r_state\":" << rState << ",\"r_moved\":" << rMoved << ",\"r_hit\":" << rHit
               << ",\"wolf_killed\":" << killed << ",\"earned_level\":" << leveled
               << ",\"guard\":" << guard << ",\"counter\":" << counter
               << ",\"counter_hit\":" << counterHit << ",\"incoming_hit\":" << incoming
               << ",\"e_state\":" << eState << ",\"e_animation\":" << eAnimation << ",\"e_hit\":" << eHit
               << ",\"e_non_charging\":" << eNonCharging << ",\"q_charging\":" << qCharging
               << ",\"q_charge_animation\":" << qChargeAnimation << ",\"q_charge_moved\":" << qChargeMoved
               << ",\"q_released\":" << qReleased << ",\"q_dashed\":" << qDashed
               << ",\"q_toward_aim\":" << qTowardAim
               << ",\"q_max_charged\":" << maxQCharged << ",\"q_max_released\":" << maxQReleased
               << ",\"q_max_distance\":" << maxQDistance << ",\"q_max_auto_animation\":" << maxQAutoAnimation
               << ",\"q_cooldown_without_replay\":" << qCooldownWithoutReplay << ",\"q_max_idle\":" << maxQIdle
               << ",\"player_hp\":" << (player ? player->GetStatus().hp : -1) << "}";
        result.flush(); trace.flush();
        ReferenceQueueFrame(passed ? L"oracle-nicky-skills-final.bmp" : L"oracle-nicky-skills-failed.bmp");
        PostQuitMessage(passed ? 0 : 1);
    };
    if (elapsed > 45.0F) { finish(false, "skill flow exceeded game-time deadline"); return; }
    if (!player || elapsed < 2.0F || InventoryManager::GetInstance()->GetInventorySlots().size() != 10) return;
    if (player->GetName() != L"Nicky") { finish(false, "wrong selected character"); return; }
    if (player->GetStatus().hp <= 0) { finish(false, "player died during skill verification"); return; }
    const auto states = player->GetPlayerStateMachine();
    const auto state = states->GetCurrentState();
    const auto animation = player->GetModelAnimator()->GetCurrentAnimationTag();
    const auto nearestWolf = [&]() {
        std::shared_ptr<Wolf> found;
        float nearest = FLT_MAX;
        for (const auto& object : CURSCENE->GetObjects())
            if (const auto candidate = std::dynamic_pointer_cast<Wolf>(object))
                if (!candidate->IsDead() && candidate->GetMonsterStatus().hp > 0)
                {
                    const float distance = Vec3::Distance(player->GetTransform()->GetPosition(), candidate->GetTransform()->GetPosition());
                    if (distance < nearest) { nearest = distance; found = candidate; }
                }
        return found;
    };
    const auto pointAt = [&](const Vec3& point) {
        const auto camera = CURSCENE->GetMainCamera()->GetCamera();
        auto viewport = GRAPHICS->GetViewport();
        const auto screen = viewport.Project(point, Matrix::Identity, camera->GetViewMatrix(), camera->GetProjectionMatrix());
        input.mouse = POINT{static_cast<LONG>(screen.x), static_cast<LONG>(screen.y)};
    };
    const auto level = [&](int index) { return static_cast<BaseSkill*>(player->GetSkill(index))->GetCurSkillLevel(); };
    const auto advance = [&](int next) { stage = next; stageStarted = elapsed; input.keys.fill(false); };
    if (stage == 0)
    {
        target = nearestWolf();
        if (!target) { finish(false, "no live source wolf"); return; }
        trace << "seconds,stage,player_state,animation,player_hp,target_hp,x,z,r_level,r_cooldown,r_down,ctrl_down,ctrl_held,picked_target,direct_ray_hit,mouse_x,mouse_y\n";
        previousHp = player->GetStatus().hp;
        advance(1);
    }
    if (target) pointAt(target->GetTransform()->GetPosition() + Vec3(0, 1, 0));
    const float age = elapsed - stageStarted;
    if (stage == 1 || stage == 4 || stage == 8 || stage == 11)
    {
        const int key = stage == 1 ? 'R' : stage == 4 ? 'W' : stage == 8 ? 'E' : 'Q';
        const int skillIndex = stage == 1 ? 3 : stage == 4 ? 1 : stage == 8 ? 2 : 0;
        if (age < 0.12F) input.keys[VK_LCONTROL] = true;
        if (age >= 0.04F && age < 0.10F) input.keys[key] = true;
        if (age > 0.16F)
        {
            if (level(skillIndex) != 1) { finish(false, "Ctrl skill learning failed"); return; }
            if (stage == 1) { rushStart = player->GetTransform()->GetPosition(); targetHp = target->GetMonsterStatus().hp; advance(2); }
            else if (stage == 4) { targetHp = target->GetMonsterStatus().hp; advance(5); }
            else if (stage == 8) { targetHp = target->GetMonsterStatus().hp; advance(9); }
            else
            {
                qStart = player->GetTransform()->GetPosition();
                qMovePoint = qStart + Vec3(-3, 0, -6);
                qAimPoint = qStart + Vec3(0, 0, -12);
                qReleaseOrigin = qStart;
                advance(12);
            }
        }
    }
    else if (stage == 2)
    {
        if (age < 0.06F) input.keys['R'] = true;
        if (state == PlayerStateType::Skill_4 && !rState) { rState = true; ReferenceQueueFrame(L"oracle-nicky-r-ready.bmp"); }
        rMoved = rMoved || Vec3::Distance(player->GetTransform()->GetPosition(), rushStart) > 1.0F;
        rHit = rHit || target->GetMonsterStatus().hp < targetHp;
        if (rMoved && !rushCaptured) { rushCaptured = true; ReferenceQueueFrame(L"oracle-nicky-r-rush.bmp"); }
        if (rState && state == PlayerStateType::Wait && age > 0.2F)
        {
            if (!rMoved || !rHit) { finish(false, "R ended without rush and target damage"); return; }
            advance(3);
        }
        else if (age > 7.0F) { finish(false, "R input did not complete"); return; }
    }
    else if (stage == 3)
    {
        if (age < 0.06F) input.keys[VK_RBUTTON] = true;
        if (target->IsDead() && state == PlayerStateType::Wait)
        {
            killed = true; leveled = player->GetStatus().level > 1;
            if (!leveled || player->GetStatus().availableSkillPoints < 1) { finish(false, "kill did not earn skill points"); return; }
            target = nearestWolf();
            if (!target) { finish(false, "no second live source wolf"); return; }
            advance(4);
        }
        else if (age > 10.0F) { finish(false, "right-click attack did not finish the first wolf"); return; }
    }
    else if (stage == 5)
    {
        if (age < 0.06F) input.keys[VK_RBUTTON] = true;
        if (target->GetMonsterStatus().hp < targetHp)
        {
            stopPoint = player->GetTransform()->GetPosition() + Vec3(-1, 0, -1);
            advance(6);
        }
        else if (age > 6.0F) { finish(false, "could not provoke the second wolf"); return; }
    }
    else if (stage == 6)
    {
        pointAt(stopPoint);
        if (age < 0.06F) input.keys[VK_RBUTTON] = true;
        if (player->GetStatus().hp < previousHp)
        {
            incoming = true; incomingHitAt = elapsed; targetHp = target->GetMonsterStatus().hp;
            advance(7);
        }
        else if (age > 6.0F) { finish(false, "provoked wolf did not attack the living player"); return; }
    }
    else if (stage == 7)
    {
        const float sinceHit = elapsed - incomingHitAt;
        if (sinceHit >= 1.15F && sinceHit < 1.23F) input.keys['W'] = true;
        if (state == PlayerStateType::Skill_2 && !guard)
        { guard = true; targetHp = target->GetMonsterStatus().hp; ReferenceQueueFrame(L"oracle-nicky-guard.bmp"); }
        if (state == PlayerStateType::Counter && !counter) { counter = true; ReferenceQueueFrame(L"oracle-nicky-counter.bmp"); }
        counterHit = counterHit || (counter && target->GetMonsterStatus().hp < targetHp);
        if (guard && counter && counterHit && state == PlayerStateType::Wait)
        { advance(8); }
        else if (age > 5.0F) { finish(false, "guard did not produce a completed damaging counter"); return; }
    }
    else if (stage == 9)
    {
        if (age < 0.06F) input.keys['E'] = true;
        if (state == PlayerStateType::Skill_3)
        {
            if (!eState) ReferenceQueueFrame(L"oracle-nicky-e.bmp");
            eState = true;
            eNonCharging = !states->GetCurrentStatePtr()->IsCharging();
        }
        eAnimation = eAnimation || animation == L"Skill_03";
        eHit = eHit || target->GetMonsterStatus().hp < targetHp;
        if (eState && state == PlayerStateType::Wait && age > 0.3F)
        {
            if (!eHit || !eNonCharging || !eAnimation) { finish(false, "E did not show its non-charging punch and target damage"); return; }
            advance(10);
        }
        else if (age > 4.0F) { finish(false, "E input did not complete"); return; }
    }
    else if (stage == 10)
    {
        if (!target->IsDead() && age < 0.06F) input.keys[VK_RBUTTON] = true;
        if (target->IsDead() && state == PlayerStateType::Wait) advance(11);
        else if (age > 7.0F) { finish(false, "could not clear the second wolf before Q traversal"); return; }
    }
    else if (stage == 12)
    {
        // The short movement order can finish during charging; keep the release
        // aim farther away so a near-zero aim vector cannot make direction ambiguous.
        pointAt(age >= 0.2F && age < 0.26F ? qMovePoint : qAimPoint);
        if (age < 1.0F)
        {
            input.keys['Q'] = true;
            qReleaseOrigin = player->GetTransform()->GetPosition();
        }
        if (age >= 0.2F && age < 0.26F) input.keys[VK_RBUTTON] = true;
        if (state == PlayerStateType::Skill_1)
        {
            const auto action = states->GetCurrentStatePtr();
            if (action->IsCharging())
            {
                if (!qCharging) ReferenceQueueFrame(L"oracle-nicky-q-charge.bmp");
                qCharging = true;
                if (Vec3::Distance(player->GetTransform()->GetPosition(), qStart) > 0.15F && !qChargeMoved)
                { qChargeMoved = true; ReferenceQueueFrame(L"oracle-nicky-q-charge-run.bmp"); }
            }
            if (action->IsReleasing())
            {
                if (!qReleased) ReferenceQueueFrame(L"oracle-nicky-q-release.bmp");
                qReleased = true;
            }
        }
        qChargeAnimation = qChargeAnimation || animation.find(L"Charge") != std::wstring::npos;
        if (qReleased && Vec3::Distance(player->GetTransform()->GetPosition(), qReleaseOrigin) > 1.0F && !qDashed)
        { qDashed = true; ReferenceQueueFrame(L"oracle-nicky-q-rush.bmp"); }
        if (qReleased && state == PlayerStateType::Wait && age > 1.0F)
        {
            Vec3 wanted = qAimPoint - qReleaseOrigin;
            Vec3 actual = player->GetTransform()->GetPosition() - qReleaseOrigin;
            if (wanted.Length() > 0.1F && actual.Length() > 0.1F)
            { wanted.Normalize(); actual.Normalize(); qTowardAim = wanted.Dot(actual) > 0.8F; }
            if (!qCharging || !qChargeAnimation || !qChargeMoved || !qDashed || !qTowardAim) { finish(false, "Q charge movement or aimed release dash was not observed"); return; }
            advance(13);
        }
        if (stage == 12 && age > 7.0F) { finish(false, "Q charge and release did not complete"); return; }
    }
    else if (stage == 13)
    {
        if (animation.find(L"Skill_01_Charge") != std::wstring::npos)
            qCooldownWithoutReplay = false;
        if (player->GetSkill(0)->GetCurrentCooldown() <= 0 && state == PlayerStateType::Wait)
        {
            maxQStart = player->GetTransform()->GetPosition();
            maxQAim = maxQStart + Vec3(12, 0, 0);
            advance(14);
        }
        else if (age > 7.0F) { finish(false, "Q did not become available after its source cooldown"); return; }
    }
    else if (stage == 14)
    {
        pointAt(maxQAim);
        if (age < 5.2F) input.keys['Q'] = true;
        const auto distance = Vec3::Distance(player->GetTransform()->GetPosition(), maxQStart);
        if (age < 5.15F && distance > 0.1F) { finish(false, "Q moved before the source key-release trigger"); return; }
        if (age > 4.9F && age < 5.2F && state == PlayerStateType::Skill_1 && states->GetCurrentStatePtr()->IsCharging())
            maxQCharged = true;
        if (age > 5.0F && age < 5.2F && animation == L"Skill_01_Rush" && !maxQAutoAnimation)
        { maxQAutoAnimation = true; ReferenceQueueFrame(L"oracle-nicky-q-max-charge.bmp"); }
        maxQReleased = maxQReleased || (state == PlayerStateType::Skill_1 && states->GetCurrentStatePtr()->IsReleasing());
        if (maxQReleased) maxQDistance = (std::max)(maxQDistance, distance);
        if (maxQReleased && state == PlayerStateType::Wait && age > 5.2F)
        {
            // Source range caps at15; its unclamped frame interpolation can
            // overshoot by one15-units/second step before forcing recovery.
            if (!qCooldownWithoutReplay || !maxQCharged || !maxQAutoAnimation || maxQDistance < 14.9F || maxQDistance > 15.3F)
            { finish(false, "maximum Q charge did not preserve its capped movement and animation"); return; }
            advance(15);
        }
        if (age > 12.0F) { finish(false, "maximum-charge Q did not finish"); return; }
    }
    else if (stage == 15)
    {
        if (animation.find(L"Skill_01_Charge") != std::wstring::npos || state != PlayerStateType::Wait)
        { finish(false, "completed maximum-charge Q restarted without input"); return; }
        if (age >= 1.0F)
        {
            maxQIdle = animation == L"Wait";
            finish(maxQIdle, "earned QWER; short and maximum Q preserve movement and remain idle until a new cast");
            return;
        }
    }
    previousHp = player->GetStatus().hp;
    const auto position = player->GetTransform()->GetPosition();
    const auto picked = states->GetPickedTargetAtMouse();
    const auto actualMouse = INPUT->GetMousePos();
    auto ray = CURSCENE->GetObjectManager()->CreateRayFromScreen(
        Vec2(static_cast<float>(actualMouse.x), static_cast<float>(actualMouse.y)), CURSCENE->GetMainCamera()->GetCamera());
    float distance = 0.0F;
    const bool directHit = target && target->GetCollider()->Intersects(ray, distance);
    trace << elapsed << ',' << stage << ',' << static_cast<int>(state) << ',' << std::string(animation.begin(), animation.end())
          << ',' << previousHp << ',' << (target ? target->GetMonsterStatus().hp : -1) << ',' << position.x << ',' << position.z
          << ',' << level(3) << ',' << player->GetSkill(3)->GetCurrentCooldown() << ',' << INPUT->GetButtonDown(KEY_TYPE::R)
          << ',' << INPUT->GetButtonDown(KEY_TYPE::LCTRL) << ',' << INPUT->GetButton(KEY_TYPE::LCTRL)
          << ',' << (picked == target) << ',' << directHit << ',' << actualMouse.x << ',' << actualMouse.y << '\n';
    if (++frames % 25 == 0) trace.flush();
}
