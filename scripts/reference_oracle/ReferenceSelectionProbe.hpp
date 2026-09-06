#pragma once

#include "Button.h"
#include "ScrollView.h"
#include "UIPanel.h"
#include "Bianca.h"
#include "Nicky.h"
#include "Wolf.h"
#include "ReferenceFrameCapture.hpp"
#include <fstream>

// Private test only. Invoke the buttons the scene actually registered; do not
// call selection handlers directly or inject input into the user's desktop.
namespace reference_selection_probe
{
struct Scenario
{
int phase = 0;
int frames = 0;
int cardClicks = 0;
int skinClicks = 0;
int expectedCharacter = 0;
float observationStart = 0.f;
bool unchangedCountdown = false;
bool explicitStart = false;
bool finished = false;

inline void Finish(bool passed, const char* reason, const char* actual = "none")
{
    if (finished) return;
    finished = true;
    std::ofstream output("selection-probe.json");
    output << "{\"passed\":" << (passed ? "true" : "false")
        << ",\"reason\":\"" << reason << "\",\"actual_player\":\"" << actual
        << "\",\"expected_character\":" << expectedCharacter
        << ",\"card_clicks\":" << cardClicks << ",\"skin_clicks\":" << skinClicks
        << ",\"unchanged_countdown\":" << unchangedCountdown
        << ",\"explicit_start\":" << explicitStart << "}\n";
    output.close();
    ReferenceQueueFrame(passed ? L"oracle-selection-gameplay.bmp" : L"oracle-selection-failure.bmp");
    PostQuitMessage(passed ? 0 : 1);
}

inline void CollectButtons(const std::shared_ptr<GameObject>& object,
                           std::vector<std::shared_ptr<Button>>& buttons)
{
    if (!object) return;
    if (auto button = object->GetButton()) buttons.push_back(button);
    if (auto panel = object->GetUIPanel())
        for (const auto& child : panel->GetChildElements()) CollectButtons(child.lock(), buttons);
}

inline std::vector<std::shared_ptr<Button>> Buttons(const std::shared_ptr<ScrollView>& scroll)
{
    std::vector<std::shared_ptr<Button>> buttons;
    if (scroll)
        for (const auto& element : scroll->GetElements()) CollectButtons(element.lock(), buttons);
    return buttons;
}

inline bool SelectCard(const std::shared_ptr<ScrollView>& cards, int character,
                       const int& selected, const float& elapsed)
{
    const auto name = character == 1 ? L"Bianca" : L"Nicky";
    const float before = elapsed;
    for (const auto& button : Buttons(cards))
        if (button->GetGameObject()->GetName() == name)
        {
            button->InvokeOnClicked();
            ++cardClicks;
            if (selected == character && elapsed == before) return true;
            Finish(false, "Character card changed countdown or selected the wrong character");
            return false;
        }
    Finish(false, "Character card button was not found");
    return false;
}

inline bool CheckSkins(const std::shared_ptr<ScrollView>& skins, int character,
                      const int& selected, const float& elapsed)
{
    const auto buttons = Buttons(skins);
    const size_t expectedCount = character == 1 ? 5 : 6;
    if (buttons.size() != expectedCount)
    {
        Finish(false, "Original skin button list is incomplete");
        return false;
    }
    const float before = elapsed;
    for (int repeat = 0; repeat < 2; ++repeat)
        for (const auto& button : buttons)
        {
            button->InvokeOnClicked();
            ++skinClicks;
            if (selected != character || elapsed != before)
            {
                Finish(false, "Skin button changed character or triggered quick start");
                return false;
            }
        }
    return true;
}
inline void UpdateSelection(const std::shared_ptr<ScrollView>& cards,
                                     const std::shared_ptr<ScrollView>& skins,
                                     const std::shared_ptr<Button>& start,
                                     const int& selected, const float& elapsed,
                                     int finalCharacter)
{
    if (finished || ++frames < 30) return;
    expectedCharacter = finalCharacter;
    // Separate actions across frames so the original deferred UI lifecycle runs.
    switch (phase)
    {
    case 0:
        if (SelectCard(cards, 2, selected, elapsed)) ++phase;
        break;
    case 1:
        if (CheckSkins(skins, 2, selected, elapsed)) ++phase;
        break;
    case 2:
        if (SelectCard(cards, 1, selected, elapsed)) ++phase;
        break;
    case 3:
        if (CheckSkins(skins, 1, selected, elapsed)) ++phase;
        break;
    case 4:
        if (SelectCard(cards, finalCharacter, selected, elapsed)) ++phase;
        break;
    case 5:
        if (CheckSkins(skins, finalCharacter, selected, elapsed))
        {
            observationStart = elapsed;
            ReferenceQueueFrame(L"oracle-selection-skins.bmp");
            ++phase;
        }
        break;
    case 6:
        // The old wrong callback would already have left this scene by now.
        if (elapsed - observationStart >= 12.f)
        {
            if (elapsed >= 20.f || selected != finalCharacter)
            {
                Finish(false, "Skin-only selection did not retain its natural countdown");
                return;
            }
            unchangedCountdown = true;
            ReferenceQueueFrame(L"oracle-selection-no-quick-start.bmp");
            ++phase;
        }
        break;
    case 7:
        if (!start) { Finish(false, "Quick start button was not found"); return; }
        start->InvokeOnClicked();
        start->InvokeOnClicked();
        if (elapsed != 44.5f || selected != finalCharacter)
        {
            Finish(false, "Explicit repeated quick start changed actor or countdown");
            return;
        }
        if (!CheckSkins(skins, finalCharacter, selected, elapsed)) return;
        explicitStart = true;
        ReferenceQueueFrame(L"oracle-selection-explicit-start.bmp");
        ++phase;
        break;
    default:
        break;
    }
}

inline void UpdateGameplay(const std::shared_ptr<Wolf>& wolf)
{
    if (finished) return;
    static std::weak_ptr<Wolf> subject;
    if (subject.expired()) subject = wolf;
    if (subject.lock() != wolf) return;
    static int gameplayFrames = 0;
    if (++gameplayFrames < 60) return;
    for (const auto& object : CURSCENE->GetObjects())
        if (object->GetType() == OBJECTTYPE::PLAYER)
        {
            const bool bianca = std::dynamic_pointer_cast<Bianca>(object) != nullptr;
            const bool nicky = std::dynamic_pointer_cast<Nicky>(object) != nullptr;
            const char* actual = bianca ? "Bianca" : nicky ? "Nicky" : "unknown";
            const bool correct = expectedCharacter == 1 ? bianca : nicky;
            Finish(phase == 8 && unchangedCountdown && explicitStart && correct,
                   correct ? "Registered selection buttons retained actor and countdown" : "Wrong gameplay actor",
                   actual);
            return;
        }
    Finish(false, "Gameplay player was not found");
}
};

inline Scenario& Current()
{
    static Scenario scenario;
    return scenario;
}
}

inline void ReferenceSelectionUiProbe(const std::shared_ptr<ScrollView>& cards,
                                     const std::shared_ptr<ScrollView>& skins,
                                     const std::shared_ptr<Button>& start,
                                     const int& selected, const float& elapsed,
                                     int finalCharacter)
{
    reference_selection_probe::Current().UpdateSelection(cards, skins, start, selected, elapsed, finalCharacter);
}

inline void ReferenceSelectionGameProbe(const std::shared_ptr<Wolf>& wolf)
{
    reference_selection_probe::Current().UpdateGameplay(wolf);
}
