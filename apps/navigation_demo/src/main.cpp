#include <dxa/engine/FrameClock.hpp>
#include <dxa/engine/GroundPlanePicking.hpp>
#include <dxa/engine/GraphicsDevice.hpp>
#include <dxa/engine/HybridDeferredRenderer.hpp>
#include <dxa/engine/InputState.hpp>
#include <dxa/engine/Window.hpp>
#include <dxa/engine/benchmark/StressScene.hpp>
#include <dxa/simulation/NavAgent.hpp>
#include <dxa/simulation/NavMesh.hpp>

#include <Windows.h>

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
constexpr std::uint32_t SceneSeed = 20260823U;
constexpr float FixedSimulationDelta = 1.0F / 60.0F;
constexpr std::array ClearColor{0.025F, 0.035F, 0.060F, 1.0F};

struct DemoOptions
{
    bool warp = false;
    bool hidden = false;
    bool verifyRender = false;
    std::uint32_t frameLimit = 0;
    std::optional<dxa::simulation::Vec2> autoDestination;
};

[[nodiscard]] std::uint32_t ParsePositiveInteger(const std::string_view value)
{
    std::uint32_t parsed = 0;
    const auto [end, error] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed);
    if (error != std::errc{} || end != value.data() + value.size() || parsed == 0)
    {
        throw std::invalid_argument{"frame count must be a positive integer"};
    }
    return parsed;
}

[[nodiscard]] float ParseFiniteFloat(const std::string_view value)
{
    float parsed = 0.0F;
    const auto [end, error] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed);
    if (error != std::errc{}
        || end != value.data() + value.size()
        || !std::isfinite(parsed))
    {
        throw std::invalid_argument{"destination coordinates must be finite numbers"};
    }
    return parsed;
}

[[nodiscard]] DemoOptions ParseOptions(
    const int argc,
    const char* const* const argv)
{
    DemoOptions options;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--warp")
        {
            options.warp = true;
        }
        else if (argument == "--hidden")
        {
            options.hidden = true;
        }
        else if (argument == "--verify-render")
        {
            options.verifyRender = true;
        }
        else if (argument == "--frames")
        {
            if (++index >= argc)
            {
                throw std::invalid_argument{"--frames requires one value"};
            }
            options.frameLimit = ParsePositiveInteger(argv[index]);
        }
        else if (argument == "--auto-destination")
        {
            if (index + 2 >= argc)
            {
                throw std::invalid_argument{"--auto-destination requires X and Z"};
            }
            const float x = ParseFiniteFloat(argv[++index]);
            const float z = ParseFiniteFloat(argv[++index]);
            options.autoDestination = dxa::simulation::Vec2{x, z};
        }
        else
        {
            throw std::invalid_argument{"unknown navigation demo argument"};
        }
    }

    if (options.hidden && options.frameLimit == 0)
    {
        throw std::invalid_argument{"--hidden requires --frames"};
    }
    if (options.autoDestination.has_value() && options.frameLimit == 0)
    {
        throw std::invalid_argument{"--auto-destination requires --frames"};
    }
    if (options.verifyRender && options.frameLimit == 0)
    {
        throw std::invalid_argument{"--verify-render requires --frames"};
    }
    return options;
}

[[nodiscard]] std::filesystem::path ExecutableDirectory()
{
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr,
        path.data(),
        static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size())
    {
        throw std::runtime_error{"GetModuleFileNameW failed"};
    }
    path.resize(length);
    return std::filesystem::path{path}.parent_path();
}

[[nodiscard]] dxa::simulation::NavMesh MakeArenaNavMesh()
{
    using dxa::simulation::NavMesh;
    using dxa::simulation::NavTriangleIndices;
    return NavMesh::Build(
        {
            {-32.0F, -32.0F},
            {32.0F, -32.0F},
            {-32.0F, 32.0F},
            {32.0F, 32.0F}
        },
        {
            NavTriangleIndices{{0, 1, 2}},
            NavTriangleIndices{{1, 3, 2}}
        },
        4.0F);
}

