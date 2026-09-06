#pragma once

#include "Wolf.h"
#include "WolfBaseAttack.h"
#include "Player.h"
#include "PlayerStateMachine.h"
#include "ReferenceFrameCapture.hpp"
#include <cfloat>
#include <fstream>

// Included only in a private instrumented copy. Requests one ordinary attack order,
// then observes the original runtime without changing stats, skills, AI, or timing.
inline void ReferenceCombatProbe(const std::shared_ptr<Wolf>& wolf)
{
    static std::ofstream output("combat-probe.csv");
    static bool requested = false;
    static int frame = 0;
    static float elapsed = 0.0F;
    static int lastHp = -1;
    static int lastPlayerHp = -1;
    static std::weak_ptr<Wolf> subject;
    std::shared_ptr<Player> player;
    for (const auto& object : CURSCENE->GetObjects())
        if (object->GetType() == OBJECTTYPE::PLAYER)
        {
            player = std::dynamic_pointer_cast<Player>(object);
            if (player) break;
        }
    if (!player) return;
    if (subject.expired())
    {
        float nearest = FLT_MAX;
        for (const auto& object : CURSCENE->GetObjects())
            if (const auto candidate = std::dynamic_pointer_cast<Wolf>(object))
            {
                const float distance = Vec3::Distance(player->GetTransform()->GetPosition(), candidate->GetTransform()->GetPosition());
                if (distance < nearest) { nearest = distance; subject = candidate; }
            }
    }
    if (subject.lock() != wolf) return;
    ++frame;
    elapsed += DT;
    if (frame == 1) output << "frame,seconds,wolf_hp,player_hp,attack_script,player_x,player_z,wolf_x,wolf_z\n";
    if (wolf->GetMonsterStatus().hp != lastHp || player->GetStatus().hp != lastPlayerHp || frame == 1)
    {
        const auto p = player->GetTransform()->GetPosition();
        const auto w = wolf->GetTransform()->GetPosition();
        lastHp = wolf->GetMonsterStatus().hp;
        lastPlayerHp = player->GetStatus().hp;
        output << frame << ',' << elapsed << ',' << lastHp << ',' << lastPlayerHp << ','
               << (wolf->GetComponent<WolfBaseAttack>() ? 1 : 0) << ','
               << p.x << ',' << p.z << ',' << w.x << ',' << w.z << '\n';
        output.flush();
    }
    static bool captured = false;
    if (!captured && elapsed >= 2.0F)
    {
        captured = true;
        ReferenceQueueFrame(L"oracle-gameplay.bmp");
    }
    if (!requested && elapsed >= 3.0F)
    {
        requested = true;
        const auto stateMachine = player->GetPlayerStateMachine();
        stateMachine->GetState(PlayerStateType::BaseAttack)->SetTarget(wolf);
        stateMachine->RequestStateChange(PlayerStateType::BaseAttack);
    }
    if (elapsed >= 8.0F || frame >= 60000)
    {
        ReferenceQueueFrame(L"oracle-final.bmp");
        output.flush();
        PostQuitMessage(0);
    }
}
