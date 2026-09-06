#define NOMINMAX
#include <Windows.h>
#include <FMOD/fmod.hpp>
#include <FMOD/fmod_dsp.h>
#include <FMOD/fmod_errors.h>
#include "ReferenceAudioCapture.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

// Keep the production declaration and methods. The fixture owns device teardown
// and loads one real WAV synchronously; asynchronous loading is a separate gate.
#define DECLARE_SINGLE(Name) public: Name() = default
#define private public
#include "SoundManager.h"
#undef private
#undef DECLARE_SINGLE
SoundManager::~SoundManager() {}
void SoundManager::Free() {}
string soundPath;
void api(FMOD_RESULT result, const char* operation) {
    if (result != FMOD_OK) throw runtime_error(string(operation) + ": " + FMOD_ErrorString(result));
}
void SoundManager::LoadSoundFile() {
    FMOD::Sound* sound = nullptr;
    api(m_System->createSound(soundPath.c_str(), FMOD_DEFAULT, nullptr, &sound), "load reference WAV");
    m_mapSound[L"probe"] = sound;
}
#include "OriginalSoundMethods.inc"

void pump(SoundManager& manager, int blocks) {
    for (int i=0;i<blocks;++i) api(manager.m_System->update(), "offline mixer update");
}
double measure(SoundManager& manager, const char* name) {
    pump(manager, 8);
    ReferenceBeginAudioWindow();
    pump(manager, 128);
    const auto result = ReferenceEndAudioWindow(name);
    cout << name << ",samples=" << result.samples << ",rms=" << result.rms << endl;
    return result.rms;
}
int wmain(int argc, wchar_t** argv) {
    try {
        if (argc != 2) throw runtime_error("Expected the local reference WAV path");
        soundPath = filesystem::path(argv[1]).string();
        SoundManager manager;
        if (FAILED(manager.Init())) throw runtime_error("Source audio initialization failed");
        FMOD::ChannelGroup* master=nullptr;
        api(manager.m_System->getMasterChannelGroup(&master), "master audio group");
        ReferenceAttachAudioCapture(manager.m_System);
        int failures=0;
        const auto require=[&](bool condition,const char* reason){ if(!condition){ cerr << reason << endl; ++failures; } };
        const auto stop=[&]{ api(master->stop(),"reset test voices"); pump(manager,4); };

        manager.PlaySound(L"probe",2,0.5F);
        const auto baseline=measure(manager,"sfx-default");
        require(baseline>1e-5,"Real reference sound produced silence before muting");
        manager.SetSFXVolume(0);
        require(measure(manager,"sfx-current-muted")<1e-8,"SFX mute did not silence the current sound");
        stop(); manager.PlaySound(L"probe",2,0.5F);
        require(measure(manager,"sfx-replay-muted")<1e-8,"New SFX playback ignored the stored mute setting");

        stop(); manager.SetSFXVolume(1);
        manager.PlaySound(L"probe",2,0.5F); manager.PlaySound(L"probe",2,0.5F);
        manager.SetSFXVolume(0);
        require(measure(manager,"sfx-overlap-muted")<1e-8,"SFX mute missed an older voice that reused the channel slot");

        stop(); manager.SetSFXVolume(1);
        manager.PlaySound(L"probe",2,0.5F); manager.PlaySound(L"probe",2,0.5F);
        manager.StopAll();
        require(measure(manager,"all-overlap-stopped")<1e-8,"StopAll left an older overlapping voice playing");

        stop(); manager.SetBGMVolume(1); manager.PlayBGM(L"probe",0.5F);
        require(measure(manager,"bgm-with-sfx-muted")>1e-5,"SFX mute incorrectly muted BGM");
        manager.SetBGMVolume(0);
        require(measure(manager,"bgm-current-muted")<1e-8,"BGM mute did not silence the current music");
        stop(); manager.PlayBGM(L"probe",0.5F);
        require(measure(manager,"bgm-replay-muted")<1e-8,"New BGM playback ignored the stored mute setting");

        stop(); manager.SetSFXVolume(1); manager.PlaySound(L"probe",2,0.5F);
        require(measure(manager,"sfx-with-bgm-muted")>1e-5,"BGM mute incorrectly muted SFX");
        stop(); manager.SetSFXVolume(0.5F); manager.PlaySound(L"probe",2,0.5F);
        const auto half=measure(manager,"sfx-half");
        require(abs(half/baseline-0.5)<0.02,"Category volume did not scale the original per-sound gain");
        ofstream result("audio-settings.json");
        result << "{\"passed\":" << (failures ? "false":"true") << ",\"failures\":" << failures
               << ",\"baseline_rms\":" << baseline << ",\"half_rms\":" << half << ",\"output\":\"offline-software-mixer\"}";
        result.close();
        ReferenceDetachAudioCapture();
        for (auto& item:manager.m_mapSound) item.second->release();
        api(manager.m_System->close(),"close test mixer"); api(manager.m_System->release(),"release test mixer");
        return failures ? 1 : 0;
    } catch(const exception& error) { cerr << error.what() << endl; return 2; }
}
