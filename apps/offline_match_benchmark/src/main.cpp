#include <dxa/offline_match_benchmark/BenchmarkOptions.hpp>

#include <dxa/simulation/ArenaMap.hpp>
#include <dxa/simulation/MatchConfig.hpp>
#include <dxa/simulation/OfflineBotController.hpp>
#include <dxa/simulation/OfflineMatch.hpp>
#include <dxa/simulation/SafeZone.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

struct MatchSummary
{
    dxa::simulation::ActorId winner = 0;
    dxa::simulation::MatchEndReason reason =
        dxa::simulation::MatchEndReason::LastSurvivor;
    std::uint32_t finishedTick = 0;
    std::uint32_t aliveContenders = 0;
    std::uint64_t eventChecksum = 0;
    bool allValuesFinite = false;

    [[nodiscard]] bool operator==(const MatchSummary&) const = default;
};

struct TickSample
{
    std::uint32_t tick = 0;
    double elapsedMilliseconds = 0.0;
    std::uint32_t aliveContenders = 0;
    std::uint32_t aliveNeutrals = 0;
    std::uint32_t eventCount = 0;
};

struct MatchRun
{
    MatchSummary summary;
    std::vector<TickSample> samples;
};

struct TickStatistics
{
    double p50 = 0.0;
    double p95 = 0.0;
    double maximum = 0.0;
};

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
        throw std::logic_error{"benchmark snapshot is missing an actor"};
    }
    return *actor;
}

void SubmitControlledBotCommand(
    dxa::simulation::OfflineMatch& match,
    const dxa::simulation::MatchSnapshot& snapshot,
    const dxa::simulation::MatchConfig& config)
{
    const dxa::simulation::SafeZoneState zone{
        snapshot.safeZoneStage,
        snapshot.safeZoneCenter,
        snapshot.safeZoneRadius,
        dxa::simulation::EvaluateSafeZone(snapshot.tick, config.tickRate)
            .damagePerSecond};
    const dxa::simulation::BotDecision decision = dxa::simulation::DecideContender(
        FindActor(snapshot, 0U),
        dxa::simulation::BotPerception{snapshot.actors, snapshot.loot, zone},
        config);
    if (decision.command.moveDestination.has_value()
        || decision.command.attackTarget.has_value())
    {
        match.Submit(decision.command);
    }
}

[[nodiscard]] std::uint32_t CountAliveNeutrals(
    const dxa::simulation::MatchSnapshot& snapshot)
{
    return static_cast<std::uint32_t>(std::count_if(
        snapshot.actors.begin(),
        snapshot.actors.end(),
        [](const dxa::simulation::ActorSnapshot& actor) {
            return actor.role == dxa::simulation::ActorRole::Neutral && actor.alive;
        }));
}

[[nodiscard]] MatchSummary Summarize(
    const dxa::simulation::MatchSnapshot& snapshot)
{
    if (!snapshot.result.has_value())
    {
        throw std::logic_error{"benchmark match has no result"};
    }
    bool allValuesFinite = std::isfinite(snapshot.elapsedSeconds)
        && std::isfinite(snapshot.safeZoneRadius);
    for (const dxa::simulation::ActorSnapshot& actor : snapshot.actors)
    {
        allValuesFinite = allValuesFinite
            && std::isfinite(actor.position.x)
            && std::isfinite(actor.position.z)
            && actor.health >= 0
            && actor.health <= 100;
    }
    return MatchSummary{
        snapshot.result->winner,
        snapshot.result->reason,
        snapshot.result->finishedTick,
        snapshot.aliveContenders,
        snapshot.eventChecksum,
        allValuesFinite};
}

