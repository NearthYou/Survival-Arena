#pragma once

#include "Wolf.h"
#include "SliderUI.h"
#include "ReferenceAudioCapture.hpp"
#include "ReferenceFrameCapture.hpp"
#include <functional>

struct ReferenceAudioScenario
{
    int lobbyFrames = 0;
    int gameplayFrames = 0;
    bool transitioning = false;
    bool finished = false;
    double baseline = -1, muted = -1, unmuted = -1, transition = -1;
};
inline ReferenceAudioScenario& ReferenceAudioScenarioState()
{
    static ReferenceAudioScenario state;
    return state;
}
inline void ReferenceFinishAudioProbe(bool passed, const char* reason)
{
    auto& state = ReferenceAudioScenarioState();
    state.finished = true;
    std::ofstream output("audio-probe.json");
    output << "{\"passed\":" << (passed ? "true" : "false") << ",\"reason\":\"" << reason
           << "\",\"baseline_rms\":" << state.baseline << ",\"muted_rms\":" << state.muted
           << ",\"unmuted_sfx_rms\":" << state.unmuted << ",\"transition_rms\":" << state.transition
           << ",\"lobby_frames\":" << state.lobbyFrames << ",\"gameplay_frames\":" << state.gameplayFrames
           << ",\"output\":\"offline-software-mixer\",\"desktop_input\":false}";
    output.close();
    ReferenceQueueFrame(passed ? L"oracle-audio-final.bmp" : L"oracle-audio-failed.bmp");
    ReferenceDetachAudioCapture();
    PostQuitMessage(passed ? 0 : 1);
}
inline void ReferenceLobbyAudioProbe(const std::shared_ptr<SliderUI>& bgm, const std::shared_ptr<SliderUI>& sfx,
    const std::function<void()>& hover, const std::function<void()>& start)
{
    auto& state = ReferenceAudioScenarioState();
    if (state.finished || state.transitioning || !ReferenceAudioReady()) return;
    try
    {
        const int frame = ++state.lobbyFrames;
        if (frame == 32) { ReferenceBeginAudioWindow(); ReferenceQueueFrame(L"oracle-audio-default.bmp"); }
        if (frame == 160)
        {
            state.baseline = ReferenceEndAudioWindow("audio-default-bgm").rms;
            if (state.baseline < 1e-5) { ReferenceFinishAudioProbe(false, "source lobby BGM produced no PCM"); return; }
            bgm->SetValue(0); sfx->SetValue(0);
        }
        if (frame == 176) { ReferenceBeginAudioWindow(); ReferenceQueueFrame(L"oracle-audio-muted.bmp"); }
        if (frame >= 176 && frame < 304 && frame % 16 == 0) hover();
        if (frame == 304)
        {
            state.muted = ReferenceEndAudioWindow("audio-muted-hover").rms;
            if (state.muted > 1e-8) { ReferenceFinishAudioProbe(false, "UI mute did not silence repeated hover sounds"); return; }
            sfx->SetValue(1);
            ReferenceBeginAudioWindow(); hover();
        }
        if (frame == 368)
        {
            state.unmuted = ReferenceEndAudioWindow("audio-unmuted-hover").rms;
            if (state.unmuted < 1e-5) { ReferenceFinishAudioProbe(false, "UI unmute did not restore the real hover sound"); return; }
            sfx->SetValue(0);
            ReferenceQueueFrame(L"oracle-audio-before-transition.bmp");
        }
        if (frame == 384)
        {
            ReferenceBeginAudioWindow();
            state.transitioning = true;
            start(); // Original start button, then original selection/countdown.
        }
    }
    catch (const std::exception& error) { ReferenceFinishAudioProbe(false, error.what()); }
}
inline void ReferenceAudioProbe(const std::shared_ptr<Wolf>& wolf)
{
    static std::weak_ptr<Wolf> observer;
    if (observer.expired()) observer = wolf;
    if (observer.lock() != wolf) return;
    auto& state = ReferenceAudioScenarioState();
    if (state.finished || !state.transitioning) return;
    if (++state.gameplayFrames != 128) return;
    try
    {
        state.transition = ReferenceEndAudioWindow("audio-muted-scene-transition").rms;
        ReferenceFinishAudioProbe(state.transition < 1e-8,
            state.transition < 1e-8 ? "real UI sliders, hover playback and mute across selection into gameplay verified"
                                   : "scene transition reset a muted audio category");
    }
    catch (const std::exception& error) { ReferenceFinishAudioProbe(false, error.what()); }
}
