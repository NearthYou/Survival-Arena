#include <dxa/engine/FrameClock.hpp>
#include <dxa/engine/GroundPlanePicking.hpp>
#include <dxa/engine/GraphicsDevice.hpp>
#include <dxa/engine/HybridDeferredRenderer.hpp>
#include <dxa/engine/InputState.hpp>
#include <dxa/engine/Window.hpp>
#include <dxa/engine/benchmark/StressScene.hpp>
#include <dxa/simulation/ArenaMap.hpp>
#include <dxa/simulation/Combat.hpp>
#include <dxa/simulation/MatchConfig.hpp>
#include <dxa/simulation/OfflineBotController.hpp>
#include <dxa/simulation/OfflineMatch.hpp>
#include <dxa/simulation/SafeZone.hpp>

#include <Windows.h>

#include <d3d11sdklayers.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
constexpr double FixedTickSeconds = 1.0 / 30.0;
constexpr std::uint32_t MaximumCatchUpTicks = 5U;
constexpr std::array ClearColor{0.025F, 0.035F, 0.060F, 1.0F};

struct DemoOptions
{
    bool warp = false;
    bool hidden = false;
    bool autoMatch = false;
    bool verifyMatch = false;
    std::uint32_t seed = 20260823U;
};

struct SceneBatches
{
    std::vector<dxa::engine::SceneCharacterState> players;
    std::vector<dxa::engine::SceneCharacterState> ai;
};

struct RenderResult
{
    dxa::engine::RenderStatistics statistics;
    bool containsRenderedPixel = false;
};