[[nodiscard]] MatchRun RunMatch(const std::uint32_t seed, const bool measureTicks)
{
    dxa::simulation::MatchConfig config = dxa::simulation::DefaultMatchConfig();
    config.seed = seed;
    const dxa::simulation::NavMesh navMesh =
        dxa::simulation::BuildSurvivalArenaNavMesh();
    dxa::simulation::OfflineMatch match = dxa::simulation::OfflineMatch::Create(
        navMesh,
        config);
    match.Start();

    MatchRun run;
    if (measureTicks)
    {
        run.samples.reserve(config.hardTimeoutTick);
    }
    while (match.Snapshot().phase != dxa::simulation::MatchPhase::Finished)
    {
        const dxa::simulation::MatchSnapshot before = match.Snapshot();
        if (before.tick % config.botDecisionIntervalTicks == 0U)
        {
            SubmitControlledBotCommand(match, before, config);
        }

        const Clock::time_point started = Clock::now();
        match.Step();
        const Clock::time_point finished = Clock::now();
        const std::vector<dxa::simulation::MatchEvent> events = match.DrainEvents();
        const dxa::simulation::MatchSnapshot after = match.Snapshot();
        if (measureTicks)
        {
            const double elapsedMilliseconds =
                std::chrono::duration<double, std::milli>(finished - started).count();
            if (!std::isfinite(elapsedMilliseconds) || elapsedMilliseconds < 0.0)
            {
                throw std::runtime_error{"benchmark tick duration is invalid"};
            }
            run.samples.push_back(TickSample{
                after.tick,
                elapsedMilliseconds,
                after.aliveContenders,
                CountAliveNeutrals(after),
                static_cast<std::uint32_t>(events.size())});
        }
    }
    run.summary = Summarize(match.Snapshot());
    return run;
}

[[nodiscard]] double NearestRank(
    const std::vector<double>& sortedSamples,
    const double percentile)
{
    if (sortedSamples.empty())
    {
        throw std::invalid_argument{"percentile requires samples"};
    }
    const double rank = std::ceil(
        percentile * static_cast<double>(sortedSamples.size()));
    const std::size_t index = static_cast<std::size_t>(std::max(1.0, rank)) - 1U;
    return sortedSamples.at(index);
}

[[nodiscard]] TickStatistics CalculateTickStatistics(
    const std::vector<TickSample>& samples)
{
    std::vector<double> durations;
    durations.reserve(samples.size());
    for (const TickSample& sample : samples)
    {
        durations.push_back(sample.elapsedMilliseconds);
    }
    std::sort(durations.begin(), durations.end());
    return TickStatistics{
        NearestRank(durations, 0.50),
        NearestRank(durations, 0.95),
        durations.back()};
}

[[nodiscard]] std::string CompilerId()
{
#if defined(_MSC_VER)
    return "MSVC";
#elif defined(__clang__)
    return "Clang";
#elif defined(__GNUC__)
    return "GCC";
#else
    return "Unknown";
#endif
}

[[nodiscard]] std::string CompilerVersion()
{
#if defined(_MSC_FULL_VER)
    return std::to_string(_MSC_FULL_VER);
#elif defined(__clang_version__)
    return __clang_version__;
#elif defined(__VERSION__)
    return __VERSION__;
#else
    return "unknown";
#endif
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

[[nodiscard]] std::string EscapeJson(const std::string_view value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value)
    {
        switch (character)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += character;
            break;
        }
    }
    return escaped;
}