int RunDemo(const DemoOptions& options)
{
    const std::uint32_t width = options.hidden ? 320U : 1280U;
    const std::uint32_t height = options.hidden ? 180U : 720U;
    dxa::engine::InputState input;
    dxa::engine::Window window;
    window.Create(
        dxa::engine::WindowConfig{
            L"DX11 Survival Arena Navigation",
            width,
            height,
            options.hidden},
        input);

    dxa::engine::GraphicsDevice graphics;
    graphics.Initialize(dxa::engine::GraphicsDeviceConfig{
        window.NativeHandle(),
        width,
        height,
        options.warp
            ? dxa::engine::GraphicsDriver::Warp
            : dxa::engine::GraphicsDriver::Hardware,
#if defined(_DEBUG)
        true
#else
        false
#endif
    });

    const std::filesystem::path executableDirectory = ExecutableDirectory();
    dxa::engine::HybridDeferredRenderer renderer;
    renderer.Initialize(
        graphics.Device(),
        dxa::engine::HybridDeferredConfig{
            width,
            height,
            options.warp ? 128U : 2048U,
            SceneSeed,
            executableDirectory / L"shaders",
            executableDirectory / L"assets"});

    const dxa::engine::benchmark::StressScene scene =
        dxa::engine::benchmark::GenerateStressScene(SceneSeed);
    if (scene.players.empty())
    {
        throw std::runtime_error{"navigation scene has no controlled player"};
    }
    const dxa::simulation::Vec2 initialPosition{
        scene.players.front().position.x,
        scene.players.front().position.z};
    const dxa::simulation::NavMesh navMesh = MakeArenaNavMesh();
    dxa::simulation::NavAgent agent{
        navMesh,
        initialPosition,
        8.0F,
        0.05F};
    renderer.SetControlledPlayerPosition({
        initialPosition.x,
        0.0F,
        initialPosition.z});

    bool autoDestinationSubmitted = false;
    bool renderVerified = !options.verifyRender;
    bool moved = false;
    dxa::engine::FrameClock clock{dxa::engine::FrameClock::Clock::now()};
    while (true)
    {
        input.BeginFrame();
        if (!window.PumpMessages())
        {
            break;
        }

        const dxa::engine::FrameTiming timing = clock.Tick(
            dxa::engine::FrameClock::Clock::now());
        if (options.autoDestination.has_value() && !autoDestinationSubmitted)
        {
            if (!navMesh.FindContainingTriangleGrid(*options.autoDestination)
                     .triangle.has_value()
                || !agent.SetDestination(*options.autoDestination))
            {
                throw std::runtime_error{"automatic destination is outside the NavMesh"};
            }
            autoDestinationSubmitted = true;
        }
        if (input.WasRightPointerPressed())
        {
            const auto ground = dxa::engine::PointerGroundDestination(
                input.Pointer(),
                width,
                height,
                dxa::engine::benchmark::SampleStressCamera(timing.frameIndex));
            if (ground.has_value())
            {
                const dxa::simulation::Vec2 destination{
                    ground->x,
                    ground->z};
                if (navMesh.FindContainingTriangleGrid(destination)
                        .triangle.has_value())
                {
                    static_cast<void>(agent.SetDestination(destination));
                }
            }
        }

        const float deltaSeconds = options.autoDestination.has_value()
            ? FixedSimulationDelta
            : static_cast<float>(timing.deltaSeconds);
        agent.Tick(deltaSeconds);
        if (agent.State() == dxa::simulation::NavAgentState::InvalidDestination
            || !navMesh.FindContainingTriangleGrid(agent.Position()).triangle.has_value())
        {
            throw std::runtime_error{"controlled player left the NavMesh"};
        }
        moved = moved || dxa::simulation::Distance(initialPosition, agent.Position()) > 0.001F;
        renderer.SetControlledPlayerPosition({
            agent.Position().x,
            0.0F,
            agent.Position().z});

        graphics.BeginFrame(ClearColor);
        const dxa::engine::RenderStatistics statistics = renderer.Render(
            graphics.Context(),
            graphics.BackBufferRenderTargetView(),
            dxa::engine::AssetSceneFrame{
                timing.frameIndex,
                options.autoDestination.has_value()
                    ? static_cast<double>(timing.frameIndex - 1U)
                        * static_cast<double>(FixedSimulationDelta)
                    : timing.totalSeconds,
                static_cast<float>(width) / static_cast<float>(height)});
        if (statistics.objectCount
                != static_cast<std::uint32_t>(
                    dxa::engine::benchmark::PlayerCount
                    + dxa::engine::benchmark::AiCount
                    + dxa::engine::benchmark::StaticInstanceCount)
            || statistics.gBufferDrawCalls == 0
            || statistics.lightingDrawCalls != 1
            || statistics.transparentDrawCalls != 1)
        {
            throw std::runtime_error{"navigation render statistics are incomplete"};
        }

        if (options.verifyRender && timing.frameIndex == options.frameLimit)
        {
            if (!graphics.BackBufferContainsNonClearPixel(ClearColor))
            {
                throw std::runtime_error{"navigation render contains only the clear color"};
            }
            renderVerified = true;
        }
        graphics.EndFrame(false);

        if (options.frameLimit != 0 && timing.frameIndex >= options.frameLimit)
        {
            break;
        }
    }

    if (!renderVerified)
    {
        throw std::runtime_error{"navigation demo ended before render verification"};
    }
    if (options.autoDestination.has_value() && !moved)
    {
        throw std::runtime_error{"automatic navigation did not move the controlled player"};
    }

    std::cout << "navigation demo complete: moved=" << std::boolalpha << moved
              << ", x=" << agent.Position().x
              << ", z=" << agent.Position().z << '\n';
    return 0;
}
} // namespace

int main(const int argc, const char* const* const argv)
{
    try
    {
        return RunDemo(ParseOptions(argc, argv));
    }
    catch (const std::exception& error)
    {
        std::cerr << "navigation demo failed: " << error.what() << '\n';
        return 1;
    }
}
