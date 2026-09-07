#pragma once
#include <array>
#include <chrono>
#include <fstream>

// Continue the real traversal after all six wolves and Alpha were defeated.
// Only input is generated: no actor movement, damage, resources or stats are assigned.
inline void ReferenceSoakInput()
{
    using Clock = std::chrono::steady_clock;
    static bool initialized = false;
    static Clock::time_point start;
    static Vec3 center{}, previous{};
    static double nextRecord = 0, nextCapture = 60, walked = 0;
    static int destinationIndex = 0;
    static std::array<unsigned, 4> casts{};
    auto& input = ReferenceProbeInput();
    input.keys.fill(false);
    const auto player = InventoryManager::GetInstance()->GetPlayer();
    if (!player) { PostQuitMessage(1); return; }
    if (!initialized)
    {
        initialized = true;
        start = Clock::now();
        center = previous = player->GetTransform()->GetPosition();
        player->GetPlayerStateMachine()->OnSkillUsed += [](int index, std::shared_ptr<GameObject>) {
            if (index >= 0 && index < 4) ++casts[index];
        };
    }
    const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    const auto position = player->GetTransform()->GetPosition();
    const float distance = Vec3::Distance(position, previous);
    if (!std::isfinite(distance) || player->GetStatus().hp <= 0)
    { PostQuitMessage(1); return; }
    walked += distance;
    previous = position;
    static const Vec3 offsets[] = {Vec3(5,0,0), Vec3(0,0,5), Vec3(-5,0,0), Vec3(0,0,-5)};
    auto destination = center + offsets[destinationIndex];
    if (Vec3::Distance(position, destination) < 1.f)
    {
        destinationIndex = (destinationIndex + 1) % 4;
        destination = center + offsets[destinationIndex];
    }
    const auto camera = CURSCENE->GetMainCamera()->GetCamera();
    const auto screen = GRAPHICS->GetViewport().Project(destination, Matrix::Identity,
        camera->GetViewMatrix(), camera->GetProjectionMatrix());
    input.mouse = POINT{static_cast<LONG>(screen.x), static_cast<LONG>(screen.y)};
    if (std::fmod(elapsed, 1.0) < 0.06) input.keys[VK_RBUTTON] = true;
    const double learnCycle = std::fmod(elapsed, 2.0);
    if (player->GetStatus().availableSkillPoints > 0 && learnCycle < 0.3)
    {
        for (int index = 0; index < 4; ++index)
        {
            if (player->GetSkill(index)->GetCurSkillLevel() != 0) continue;
            input.keys[VK_LCONTROL] = true;
            if (learnCycle > 0.1) input.keys[std::array<int,4>{'Q','W','E','R'}[index]] = true;
            break;
        }
    }
    if (!input.keys[VK_LCONTROL])
    {
        if (std::fmod(elapsed, 13.0) < 0.5) input.keys['Q'] = true;
        else if (std::fmod(elapsed, 17.0) < 0.06) input.keys['W'] = true;
        else if (std::fmod(elapsed, 11.0) < 0.06) input.keys['E'] = true;
    }
    if (elapsed >= nextRecord)
    {
        nextRecord = elapsed + 1.;
        std::ofstream state("soak-state.json");
        state << "{\"seconds_after_combat\":" << elapsed << ",\"walked\":" << walked
              << ",\"player_hp\":" << player->GetStatus().hp
              << ",\"stamina\":" << player->GetStatus().stamina
              << ",\"q_casts\":" << casts[0] << ",\"w_casts\":" << casts[1]
              << ",\"e_casts\":" << casts[2] << "}";
    }
    if (elapsed >= nextCapture)
    {
        const auto name = L"oracle-soak-" + std::to_wstring(static_cast<int>(elapsed)) + L".bmp";
        ReferenceQueueFrame(name.c_str());
        nextCapture = elapsed + 300.;
    }
}