void WriteOutputs(
    const std::filesystem::path& outputDirectory,
    const dxa::offline_match_benchmark::OfflineMatchBenchmarkOptions& options,
    const MatchRun& measured,
    const TickStatistics& statistics)
{
    if (!outputDirectory.parent_path().empty())
    {
        std::filesystem::create_directories(outputDirectory.parent_path());
    }
    if (!std::filesystem::create_directory(outputDirectory))
    {
        throw std::runtime_error{"offline match benchmark output directory already exists"};
    }

    std::ofstream ticksFile{outputDirectory / "ticks.csv"};
    if (!ticksFile)
    {
        throw std::runtime_error{"failed to create offline match tick CSV"};
    }
    ticksFile.imbue(std::locale::classic());
    ticksFile << "tick,elapsed_ms,alive_contenders,alive_neutrals,event_count\n";
    ticksFile << std::setprecision(9);
    for (const TickSample& sample : measured.samples)
    {
        ticksFile << sample.tick << ','
                  << sample.elapsedMilliseconds << ','
                  << sample.aliveContenders << ','
                  << sample.aliveNeutrals << ','
                  << sample.eventCount << '\n';
    }
    if (!ticksFile)
    {
        throw std::runtime_error{"failed to write offline match tick CSV"};
    }

    const std::uint32_t logicalProcessors = std::max(
        1U,
        std::thread::hardware_concurrency());
    std::ofstream resultFile{outputDirectory / "result.json"};
    if (!resultFile)
    {
        throw std::runtime_error{"failed to create offline match result JSON"};
    }
    resultFile.imbue(std::locale::classic());
    resultFile << std::setprecision(9)
               << "{\n"
               << "  \"schema_version\": 1,\n"
               << "  \"commit_sha\": \"" << EscapeJson(options.commitSha) << "\",\n"
               << "  \"seed\": " << options.seed << ",\n"
               << "  \"winner\": " << measured.summary.winner << ",\n"
               << "  \"end_reason\": \"" << EndReasonName(measured.summary.reason) << "\",\n"
               << "  \"finished_tick\": " << measured.summary.finishedTick << ",\n"
               << "  \"event_checksum\": \"" << measured.summary.eventChecksum << "\",\n"
               << "  \"repeat_mismatch_count\": 0,\n"
               << "  \"tick_ms\": {\"p50\": " << statistics.p50
               << ", \"p95\": " << statistics.p95
               << ", \"max\": " << statistics.maximum << "},\n"
               << "  \"population\": {\"contenders\": 24, \"neutrals\": 100},\n"
               << "  \"compiler\": {\"id\": \"" << EscapeJson(CompilerId())
               << "\", \"version\": \"" << EscapeJson(CompilerVersion()) << "\"},\n"
               << "  \"cpu\": {\"logical_processors\": " << logicalProcessors << "}\n"
               << "}\n";
    if (!resultFile)
    {
        throw std::runtime_error{"failed to write offline match result JSON"};
    }
}

int RunBenchmark(
    const dxa::offline_match_benchmark::OfflineMatchBenchmarkOptions& options)
{
    const std::filesystem::path outputDirectory{options.outputDirectory};
    if (std::filesystem::exists(outputDirectory))
    {
        throw std::runtime_error{"offline match benchmark output directory already exists"};
    }

    const MatchRun first = RunMatch(options.seed, false);
    const MatchRun repeated = RunMatch(options.seed, false);
    if (first.summary != repeated.summary)
    {
        std::cerr << "offline match validation mismatch\n";
        return 3;
    }

    const MatchRun measured = RunMatch(options.seed, true);
    if (first.summary != measured.summary
        || measured.samples.size() != measured.summary.finishedTick
        || !measured.summary.allValuesFinite)
    {
        std::cerr << "offline match measurement mismatch\n";
        return 3;
    }

    const TickStatistics statistics = CalculateTickStatistics(measured.samples);
    WriteOutputs(outputDirectory, options, measured, statistics);
    std::cout << "offline match benchmark complete: tick="
              << measured.summary.finishedTick
              << ", winner=" << measured.summary.winner
              << ", p95_ms=" << statistics.p95 << '\n';
    return 0;
}
} // namespace

int main(const int argc, const char* const* const argv)
{
    try
    {
        std::vector<std::string_view> arguments;
        arguments.reserve(static_cast<std::size_t>(std::max(0, argc - 1)));
        for (int index = 1; index < argc; ++index)
        {
            arguments.emplace_back(argv[index]);
        }
        const auto parsed =
            dxa::offline_match_benchmark::ParseOfflineMatchBenchmarkOptions(arguments);
        if (!parsed.options.has_value())
        {
            std::cerr << parsed.error << '\n';
            return 2;
        }
        return RunBenchmark(*parsed.options);
    }
    catch (const std::exception& error)
    {
        std::cerr << "offline match benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