[[nodiscard]] std::uint32_t ParseSeed(const std::string_view value)
{
    std::uint32_t parsed = 0;
    const auto [end, error] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed);
    if (error != std::errc{} || end != value.data() + value.size())
    {
        throw std::invalid_argument{"seed must be an unsigned integer"};
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
        else if (argument == "--auto-match")
        {
            options.autoMatch = true;
        }
        else if (argument == "--verify-match")
        {
            options.verifyMatch = true;
        }
        else if (argument == "--seed")
        {
            if (++index >= argc)
            {
                throw std::invalid_argument{"--seed requires one value"};
            }
            options.seed = ParseSeed(argv[index]);
        }
        else
        {
            throw std::invalid_argument{"unknown offline match demo argument"};
        }
    }

    if (options.hidden && !options.autoMatch)
    {
        throw std::invalid_argument{"--hidden requires --auto-match"};
    }
    if (options.verifyMatch && !options.autoMatch)
    {
        throw std::invalid_argument{"--verify-match requires --auto-match"};
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

[[nodiscard]] const dxa::simulation::ActorSnapshot& FindActor(
    const dxa::simulation::MatchSnapshot& snapshot,
    const dxa::simulation::ActorId id)
{
    const auto actor = std::lower_bound(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        id,
        [](const dxa::simulation::ActorSnapshot& candidate,
           const dxa::simulation::ActorId value) {
            return candidate.id < value;
        });
    if (actor == snapshot.actors.end() || actor->id != id)
    {
        throw std::logic_error{"offline match snapshot is missing an actor"};
    }
    return *actor;
}

[[nodiscard]] dxa::simulation::SafeZoneState SnapshotZone(
    const dxa::simulation::MatchSnapshot& snapshot,
    const dxa::simulation::MatchConfig& config)
{
    return dxa::simulation::SafeZoneState{
        snapshot.safeZoneStage,
        snapshot.safeZoneCenter,
        snapshot.safeZoneRadius,
        dxa::simulation::EvaluateSafeZone(snapshot.tick, config.tickRate)
            .damagePerSecond};
}

void SubmitAutoControlledCommand(
    dxa::simulation::OfflineMatch& match,
    const dxa::simulation::MatchSnapshot& snapshot,
    const dxa::simulation::MatchConfig& config)
{
    const dxa::simulation::BotDecision decision = dxa::simulation::DecideContender(
        FindActor(snapshot, 0U),
        dxa::simulation::BotPerception{
            snapshot.actors,
            snapshot.loot,
            SnapshotZone(snapshot, config)},
        config);
    if (decision.command.moveDestination.has_value()
        || decision.command.attackTarget.has_value())
    {
        match.Submit(decision.command);
    }
}

[[nodiscard]] std::optional<dxa::simulation::ActorId> NearestAttackTarget(
    const dxa::simulation::MatchSnapshot& snapshot)
{
    const dxa::simulation::ActorSnapshot& controlled = FindActor(snapshot, 0U);
    if (!controlled.alive || controlled.cooldownTicksRemaining != 0U)
    {
        return std::nullopt;
    }

    const float range = dxa::simulation::WeaponDefinitionFor(controlled.weapon).range;
    const double rangeSquared = static_cast<double>(range) * range;
    const dxa::simulation::ActorSnapshot* closest = nullptr;
    double closestDistance = std::numeric_limits<double>::infinity();
    for (const dxa::simulation::ActorSnapshot& candidate : snapshot.actors)
    {
        if (!candidate.alive || candidate.id == controlled.id)
        {
            continue;
        }
        const double deltaX = static_cast<double>(candidate.position.x)
            - controlled.position.x;
        const double deltaZ = static_cast<double>(candidate.position.z)
            - controlled.position.z;
        const double distance = deltaX * deltaX + deltaZ * deltaZ;
        if (distance > rangeSquared)
        {
            continue;
        }
        if (closest == nullptr
            || distance < closestDistance
            || (distance == closestDistance && candidate.id < closest->id))
        {
            closest = &candidate;
            closestDistance = distance;
        }
    }
    return closest == nullptr
        ? std::nullopt
        : std::optional<dxa::simulation::ActorId>{closest->id};
}

[[nodiscard]] SceneBatches BuildSceneBatches(
    const dxa::simulation::MatchSnapshot& snapshot)
{
    SceneBatches batches;
    batches.players.reserve(dxa::engine::benchmark::PlayerCount);
    batches.ai.reserve(dxa::engine::benchmark::AiCount);
    for (const dxa::simulation::ActorSnapshot& actor : snapshot.actors)
    {
        dxa::engine::SceneCharacterState state{
            {actor.position.x, 0.0F, actor.position.z},
            actor.alive};
        if (actor.role == dxa::simulation::ActorRole::Contender)
        {
            batches.players.push_back(state);
        }
        else
        {
            batches.ai.push_back(state);
        }
    }
    if (batches.players.size() != dxa::engine::benchmark::PlayerCount
        || batches.ai.size() != dxa::engine::benchmark::AiCount)
    {
        throw std::logic_error{"offline match population does not fit renderer slots"};
    }
    return batches;
}

[[nodiscard]] std::wstring WeaponName(const dxa::simulation::WeaponType weapon)
{
    switch (weapon)
    {
    case dxa::simulation::WeaponType::Blade:
        return L"Blade";
    case dxa::simulation::WeaponType::Rifle:
        return L"Rifle";
    case dxa::simulation::WeaponType::ArcPulse:
        return L"ArcPulse";
    }
    throw std::logic_error{"unknown weapon type"};
}

[[nodiscard]] std::string EndReasonName(
    const dxa::simulation::MatchEndReason reason)
{
    switch (reason)
    {
    case dxa::simulation::MatchEndReason::LastSurvivor:
        return "last_survivor";
    case dxa::simulation::MatchEndReason::TimeLimit:
        return "time_limit";
    }
    throw std::logic_error{"unknown match end reason"};
}

[[nodiscard]] std::wstring BuildWindowTitle(
    const dxa::simulation::MatchSnapshot& snapshot)
{
    const dxa::simulation::ActorSnapshot& controlled = FindActor(snapshot, 0U);
    std::wostringstream title;
    title << L"DX11 Survival Arena | "
          << std::fixed << std::setprecision(1) << snapshot.elapsedSeconds
          << L"s | Alive " << snapshot.aliveContenders
          << L" | " << WeaponName(controlled.weapon);
    if (snapshot.result.has_value())
    {
        title << L" | Winner " << snapshot.result->winner;
    }
    return title.str();
}

[[nodiscard]] RenderResult RenderSnapshot(
    dxa::engine::HybridDeferredRenderer& renderer,
    dxa::engine::GraphicsDevice& graphics,
    const dxa::simulation::MatchSnapshot& snapshot,
    const std::uint64_t frameIndex,
    const std::uint32_t width,
    const std::uint32_t height,
    const bool verifyPixels)
{
    const SceneBatches batches = BuildSceneBatches(snapshot);
    renderer.SetPlayerStates(batches.players);
    renderer.SetAiStates(batches.ai);
    renderer.SetZoneRadius(snapshot.safeZoneRadius);

    graphics.BeginFrame(ClearColor);
    const dxa::engine::RenderStatistics statistics = renderer.Render(
        graphics.Context(),
        graphics.BackBufferRenderTargetView(),
        dxa::engine::AssetSceneFrame{
            frameIndex,
            snapshot.elapsedSeconds,
            static_cast<float>(width) / static_cast<float>(height)});
    const bool containsRenderedPixel = !verifyPixels
        || graphics.BackBufferContainsNonClearPixel(ClearColor);
    graphics.EndFrame(false);

    const std::uint32_t aliveActors = static_cast<std::uint32_t>(std::count_if(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        [](const dxa::simulation::ActorSnapshot& actor) { return actor.alive; }));
    const std::uint32_t expectedObjects = aliveActors
        + static_cast<std::uint32_t>(dxa::engine::benchmark::StaticInstanceCount);
    if (statistics.objectCount != expectedObjects
        || statistics.visibleObjectCount + statistics.culledObjectCount
            != statistics.objectCount
        || statistics.gBufferDrawCalls == 0U
        || statistics.lightingDrawCalls != 1U
        || statistics.transparentDrawCalls != 1U)
    {
        throw std::runtime_error{"offline match render statistics are incomplete"};
    }
    return RenderResult{statistics, containsRenderedPixel};
}

[[nodiscard]] std::vector<std::string> CollectDebugErrors(
    ID3D11InfoQueue* const infoQueue)
{
    std::vector<std::string> errors;
    if (infoQueue == nullptr)
    {
        return errors;
    }
    const std::uint64_t messageCount =
        infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
    for (std::uint64_t index = 0; index < messageCount; ++index)
    {
        SIZE_T messageSize = 0;
        if (FAILED(infoQueue->GetMessage(index, nullptr, &messageSize)))
        {
            errors.emplace_back("failed to read DX11 debug message size");
            continue;
        }
        auto storage = std::make_unique<std::byte[]>(messageSize);
        auto* const message = reinterpret_cast<D3D11_MESSAGE*>(storage.get());
        if (FAILED(infoQueue->GetMessage(index, message, &messageSize)))
        {
            errors.emplace_back("failed to read DX11 debug message");
            continue;
        }
        if (message->Severity == D3D11_MESSAGE_SEVERITY_CORRUPTION
            || message->Severity == D3D11_MESSAGE_SEVERITY_ERROR)
        {
            errors.emplace_back(message->pDescription, message->DescriptionByteLength);
        }
    }
    return errors;
}

void VerifyCompletedMatch(
    const dxa::simulation::MatchSnapshot& snapshot,
    const std::uint64_t eventCount,
    const bool containsRenderedPixel,
    const std::vector<std::string>& debugErrors)
{
    if (!snapshot.result.has_value()
        || snapshot.result->finishedTick < 14400U
        || snapshot.result->finishedTick > 18000U
        || snapshot.aliveContenders != 1U
        || eventCount == 0U
        || snapshot.eventChecksum == 0U
        || !containsRenderedPixel
        || !debugErrors.empty())
    {
        throw std::runtime_error{"offline match verification failed"};
    }

    std::optional<dxa::simulation::ActorId> survivor;
    for (const dxa::simulation::ActorSnapshot& actor : snapshot.actors)
    {
        if (!std::isfinite(actor.position.x)
            || !std::isfinite(actor.position.z)
            || actor.health < 0
            || actor.health > 100)
        {
            throw std::runtime_error{"offline match actor state is invalid"};
        }
        if (actor.role == dxa::simulation::ActorRole::Contender && actor.alive)
        {
            survivor = actor.id;
        }
    }
    if (!survivor.has_value() || *survivor != snapshot.result->winner)
    {
        throw std::runtime_error{"offline match winner does not match the survivor"};
    }
}

int RunAutoMatch(
    dxa::simulation::OfflineMatch& match,
    const dxa::simulation::MatchConfig& config,
    dxa::engine::Window& window,
    dxa::engine::GraphicsDevice& graphics,
    dxa::engine::HybridDeferredRenderer& renderer,
    ID3D11InfoQueue* const infoQueue,
    const DemoOptions& options,
    const std::uint32_t width,
    const std::uint32_t height)
{
    const dxa::simulation::MatchSnapshot initial = match.Snapshot();
    window.SetTitle(BuildWindowTitle(initial));
    const RenderResult initialRender = RenderSnapshot(
        renderer,
        graphics,
        initial,
        1U,
        width,
        height,
        options.verifyMatch);

    std::uint64_t eventCount = 0;
    while (match.Snapshot().phase != dxa::simulation::MatchPhase::Finished)
    {
        const dxa::simulation::MatchSnapshot snapshot = match.Snapshot();
        if (snapshot.tick % config.botDecisionIntervalTicks == 0U)
        {
            SubmitAutoControlledCommand(match, snapshot, config);
        }
        match.Step();
        eventCount += match.DrainEvents().size();
    }

    const dxa::simulation::MatchSnapshot result = match.Snapshot();
    window.SetTitle(BuildWindowTitle(result));
    const RenderResult finalRender = RenderSnapshot(
        renderer,
        graphics,
        result,
        2U,
        width,
        height,
        options.verifyMatch);
    graphics.Context()->Flush();
    const std::vector<std::string> debugErrors = CollectDebugErrors(infoQueue);
    if (options.verifyMatch)
    {
        VerifyCompletedMatch(
            result,
            eventCount,
            initialRender.containsRenderedPixel && finalRender.containsRenderedPixel,
            debugErrors);
    }

    std::cout << "offline match complete: tick=" << result.tick
              << ", seconds=" << result.elapsedSeconds
              << ", winner=" << result.result->winner
              << ", reason=" << EndReasonName(result.result->reason)
              << ", checksum=" << result.eventChecksum << '\n';
    return 0;
}

int RunVisibleMatch(
    dxa::simulation::OfflineMatch& match,
    const dxa::simulation::NavMesh& navMesh,
    dxa::engine::InputState& input,
    dxa::engine::Window& window,
    dxa::engine::GraphicsDevice& graphics,
    dxa::engine::HybridDeferredRenderer& renderer,
    ID3D11InfoQueue* const infoQueue,
    const std::uint32_t width,
    const std::uint32_t height)
{
    double accumulator = 0.0;
    std::uint64_t eventCount = 0;
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
        dxa::simulation::MatchSnapshot snapshot = match.Snapshot();
        const bool active = snapshot.phase == dxa::simulation::MatchPhase::Running
            || snapshot.phase == dxa::simulation::MatchPhase::SuddenDeath;

        if (active && input.WasRightPointerPressed())
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
                    dxa::simulation::MatchCommand command;
                    command.actor = 0U;
                    command.moveDestination = destination;
                    match.Submit(command);
                }
            }
        }

        accumulator += timing.deltaSeconds;
        std::uint32_t catchUpTicks = 0;
        while (accumulator >= FixedTickSeconds
               && catchUpTicks < MaximumCatchUpTicks)
        {
            snapshot = match.Snapshot();
            if (snapshot.phase == dxa::simulation::MatchPhase::Finished)
            {
                accumulator = 0.0;
                break;
            }
            if (const auto target = NearestAttackTarget(snapshot); target.has_value())
            {
                dxa::simulation::MatchCommand command;
                command.actor = 0U;
                command.attackTarget = *target;
                match.Submit(command);
            }
            match.Step();
            eventCount += match.DrainEvents().size();
            accumulator -= FixedTickSeconds;
            ++catchUpTicks;
        }
        if (catchUpTicks == MaximumCatchUpTicks)
        {
            accumulator = std::min(
                accumulator,
                FixedTickSeconds * static_cast<double>(MaximumCatchUpTicks));
        }

        snapshot = match.Snapshot();
        window.SetTitle(BuildWindowTitle(snapshot));
        (void)RenderSnapshot(
            renderer,
            graphics,
            snapshot,
            timing.frameIndex,
            width,
            height,
            false);
    }

    graphics.Context()->Flush();
    const std::vector<std::string> debugErrors = CollectDebugErrors(infoQueue);
    if (!debugErrors.empty())
    {
        throw std::runtime_error{debugErrors.front()};
    }

    const dxa::simulation::MatchSnapshot result = match.Snapshot();
    if (result.result.has_value())
    {
        std::cout << "offline match complete: tick=" << result.tick
                  << ", seconds=" << result.elapsedSeconds
                  << ", winner=" << result.result->winner
                  << ", reason=" << EndReasonName(result.result->reason)
                  << ", checksum=" << result.eventChecksum
                  << ", events=" << eventCount << '\n';
    }
    else
    {
        std::cout << "offline match closed before completion: tick="
                  << result.tick << '\n';
    }
    return 0;
}

