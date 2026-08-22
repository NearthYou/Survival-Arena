#include <dxa/engine/EngineApp.hpp>

#include <dxa/engine/AssetSceneRenderer.hpp>
#include <dxa/engine/ForwardRenderer.hpp>
#include <dxa/engine/FrameClock.hpp>
#include <dxa/engine/GpuFrameTimer.hpp>
#include <dxa/engine/GraphicsDevice.hpp>
#include <dxa/engine/InputState.hpp>
#include <dxa/engine/SystemMetrics.hpp>
#include <dxa/engine/Window.hpp>
#include <dxa/engine/benchmark/BenchmarkReport.hpp>

#include <array>
#include <chrono>
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
            sample.gpuForwardMilliseconds = result.elapsedMilliseconds;
        }
    }
}
} // namespace

int EngineApp::Run(
    const EngineRunOptions& options,
    const std::filesystem::path& shaderPath,
    const std::filesystem::path& assetRoot) const
{
    ValidateBenchmarkOptions(options);

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
        gpuTimer.Initialize(graphics.Device());
    }

    const bool useAssetScene = !assetRoot.empty();
    ForwardRenderer fallbackRenderer;
    AssetSceneRenderer assetRenderer;
    if (useAssetScene)
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
    if (options.verifyAssetScene && (!useAssetScene || !assetRenderer.AssetSceneReady()))
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
        const bool measuredFrame = options.benchmark.has_value()
            && timing.frameIndex >= firstMeasuredFrame
            && timing.frameIndex <= finalMeasuredFrame;
        const auto cpuStart = std::chrono::steady_clock::now();
        graphics.BeginFrame(ClearColor);
        const bool gpuTimingActive = measuredFrame
            && gpuTimer.BeginFrame(graphics.Context(), timing.frameIndex);
        RenderStatistics statistics;
        if (useAssetScene)
        {
            statistics = assetRenderer.Render(
                graphics.Context(),
                AssetSceneFrame{
                    timing.frameIndex,
                    timing.totalSeconds,
                    static_cast<float>(options.width) / static_cast<float>(options.height)});
            if (options.benchmark.has_value())
            {
                if (statistics.objectCount != ExpectedStressObjects
                    || statistics.drawCalls < statistics.objectCount
                    || statistics.triangleCount <= statistics.drawCalls)
                {
                    throw std::runtime_error{"stress scene render statistics are incomplete"};
                }
            }
        }
        else
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
            samples.push_back(benchmark::FrameSample{
                timing.frameIndex,
                cpuMilliseconds,
                std::nullopt,
                statistics.drawCalls,
                statistics.triangleCount,
                statistics.objectCount,
                GetCurrentProcessWorkingSetBytes()});
        }
        if (options.benchmark.has_value())
        {
            const auto ready = gpuTimer.ResolveReady(graphics.Context());
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
        ApplyGpuResults(samples, drained, firstMeasuredFrame);
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
                options.benchmark->startedAt},
            samples);
    }

    return 0;
}
} // namespace dxa::engine
