#include <dxa/engine/EngineApp.hpp>

#include <dxa/engine/AssetSceneRenderer.hpp>
#include <dxa/engine/ForwardRenderer.hpp>
#include <dxa/engine/FrameClock.hpp>
#include <dxa/engine/GroundPlanePicking.hpp>
#include <dxa/engine/GpuFrameTimer.hpp>
#include <dxa/engine/GraphicsDevice.hpp>
#include <dxa/engine/HybridDeferredRenderer.hpp>
#include <dxa/engine/InputState.hpp>
#include <dxa/engine/RuntimeScene.hpp>
#include <dxa/engine/SystemMetrics.hpp>
#include <dxa/engine/Window.hpp>
#include <dxa/engine/benchmark/BenchmarkReport.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace dxa::engine
{
namespace
{
constexpr std::uint32_t ExpectedStressObjects = static_cast<std::uint32_t>(
    benchmark::PlayerCount
    + benchmark::AiCount
    + benchmark::StaticInstanceCount);
constexpr double RuntimeFixedSeconds = 1.0 / 30.0;
constexpr std::size_t MaximumRuntimeUpdatesPerFrame = 5U;

void ValidateBenchmarkOptions(const EngineRunOptions& options)
{
    if (!options.benchmark.has_value())
    {
        return;
    }

    const BenchmarkRunOptions& benchmarkOptions = *options.benchmark;
    if (benchmarkOptions.outputDirectory.empty()
        || benchmarkOptions.measuredFrames == 0
        || benchmarkOptions.commitSha.empty())
    {
        throw std::invalid_argument{"benchmark output, measured frames, and commit SHA are required"};
    }
    if (benchmarkOptions.measuredFrames > GpuFrameTimer::MaximumQuerySlotCount)
    {
        throw std::invalid_argument{"benchmark measured frame count exceeds GPU query capacity"};
    }
    if (options.vsync)
    {
        throw std::invalid_argument{"benchmark run requires vsync to be disabled"};
    }
    if (benchmarkOptions.warmupFrames > std::numeric_limits<std::uint32_t>::max()
            - benchmarkOptions.measuredFrames
        || options.frameLimit
            != benchmarkOptions.warmupFrames + benchmarkOptions.measuredFrames)
    {
        throw std::invalid_argument{"benchmark frame limit does not match its measurement window"};
    }
}

void ApplyGpuResults(
    std::vector<benchmark::FrameSample>& samples,
    const std::span<const GpuFrameResult> results,
    const std::uint32_t firstMeasuredFrame)
{
    for (const GpuFrameResult& result : results)
    {
        if (result.frameIndex < firstMeasuredFrame)
        {
            continue;
        }
        const std::uint64_t offset = result.frameIndex - firstMeasuredFrame;
        if (offset >= samples.size())
        {
            continue;
        }
        benchmark::FrameSample& sample = samples[static_cast<std::size_t>(offset)];
        if (sample.frameIndex == result.frameIndex)
        {
            sample.gpuForwardMilliseconds = result.passDurations.forwardMilliseconds;
            sample.gpuTotalMilliseconds = result.passDurations.totalMilliseconds;
            sample.gpuShadowMilliseconds = result.passDurations.shadowMilliseconds;
            sample.gpuGBufferMilliseconds = result.passDurations.gBufferMilliseconds;
            sample.gpuLightingMilliseconds = result.passDurations.lightingMilliseconds;
            sample.gpuTransparentMilliseconds = result.passDurations.transparentMilliseconds;
        }
    }
}
} // namespace

int EngineApp::Run(
    const EngineRunOptions& options,
    const std::filesystem::path& shaderPath,
    const std::filesystem::path& assetRoot,
    IRuntimeSceneController* const runtimeScene) const
{
    ValidateBenchmarkOptions(options);
    if (runtimeScene != nullptr
        && options.renderPath != RenderPath::HybridDeferred)
    {
        throw std::invalid_argument{
            "runtime scene requires the hybrid deferred renderer"};
    }

    InputState input;
    Window window;
    window.Create(
        WindowConfig{L"DX11 Survival Arena", options.width, options.height, options.hidden},
        input);

    GraphicsDevice graphics;
    graphics.Initialize(GraphicsDeviceConfig{
        window.NativeHandle(),
        options.width,
        options.height,
        options.driver,
#if defined(_DEBUG)
        true
#else
        false
#endif
    });

    GpuFrameTimer gpuTimer;
    if (options.benchmark.has_value())
    {
        gpuTimer.Initialize(graphics.Device(), options.benchmark->measuredFrames);
    }

    const bool useAssetScene = !assetRoot.empty();
    const bool useHybridDeferred = options.renderPath == RenderPath::HybridDeferred;
    if (useHybridDeferred && !useAssetScene)
    {
        throw std::runtime_error{"hybrid deferred renderer requires runtime assets"};
    }
    ForwardRenderer fallbackRenderer;
    AssetSceneRenderer assetRenderer;
    HybridDeferredRenderer hybridRenderer;
    if (useHybridDeferred)
    {
        hybridRenderer.Initialize(
            graphics.Device(),
            HybridDeferredConfig{
                options.width,
                options.height,
                2048,
                options.benchmark.has_value()
                    ? options.benchmark->seed
                    : 20260823U,
                shaderPath.parent_path(),
                assetRoot});
    }
    else if (useAssetScene)
    {
        assetRenderer.Initialize(
            graphics.Device(),
            shaderPath.parent_path() / L"asset_scene.hlsl",
            assetRoot,
            AssetSceneConfig{
                options.benchmark.has_value()
                    ? std::optional<std::uint32_t>{options.benchmark->seed}
                    : std::nullopt});
    }
    else
    {
        fallbackRenderer.Initialize(graphics.Device(), shaderPath);
    }
    const bool selectedAssetSceneReady = useHybridDeferred
        ? hybridRenderer.ShadowMapReady()
        : useAssetScene && assetRenderer.AssetSceneReady();
    if (options.verifyAssetScene && !selectedAssetSceneReady)
    {
        throw std::runtime_error{"asset scene verification requirements were not met"};
    }
    if (options.benchmark.has_value() && !useAssetScene)
    {
        throw std::runtime_error{"stress scene requires runtime assets"};
    }

    constexpr std::array ClearColor{0.025F, 0.035F, 0.060F, 1.0F};
    FrameClock clock{FrameClock::Clock::now()};
    bool renderVerified = !options.verifyRender;
    std::vector<benchmark::FrameSample> samples;
    std::string adapterName;
    std::uint32_t firstMeasuredFrame = 0;
    std::uint32_t finalMeasuredFrame = 0;
    std::size_t gpuBeginRejected = 0;
    std::size_t gpuResolved = 0;
    std::size_t gpuDiscarded = 0;
    double runtimeAccumulator = 0.0;
    std::optional<benchmark::SceneVector3> pendingMoveDestination;
    if (options.benchmark.has_value())
    {
        samples.reserve(options.benchmark->measuredFrames);
        adapterName = GetAdapterNameUtf8(graphics.Device());
        firstMeasuredFrame = options.benchmark->warmupFrames + 1;
        finalMeasuredFrame = options.benchmark->warmupFrames
            + options.benchmark->measuredFrames;
    }

    while (true)
    {
        input.BeginFrame();
        if (!window.PumpMessages())
        {
            break;
        }

        const FrameTiming timing = clock.Tick(FrameClock::Clock::now());
        if (runtimeScene != nullptr)
        {
            if (input.WasRightPointerPressed())
            {
                const PointerPosition pointer = input.Pointer();
                if (pointer.x >= 0
                    && pointer.y >= 0
                    && static_cast<std::uint32_t>(pointer.x) < options.width
                    && static_cast<std::uint32_t>(pointer.y) < options.height)
                {
                    pendingMoveDestination = PointerGroundDestination(
                        pointer,
                        options.width,
                        options.height,
                        benchmark::SampleStressCamera(timing.frameIndex));
                }
            }

            runtimeAccumulator += timing.deltaSeconds;
            std::size_t updateCount = 0U;
            while (runtimeAccumulator >= RuntimeFixedSeconds
                   && updateCount < MaximumRuntimeUpdatesPerFrame)
            {
                RuntimeInputFrame runtimeInput;
                if (updateCount == 0U
                    && pendingMoveDestination.has_value())
                {
                    runtimeInput.moveDestination = pendingMoveDestination;
                    pendingMoveDestination.reset();
                }
                runtimeScene->FixedUpdate(runtimeInput);
                runtimeAccumulator -= RuntimeFixedSeconds;
                ++updateCount;
            }
            if (updateCount == MaximumRuntimeUpdatesPerFrame)
            {
                runtimeAccumulator = std::min(
                    runtimeAccumulator,
                    RuntimeFixedSeconds
                        * static_cast<double>(
                            MaximumRuntimeUpdatesPerFrame));
            }

            const RuntimeSceneFrame scene = runtimeScene->SampleScene();
            hybridRenderer.SetPlayerStates(scene.players);
            hybridRenderer.SetAiStates(scene.ai);
            hybridRenderer.SetZoneRadius(scene.zoneRadius);
            hybridRenderer.SetControlledPlayerPosition(
                scene.controlledPlayer);
        }
        const bool measuredFrame = options.benchmark.has_value()
            && timing.frameIndex >= firstMeasuredFrame
            && timing.frameIndex <= finalMeasuredFrame;
        const auto cpuStart = std::chrono::steady_clock::now();
        graphics.BeginFrame(ClearColor);
        const bool gpuTimingActive = measuredFrame
            && gpuTimer.BeginFrame(graphics.Context(), timing.frameIndex);
        if (measuredFrame && !gpuTimingActive)
        {
            ++gpuBeginRejected;
        }
        RenderStatistics statistics;
        if (useHybridDeferred)
        {
            RenderPassCallback passCompleted;
            if (gpuTimingActive)
            {
                passCompleted = [&](const RenderPass pass) {
                    gpuTimer.MarkPass(graphics.Context(), pass);
                };
            }
            statistics = hybridRenderer.Render(
                graphics.Context(),
                graphics.BackBufferRenderTargetView(),
                AssetSceneFrame{
                    timing.frameIndex,
                    timing.totalSeconds,
                    static_cast<float>(options.width) / static_cast<float>(options.height)},
                passCompleted);
        }
        else if (useAssetScene)
        {
            statistics = assetRenderer.Render(
                graphics.Context(),
                AssetSceneFrame{
                    timing.frameIndex,
                    timing.totalSeconds,
                    static_cast<float>(options.width) / static_cast<float>(options.height)});
        }
        if (options.benchmark.has_value() && useAssetScene)
        {
            if (statistics.objectCount != ExpectedStressObjects
                || statistics.triangleCount <= statistics.drawCalls)
            {
                throw std::runtime_error{"stress scene render statistics are incomplete"};
            }
            if (useHybridDeferred)
            {
                if (statistics.visibleObjectCount + statistics.culledObjectCount
                        != statistics.objectCount
                    || statistics.shadowDrawCalls == 0
                    || statistics.gBufferDrawCalls == 0
                    || statistics.lightingDrawCalls != 1
                    || statistics.transparentDrawCalls != 1)
                {
                    throw std::runtime_error{"hybrid pass statistics are incomplete"};
                }
            }
            else if (statistics.drawCalls < statistics.objectCount)
            {
                throw std::runtime_error{"forward stress scene draw statistics are incomplete"};
            }
        }
        if (!useAssetScene)
        {
            fallbackRenderer.Render(
                graphics.Context(),
                timing.totalSeconds,
                static_cast<float>(options.width) / static_cast<float>(options.height));
        }
        if (gpuTimingActive)
        {
            gpuTimer.EndFrame(graphics.Context());
        }

        if (options.verifyRender && timing.frameIndex == options.frameLimit)
        {
            if (!graphics.BackBufferContainsNonClearPixel(ClearColor))
            {
                throw std::runtime_error("render verification found only the clear color");
            }
            renderVerified = true;
        }

        graphics.EndFrame(options.vsync);
        const auto cpuEnd = std::chrono::steady_clock::now();

        if (measuredFrame)
        {
            const double cpuMilliseconds = std::chrono::duration<double, std::milli>(
                cpuEnd - cpuStart).count();
            benchmark::FrameSample sample{
                timing.frameIndex,
                cpuMilliseconds,
                std::nullopt,
                statistics.drawCalls,
                statistics.triangleCount,
                statistics.objectCount,
                GetCurrentProcessWorkingSetBytes()};
            sample.shadowDrawCalls = statistics.shadowDrawCalls;
            sample.gBufferDrawCalls = statistics.gBufferDrawCalls;
            sample.lightingDrawCalls = statistics.lightingDrawCalls;
            sample.transparentDrawCalls = statistics.transparentDrawCalls;
            sample.visibleObjectCount = statistics.visibleObjectCount;
            sample.culledObjectCount = statistics.culledObjectCount;
            samples.push_back(sample);
        }
        if (options.benchmark.has_value())
        {
            const auto ready = gpuTimer.ResolveReady(graphics.Context());
            for (const GpuFrameResult& result : ready)
            {
                if (result.elapsedMilliseconds.has_value())
                {
                    ++gpuResolved;
                }
                else
                {
                    ++gpuDiscarded;
                }
            }
            ApplyGpuResults(samples, ready, firstMeasuredFrame);
        }

        if (options.frameLimit != 0 && timing.frameIndex >= options.frameLimit)
        {
            break;
        }
    }

    if (!renderVerified)
    {
        throw std::runtime_error("render verification ended before the requested frame");
    }

    if (options.benchmark.has_value())
    {
        const auto drained = gpuTimer.Drain(graphics.Context(), std::chrono::seconds{2});
        for (const GpuFrameResult& result : drained)
        {
            if (result.elapsedMilliseconds.has_value())
            {
                ++gpuResolved;
            }
            else
            {
                ++gpuDiscarded;
            }
        }
        ApplyGpuResults(samples, drained, firstMeasuredFrame);
        const std::size_t gpuAccounted = gpuBeginRejected + gpuResolved + gpuDiscarded;
        if (gpuAccounted > samples.size())
        {
            throw std::runtime_error{"GPU query diagnostic counts are inconsistent"};
        }
        const std::size_t gpuUnresolved = samples.size() - gpuAccounted;
        std::clog << "GPU query diagnostics: rejected=" << gpuBeginRejected
                  << ", resolved=" << gpuResolved
                  << ", discarded=" << gpuDiscarded
                  << ", unresolved=" << gpuUnresolved << '\n';
        if (samples.size() != options.benchmark->measuredFrames)
        {
            throw std::runtime_error{"benchmark ended before every measured frame completed"};
        }

        benchmark::WriteBenchmarkReport(
            options.benchmark->outputDirectory,
            benchmark::BenchmarkMetadata{
                options.benchmark->seed,
                options.width,
                options.height,
                options.benchmark->warmupFrames,
                options.benchmark->measuredFrames,
                options.benchmark->commitSha,
                adapterName,
                options.benchmark->command,
                options.benchmark->startedAt,
                options.renderPath},
            samples);
    }

    return 0;
}
} // namespace dxa::engine
