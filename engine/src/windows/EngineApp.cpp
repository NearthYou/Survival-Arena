#include <dxa/engine/EngineApp.hpp>

#include <dxa/engine/AssetSceneRenderer.hpp>
#include <dxa/engine/ForwardRenderer.hpp>
#include <dxa/engine/FrameClock.hpp>
#include <dxa/engine/GraphicsDevice.hpp>
#include <dxa/engine/InputState.hpp>
#include <dxa/engine/Window.hpp>

#include <array>
#include <chrono>
#include <stdexcept>

namespace dxa::engine
{
int EngineApp::Run(
    const EngineRunOptions& options,
    const std::filesystem::path& shaderPath,
    const std::filesystem::path& assetRoot) const
{
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

    const bool useAssetScene = !assetRoot.empty();
    ForwardRenderer fallbackRenderer;
    AssetSceneRenderer assetRenderer;
    if (useAssetScene)
    {
        assetRenderer.Initialize(
            graphics.Device(),
            shaderPath.parent_path() / L"asset_scene.hlsl",
            assetRoot,
            AssetSceneConfig{options.stressSceneSeed});
    }
    else
    {
        fallbackRenderer.Initialize(graphics.Device(), shaderPath);
    }
    if (options.verifyAssetScene && (!useAssetScene || !assetRenderer.AssetSceneReady()))
    {
        throw std::runtime_error{"asset scene verification requirements were not met"};
    }
    if (options.stressSceneSeed.has_value() && !useAssetScene)
    {
        throw std::runtime_error{"stress scene requires runtime assets"};
    }

    constexpr std::array ClearColor{0.025F, 0.035F, 0.060F, 1.0F};
    FrameClock clock{FrameClock::Clock::now()};
    bool renderVerified = !options.verifyRender;

    while (true)
    {
        input.BeginFrame();
        if (!window.PumpMessages())
        {
            break;
        }

        const FrameTiming timing = clock.Tick(FrameClock::Clock::now());
        graphics.BeginFrame(ClearColor);
        if (useAssetScene)
        {
            const RenderStatistics statistics = assetRenderer.Render(
                graphics.Context(),
                AssetSceneFrame{
                    timing.frameIndex,
                    timing.totalSeconds,
                    static_cast<float>(options.width) / static_cast<float>(options.height)});
            if (options.stressSceneSeed.has_value())
            {
                constexpr std::uint32_t ExpectedObjects = static_cast<std::uint32_t>(
                    benchmark::PlayerCount
                    + benchmark::AiCount
                    + benchmark::StaticInstanceCount);
                if (statistics.objectCount != ExpectedObjects
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

        if (options.verifyRender && timing.frameIndex == options.frameLimit)
        {
            if (!graphics.BackBufferContainsNonClearPixel(ClearColor))
            {
                throw std::runtime_error("render verification found only the clear color");
            }
            renderVerified = true;
        }

        graphics.EndFrame(options.vsync);

        if (options.frameLimit != 0 && timing.frameIndex >= options.frameLimit)
        {
            break;
        }
    }

    if (!renderVerified)
    {
        throw std::runtime_error("render verification ended before the requested frame");
    }

    return 0;
}
} // namespace dxa::engine
