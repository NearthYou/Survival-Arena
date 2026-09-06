#pragma once

#include <FMOD/fmod.hpp>
#include <FMOD/fmod_dsp.h>
#include <FMOD/fmod_errors.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

struct ReferenceAudioCaptureState
{
    FMOD::System* system = nullptr;
    FMOD::DSP* observer = nullptr;
    const std::atomic<bool>* loadingComplete = nullptr;
    std::mutex mutex;
    std::vector<float> samples;
    bool recording = false;
};
inline ReferenceAudioCaptureState& ReferenceAudioCapture()
{
    static ReferenceAudioCaptureState state;
    return state;
}
inline void ReferenceAudioCheck(FMOD_RESULT result, const char* operation)
{
    if (result != FMOD_OK) throw std::runtime_error(std::string(operation) + ": " + FMOD_ErrorString(result));
}
inline FMOD_RESULT F_CALL ReferenceReadAudio(FMOD_DSP_STATE*, float* input, float* output,
    unsigned int length, int channels, int* outputChannels)
{
    *outputChannels = channels;
    const size_t count = static_cast<size_t>(length) * channels;
    if (input) std::copy_n(input, count, output);
    else std::fill_n(output, count, 0.0F);
    auto& state = ReferenceAudioCapture();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.recording) state.samples.insert(state.samples.end(), output, output + count);
    return FMOD_OK;
}
inline FMOD_RESULT F_CALL ReferenceProcessAudioSilence(FMOD_DSP_STATE*, FMOD_BOOL, unsigned int,
    FMOD_CHANNELMASK, int, FMOD_SPEAKERMODE) { return FMOD_OK; }
inline void ReferenceAttachAudioCapture(FMOD::System* system, const std::atomic<bool>* loadingComplete = nullptr)
{
    auto& state = ReferenceAudioCapture();
    if (state.system) throw std::runtime_error("Audio observer already attached");
    FMOD_DSP_DESCRIPTION description{};
    description.pluginsdkversion = FMOD_PLUGIN_SDK_VERSION;
    strcpy_s(description.name, "Reference PCM observer");
    description.version = 0x10000;
    description.numinputbuffers = 1;
    description.numoutputbuffers = 1;
    description.read = ReferenceReadAudio;
    description.shouldiprocess = ReferenceProcessAudioSilence;
    FMOD::ChannelGroup* master = nullptr;
    ReferenceAudioCheck(system->getMasterChannelGroup(&master), "master audio group");
    ReferenceAudioCheck(system->createDSP(&description, &state.observer), "PCM observer");
    ReferenceAudioCheck(master->addDSP(0, state.observer), "attach PCM observer");
    state.system = system;
    state.loadingComplete = loadingComplete;
}
inline bool ReferenceAudioReady()
{
    const auto& state = ReferenceAudioCapture();
    return state.system && (!state.loadingComplete || state.loadingComplete->load());
}
inline void ReferencePumpAudio(int blocks = 1)
{
    // The source loader owns its own update until it publishes readiness.
    if (!ReferenceAudioReady()) return;
    auto system = ReferenceAudioCapture().system;
    for (int i = 0; i < blocks; ++i) ReferenceAudioCheck(system->update(), "offline mixer update");
}
inline void ReferenceBeginAudioWindow()
{
    auto& state = ReferenceAudioCapture();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.samples.clear();
    state.recording = true;
}
struct ReferenceAudioMeasurement { size_t samples; double rms; };
inline ReferenceAudioMeasurement ReferenceEndAudioWindow(const char* name)
{
    auto& state = ReferenceAudioCapture();
    std::vector<float> samples;
    { std::lock_guard<std::mutex> lock(state.mutex); state.recording = false; samples.swap(state.samples); }
    if (samples.size() < 4096) throw std::runtime_error("Mixer produced no measurable PCM blocks");
    double squares = 0;
    for (float value : samples) squares += static_cast<double>(value) * value;
    const double rms = std::sqrt(squares / samples.size());
    std::ofstream wave(std::string(name) + ".wav", std::ios::binary);
    const auto u16 = [&](std::uint16_t value) { wave.write(reinterpret_cast<const char*>(&value), 2); };
    const auto u32 = [&](std::uint32_t value) { wave.write(reinterpret_cast<const char*>(&value), 4); };
    const auto bytes = static_cast<std::uint32_t>(samples.size() * sizeof(float));
    wave.write("RIFF", 4); u32(36 + bytes); wave.write("WAVEfmt ", 8); u32(16);
    u16(3); u16(2); u32(48000); u32(384000); u16(8); u16(32);
    wave.write("data", 4); u32(bytes); wave.write(reinterpret_cast<const char*>(samples.data()), bytes);
    if (!wave) throw std::runtime_error("Could not preserve PCM evidence");
    return {samples.size(), rms};
}
inline void ReferenceDetachAudioCapture()
{
    auto& state = ReferenceAudioCapture();
    if (!state.system) return;
    FMOD::ChannelGroup* master = nullptr;
    ReferenceAudioCheck(state.system->getMasterChannelGroup(&master), "master audio group");
    ReferenceAudioCheck(master->removeDSP(state.observer), "detach PCM observer");
    state.observer->release();
    state.observer = nullptr;
    state.system = nullptr;
    state.loadingComplete = nullptr;
}
