#include <dxa/engine/EngineApp.hpp>

#include <dxa/engine/ForwardRenderer.hpp>
#include <dxa/engine/FrameClock.hpp>
#include <dxa/engine/GraphicsDevice.hpp>
#include <dxa/engine/InputState.hpp>
#include <dxa/engine/Window.hpp>

#include <array>
#include <chrono>

namespace dxa::engine
{
int EngineApp::Run(
    const EngineRunOptions& options,
    const std::filesystem::path& shaderPath) const
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

    ForwardRenderer renderer;
    renderer.Initialize(graphics.Device(), shaderPath);

    constexpr std::array ClearColor{0.025F, 0.035F, 0.060F, 1.0F};
    FrameClock clock{FrameClock::Clock::now()};

    while (true)
    {
        input.BeginFrame();
        if (!window.PumpMessages())
        {
            break;
        }

        const FrameTiming timing = clock.Tick(FrameClock::Clock::now());
        graphics.BeginFrame(ClearColor);
        renderer.Render(
            graphics.Context(),
            timing.totalSeconds,
            static_cast<float>(options.width) / static_cast<float>(options.height));
        graphics.EndFrame(options.vsync);

        if (options.frameLimit != 0 && timing.frameIndex >= options.frameLimit)
        {
            break;
        }
    }

    return 0;
}
} // namespace dxa::engine