int RunDemo(const DemoOptions& options)
{
    const std::uint32_t width = options.hidden ? 320U : 1280U;
    const std::uint32_t height = options.hidden ? 180U : 720U;
    dxa::engine::InputState input;
    dxa::engine::Window window;
    window.Create(
        dxa::engine::WindowConfig{
            L"DX11 Survival Arena Offline Match",
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

    Microsoft::WRL::ComPtr<ID3D11InfoQueue> infoQueue;
    if (graphics.DebugLayerEnabled())
    {
        if (FAILED(graphics.Device()->QueryInterface(
                IID_PPV_ARGS(infoQueue.ReleaseAndGetAddressOf()))))
        {
            throw std::runtime_error{"failed to query DX11 info queue"};
        }
        infoQueue->ClearStoredMessages();
    }

    const std::filesystem::path executableDirectory = ExecutableDirectory();
    dxa::engine::HybridDeferredRenderer renderer;
    renderer.Initialize(
        graphics.Device(),
        dxa::engine::HybridDeferredConfig{
            width,
            height,
            options.warp ? 128U : 2048U,
            options.seed,
            executableDirectory / L"shaders",
            executableDirectory / L"assets"});

    dxa::simulation::MatchConfig config = dxa::simulation::DefaultMatchConfig();
    config.seed = options.seed;
    const dxa::simulation::NavMesh navMesh =
        dxa::simulation::BuildSurvivalArenaNavMesh();
    dxa::simulation::OfflineMatch match = dxa::simulation::OfflineMatch::Create(
        navMesh,
        config);
    match.Start();

    if (options.autoMatch)
    {
        return RunAutoMatch(
            match,
            config,
            window,
            graphics,
            renderer,
            infoQueue.Get(),
            options,
            width,
            height);
    }
    return RunVisibleMatch(
        match,
        navMesh,
        input,
        window,
        graphics,
        renderer,
        infoQueue.Get(),
        width,
        height);
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
        std::cerr << "offline match demo failed: " << error.what() << '\n';
        return 1;
    }
}
