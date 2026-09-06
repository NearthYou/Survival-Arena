#pragma once

#include "Wolf.h"
#include "Alpha.h"
#include "Player.h"
#include "PlayerStateMachine.h"
#include "MonsterStateMachine.h"
#include "ModelAnimator.h"
#include "NavMesh.h"
#include "NavMeshAgent.h"
#include "Camera.h"
#include "Viewport.h"
#include "InventoryManager.h"
#include "BiancaESkillCircle.h"
#include "ReferenceProbeInput.hpp"
#include "ReferenceFrameCapture.hpp"
#include <cfloat>
#include <cmath>
#include <fstream>

// Test observer only. The private build supplies synthetic input to the original
// consumers; this code never moves actors, assigns stats or deals damage.
inline void ReferenceTraversalProbe(const std::shared_ptr<Wolf>& wolf)
{
    static std::weak_ptr<Wolf> observer;
    if (observer.expired()) observer = wolf;
    if (observer.lock() != wolf) return;
    static std::ofstream trace("traversal-boss.csv");
    static std::shared_ptr<Monster> target;
    static std::shared_ptr<Alpha> boss;
    static std::shared_ptr<NavMesh> nav;
    static float elapsed = 0, stageAt = 0, commandAt = -1, progressAt = 0;
    static float walked = 0, minY = FLT_MAX, maxY = -FLT_MAX, maxStep = 0;
    static float chaseRunTime = 0;
    static Vec3 previous{}, progressPosition{}, bossOrigin{}, destination{};
    static int stage = 0, kills = 0, frames = 0, levelBefore = 1, bossHpBefore = 0;
    static bool initialized = false, finished = false, bossPhase = false;
    static bool navigationDiagnosed = false;
    static bool attackAlreadyStarted = false;
    static bool runSeen = false, runAnimation = false, bossReached = false;
    static bool chaseRun = false;
    static bool bossAttack = false, bossSkill = false, bossIncoming = false, bossDeath = false;
    auto& input = ReferenceProbeInput();
    input.keys.fill(false);
    if (finished) return;
    elapsed += DT;
    const auto player = InventoryManager::GetInstance()->GetPlayer();
    const auto finish = [&](bool passed, const char* reason) {
        finished = true;
        std::ofstream output("traversal-boss.json");
        const auto character = player ? player->GetName() : std::wstring{};
        output << "{\"passed\":" << (passed ? "true" : "false") << ",\"reason\":\"" << reason
               << "\",\"seconds\":" << elapsed << ",\"stage\":" << stage << ",\"wolves_killed\":" << kills
               << ",\"walked\":" << walked << ",\"max_frame_step\":" << maxStep << ",\"min_y\":" << minY << ",\"max_y\":" << maxY
               << ",\"run_state\":" << runSeen << ",\"run_animation\":" << runAnimation << ",\"boss_reached\":" << bossReached
               << ",\"chase_run\":" << chaseRun << ",\"player_name\":\"" << std::string(character.begin(), character.end()) << "\""
               << ",\"boss_attack\":" << bossAttack << ",\"boss_skill\":" << bossSkill << ",\"boss_incoming\":" << bossIncoming
               << ",\"boss_dead\":" << bossDeath << ",\"boss_hp\":" << (boss ? boss->GetMonsterStatus().hp : -1)
               << ",\"player_hp\":" << (player ? player->GetStatus().hp : -1)
               << ",\"player_level\":" << (player ? player->GetStatus().level : -1) << "}";
        output.flush(); trace.flush();
        ReferenceQueueFrame(passed ? L"oracle-traversal-final.bmp" : L"oracle-traversal-failed.bmp");
        PostQuitMessage(passed ? 0 : 1);
    };
    if (elapsed > 160) { finish(false, "traversal game-time deadline"); return; }
    if (!player || elapsed < 2 || InventoryManager::GetInstance()->GetInventorySlots().size() != 10) return;
    if (player->GetStatus().hp <= 0) { finish(false, "player died during ordinary traversal or combat"); return; }
    const auto position = player->GetTransform()->GetPosition();
    const auto states = player->GetPlayerStateMachine();
    const auto state = states->GetCurrentState();
    const auto animation = player->GetModelAnimator()->GetCurrentAnimationTag();
    const auto flatDistance = [](Vec3 a, Vec3 b) { a.y = b.y = 0; return Vec3::Distance(a, b); };
    const auto pointAt = [&](const Vec3& point) {
        const auto camera = CURSCENE->GetMainCamera()->GetCamera();
        auto viewport = GRAPHICS->GetViewport();
        const auto screen = viewport.Project(point, Matrix::Identity, camera->GetViewMatrix(), camera->GetProjectionMatrix());
        if (!std::isfinite(screen.x) || !std::isfinite(screen.y) || screen.z < 0 || screen.z > 1 ||
            screen.x < 20 || screen.x > viewport.GetWidth() - 20 || screen.y < 20 || screen.y > viewport.GetHeight() * 0.72F)
            return false;
        input.mouse = POINT{static_cast<LONG>(screen.x), static_cast<LONG>(screen.y)};
        return true;
    };
    const auto advance = [&](int next) {
        stage = next; stageAt = elapsed; commandAt = -1; progressAt = elapsed;
        progressPosition = position; input.keys.fill(false);
    };
    if (!initialized)
    {
        int wolfCount = 0;
        for (const auto& object : CURSCENE->GetObjects())
        {
            if (std::dynamic_pointer_cast<Wolf>(object)) ++wolfCount;
            if (const auto candidate = std::dynamic_pointer_cast<Alpha>(object)) boss = candidate;
            if (const auto mesh = object->GetFixedComponent<NavMesh>(ComponentType::NavMesh)) nav = mesh;
        }
        if (wolfCount != 6 || !boss || !nav || boss->GetMonsterStatus().hp != 1000)
        { finish(false, "original six-wolf and Alpha scene was not present"); return; }
        initialized = true; previous = position; bossOrigin = boss->GetTransform()->GetPosition();
        trace << "seconds,stage,kills,state,animation,player_hp,level,target_hp,x,y,z,nav_state,mouse_x,mouse_y,boss_attack,boss_skill\n";
        ReferenceQueueFrame(L"oracle-traversal-start.bmp");
    }
    const float step = Vec3::Distance(previous, position);
    walked += step; maxStep = (std::max)(maxStep, step); previous = position;
    minY = (std::min)(minY, position.y); maxY = (std::max)(maxY, position.y);
    if (!std::isfinite(step) || step > 2.0F) { finish(false, "discontinuous movement without a mobility skill"); return; }
    runSeen = runSeen || state == PlayerStateType::Run;
    runAnimation = runAnimation || animation == L"Run";
    if (state == PlayerStateType::BaseAttack && player->GetNavMeshAgent()->IsMoving() && step > 0.001F && animation == L"Run")
        chaseRunTime += DT;
    else chaseRunTime = 0;
    if (!chaseRun && chaseRunTime > 0.1F) { chaseRun = true; ReferenceQueueFrame(L"oracle-combat-run.bmp"); }
    if (bossPhase)
    {
        bossAttack = bossAttack || boss->GetMonsterStateMachine()->IsInState(MonsterStateType::Attack);
        bossIncoming = bossIncoming || player->GetStatus().hp < bossHpBefore;
        for (const auto& object : CURSCENE->GetObjects())
            if (std::dynamic_pointer_cast<BiancaESkillCircle>(object) && object->GetActive() && !bossSkill)
            { bossSkill = true; ReferenceQueueFrame(L"oracle-alpha-skill.bmp"); }
        bossDeath = boss->IsDead() && boss->GetMonsterStatus().hp <= 0;
    }
    if (stage == 0)
    {
        target.reset(); float nearest = FLT_MAX;
        for (const auto& object : CURSCENE->GetObjects())
            if (const auto candidate = std::dynamic_pointer_cast<Wolf>(object))
                if (!candidate->IsDead() && candidate->GetMonsterStatus().hp > 0)
                {
                    const auto distance = flatDistance(position, candidate->GetTransform()->GetPosition());
                    if (distance < nearest) { target = candidate; nearest = distance; }
                }
        if (!target)
        {
            if (kills != 6) { finish(false, "wolves disappeared without observed kills"); return; }
            target = boss; bossPhase = true; bossHpBefore = player->GetStatus().hp;
            ReferenceQueueFrame(L"oracle-traversal-wolves-cleared.bmp");
        }
        levelBefore = player->GetStatus().level;
        attackAlreadyStarted = false;
        advance(1);
    }
    const float age = elapsed - stageAt;
    if (stage == 1)
    {
        const auto targetPosition = target->GetTransform()->GetPosition();
        const float arrive = bossPhase ? 3.8F : 7.0F;
        // A projected approach click can legitimately select the encounter's
        // collider. Adopt that real attack; only cancel an old target's state.
        const bool activeEncounterAttack = state == PlayerStateType::BaseAttack &&
            states->GetCurrentStatePtr()->GetTarget() == target;
        if (activeEncounterAttack || (flatDistance(position, targetPosition) < arrive && state != PlayerStateType::BaseAttack))
        {
            attackAlreadyStarted = activeEncounterAttack;
            if (bossPhase)
            {
                bossReached = flatDistance(position, bossOrigin) < 8 && walked > 100;
                ReferenceQueueFrame(L"oracle-alpha-arrival.bmp");
            }
            advance(2);
        }
        else if (elapsed - commandAt >= 0.6F)
        {
            std::vector<Vec3> path;
            nav->FindPath(position, nav->GetNearestPointOnNavMesh(targetPosition), path);
            if (path.empty()) { finish(false, "source navigation produced no route"); return; }
            destination = path.back();
            for (const auto& node : path)
                if (flatDistance(node, position) > 0.7F) { destination = node; break; }
            Vec3 direction = destination - position;
            // A movement click must stop outside the encounter collider. Clicking
            // its center would correctly become an attack before arrival is checked.
            if (flatDistance(destination, targetPosition) < 0.5F && direction.Length() > arrive * 0.9F)
                direction *= (direction.Length() - arrive * 0.9F) / direction.Length();
            if (flatDistance(position, targetPosition) < arrive && state == PlayerStateType::BaseAttack)
                direction = position - targetPosition;
            if (direction.Length() > 6) { direction.Normalize(); direction *= 6; }
            destination = nav->GetNearestPointOnNavMesh(position + direction);
            if (!pointAt(destination)) { finish(false, "next movement point was outside the playable viewport"); return; }
            input.keys[VK_RBUTTON] = true; commandAt = elapsed;
        }
        if (flatDistance(position, progressPosition) > 0.5F) { progressAt = elapsed; progressPosition = position; }
        if (!navigationDiagnosed && elapsed - progressAt > 2)
        {
            navigationDiagnosed = true;
            std::ofstream diagnostics("traversal-navigation.csv");
            diagnostics << "kind,index,x,y,z,nearest_error\n";
            const auto record = [&](const char* kind, int index, const Vec3& point) {
                diagnostics << kind << ',' << index << ',' << point.x << ',' << point.y << ',' << point.z
                            << ',' << Vec3::Distance(point, nav->GetNearestPointOnNavMesh(point)) << '\n';
            };
            record("player", 0, position); record("command", 0, destination); record("target", 0, targetPosition);
            auto viewport = GRAPHICS->GetViewport();
            const auto camera = CURSCENE->GetMainCamera()->GetCamera();
            const auto mouse = INPUT->GetMousePos();
            const auto nearPoint = viewport.UnProject(Vec3(static_cast<float>(mouse.x), static_cast<float>(mouse.y), 0), Matrix::Identity, camera->GetViewMatrix(), camera->GetProjectionMatrix());
            const auto farPoint = viewport.UnProject(Vec3(static_cast<float>(mouse.x), static_cast<float>(mouse.y), 1), Matrix::Identity, camera->GetViewMatrix(), camera->GetProjectionMatrix());
            auto direction = farPoint - nearPoint; direction.Normalize();
            Vec3 hit;
            if (nav->RaycastNavMesh(Ray(nearPoint, direction), hit))
            {
                record("input_hit", 0, hit);
                std::vector<Vec3> inputPath;
                nav->FindPath(position, hit, inputPath);
                for (size_t index = 0; index < inputPath.size(); ++index) record("input_path", static_cast<int>(index), inputPath[index]);
            }
            std::vector<Vec3> encounterPath;
            nav->FindPath(position, nav->GetNearestPointOnNavMesh(targetPosition), encounterPath);
            for (size_t index = 0; index < encounterPath.size(); ++index) record("encounter_path", static_cast<int>(index), encounterPath[index]);
            direction = destination - position;
            if (direction.Length() > 0.001F)
            {
                direction.Normalize();
                record("small_step", 0, position + direction * 0.05F);
                record("projected_step", 0, nav->GetNearestPointOnNavMesh(position + direction * 0.05F));
            }
            ReferenceQueueFrame(L"oracle-navigation-stall.bmp");
        }
        if (stage == 1 && elapsed - progressAt > 8) { finish(false, "right-click route made no movement progress"); return; }
        if (stage == 1 && age > 65) { finish(false, "could not reach the next encounter"); return; }
    }
    else if (stage == 2)
    {
        if (!attackAlreadyStarted && age < 0.06F)
        {
            // Aim higher on the visible body when its feet are near the HUD.
            if (!pointAt(target->GetTransform()->GetPosition() + Vec3(0, 1, 0)) &&
                !pointAt(target->GetTransform()->GetPosition() + Vec3(0, 2, 0)) &&
                !pointAt(target->GetTransform()->GetPosition() + Vec3(0, 3, 0)))
            { finish(false, "encounter target was outside the playable viewport"); return; }
            input.keys[VK_RBUTTON] = true;
        }
        if (target->IsDead() && target->GetMonsterStatus().hp <= 0)
        {
            if (!bossPhase) ++kills;
            advance(3);
        }
        else if (age > 22) { finish(false, "ordinary right-click attack did not finish the encounter"); return; }
    }
    else if (stage == 3 && age > 2)
    {
        if (player->GetStatus().level <= levelBefore) { finish(false, "kill did not deliver its source progression reward"); return; }
        if (bossPhase)
        {
            if (!bossReached || !bossAttack || !bossIncoming || !bossSkill || !bossDeath || !runSeen || !runAnimation)
            { finish(false, "boss or traversal checkpoints were incomplete"); return; }
            if (player->GetName() == L"Nicky" && !chaseRun)
            { finish(false, "Nicky combat pursuit did not sustain the run animation"); return; }
            finish(true, "input-only map traversal, six wolf kills and progression, Alpha attack and skill, living-player boss kill");
            return;
        }
        const auto capture = L"oracle-wolf-" + std::to_wstring(kills) + L"-cleared.bmp";
        ReferenceQueueFrame(capture.c_str());
        advance(0);
    }
    trace << elapsed << ',' << stage << ',' << kills << ',' << static_cast<int>(state) << ',' << std::string(animation.begin(), animation.end())
          << ',' << player->GetStatus().hp << ',' << player->GetStatus().level << ',' << (target ? target->GetMonsterStatus().hp : -1)
          << ',' << position.x << ',' << position.y << ',' << position.z << ',' << static_cast<int>(player->GetNavMeshAgent()->GetState())
          << ',' << input.mouse.x << ',' << input.mouse.y << ',' << bossAttack << ',' << bossSkill << '\n';
    if (++frames % 25 == 0) trace.flush();
}
