#include <dxa/simulation/AiDecision.hpp>
#include <dxa/simulation/NavMesh.hpp>
#include <dxa/simulation/SpatialIndex.hpp>
#include <dxa/simulation_benchmark/BenchmarkOptions.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
using dxa::simulation::Aabb2;
using dxa::simulation::AiArchetype;
using dxa::simulation::AiBlackboard;
using dxa::simulation::AiCommandType;
using dxa::simulation::BehaviorTreeAiController;
using dxa::simulation::FsmAiController;
using dxa::simulation::LinearSpatialIndex;
using dxa::simulation::LooseQuadtree;
using dxa::simulation::LooseQuadtreeConfig;
using dxa::simulation::NavMesh;
using dxa::simulation::NavQueryResult;
using dxa::simulation::NavTriangleIndices;
using dxa::simulation::SpatialEntity;
using dxa::simulation::SpatialQueryResult;
using dxa::simulation::Vec2;
using dxa::simulation_benchmark::SimulationBenchmarkOptions;

constexpr std::uint64_t FnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t FnvPrime = 1099511628211ULL;
constexpr std::uint32_t NavColumns = 64U;
constexpr std::uint32_t NavRows = 64U;
constexpr std::uint32_t SpatialEntityCount = 1124U;

enum class BenchmarkCase : std::size_t
{
    NavLinear,
    NavGrid,
    SpatialLinearAabb,
    SpatialQuadtreeAabb,
    SpatialLinearPick,
    SpatialQuadtreePick,
    AiFsm,
    AiBehaviorTree,
    Count
};

struct Evaluation
{
    std::uint64_t checksum = FnvOffset;
    std::uint64_t work = 0;
};

struct CaseResult
{
    std::string name;
    std::string workLabel;
    double medianMilliseconds = 0.0;
    std::vector<double> samplesMilliseconds;
    std::uint64_t work = 0;
    std::uint64_t checksum = FnvOffset;
};

struct AiWorkItem
{
    AiArchetype archetype = AiArchetype::Melee;
    AiBlackboard blackboard;
};

class DeterministicRandom
{
public:
    explicit DeterministicRandom(const std::uint32_t seed)
        : engine_{seed}
    {
    }

    [[nodiscard]] std::uint32_t Next()
    {
        static_assert(
            std::mt19937::max() <= std::numeric_limits<std::uint32_t>::max());
        return static_cast<std::uint32_t>(engine_());
    }

    [[nodiscard]] float Range(const float minimum, const float maximum)
    {
        constexpr float InverseRange = 1.0F / 16777216.0F;
        const float unit = static_cast<float>(Next() >> 8U) * InverseRange;
        return minimum + (maximum - minimum) * unit;
    }

private:
    std::mt19937 engine_;
};

void HashByte(std::uint64_t& hash, const std::uint8_t value) noexcept
{
    hash ^= value;
    hash *= FnvPrime;
}

void HashValue(std::uint64_t& hash, std::uint64_t value) noexcept
{
    for (std::uint32_t index = 0; index < 8U; ++index)
    {
        HashByte(hash, static_cast<std::uint8_t>(value & 0xFFU));
        value >>= 8U;
    }
}

void HashTriangle(
    std::uint64_t& hash,
    const std::optional<dxa::simulation::TriangleId> triangle) noexcept
{
    HashValue(
        hash,
        triangle.has_value()
            ? static_cast<std::uint64_t>(*triangle)
            : std::numeric_limits<std::uint64_t>::max());
}

void HashIds(
    std::uint64_t& hash,
    const std::vector<dxa::simulation::SpatialEntityId>& ids) noexcept
{
    HashValue(hash, static_cast<std::uint64_t>(ids.size()));
    for (const auto id : ids)
    {
        HashValue(hash, static_cast<std::uint64_t>(id));
    }
}

[[nodiscard]] std::string HexChecksum(const std::uint64_t checksum)
{
    std::ostringstream formatted;
    formatted << std::hex << std::setfill('0') << std::setw(16) << checksum;
    return formatted.str();
}

[[nodiscard]] NavMesh MakeGridNavMesh()
{
    std::vector<Vec2> vertices;
    vertices.reserve(
        static_cast<std::size_t>(NavColumns + 1U)
        * static_cast<std::size_t>(NavRows + 1U));
    for (std::uint32_t row = 0; row <= NavRows; ++row)
    {
        for (std::uint32_t column = 0; column <= NavColumns; ++column)
        {
            vertices.push_back({
                -32.0F + static_cast<float>(column),
                -32.0F + static_cast<float>(row)});
        }
    }

    const auto vertexId = [](const std::uint32_t column, const std::uint32_t row) {
        return row * (NavColumns + 1U) + column;
    };
    std::vector<NavTriangleIndices> triangles;
    triangles.reserve(
        static_cast<std::size_t>(NavColumns)
        * static_cast<std::size_t>(NavRows)
        * 2U);
    for (std::uint32_t row = 0; row < NavRows; ++row)
    {
        for (std::uint32_t column = 0; column < NavColumns; ++column)
        {
            const std::uint32_t lowerLeft = vertexId(column, row);
            const std::uint32_t lowerRight = vertexId(column + 1U, row);
            const std::uint32_t upperLeft = vertexId(column, row + 1U);
            const std::uint32_t upperRight = vertexId(column + 1U, row + 1U);
            triangles.push_back(NavTriangleIndices{{
                lowerLeft,
                lowerRight,
                upperLeft}});
            triangles.push_back(NavTriangleIndices{{
                lowerRight,
                upperRight,
                upperLeft}});
        }
    }
    return NavMesh::Build(std::move(vertices), std::move(triangles), 4.0F);
}

[[nodiscard]] std::vector<Vec2> GenerateNavQueries(
    const std::uint32_t count,
    DeterministicRandom& random)
{
    std::vector<Vec2> queries;
    queries.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        queries.push_back({
            random.Range(-36.0F, 36.0F),
            random.Range(-36.0F, 36.0F)});
    }
    return queries;
}

[[nodiscard]] std::vector<SpatialEntity> GenerateSpatialEntities(
    DeterministicRandom& random)
{
    std::vector<SpatialEntity> entities;
    entities.reserve(SpatialEntityCount);
    for (std::uint32_t index = 0; index < SpatialEntityCount; ++index)
    {
        const Vec2 center{
            random.Range(-60.0F, 60.0F),
            random.Range(-60.0F, 60.0F)};
        const float halfX = random.Range(0.05F, 1.5F);
        const float halfZ = random.Range(0.05F, 1.5F);
        entities.push_back(SpatialEntity{
            index + 1U,
            Aabb2::Create(
                {center.x - halfX, center.z - halfZ},
                {center.x + halfX, center.z + halfZ})});
    }
    return entities;
}

[[nodiscard]] std::vector<Aabb2> GenerateAabbQueries(
    const std::uint32_t count,
    DeterministicRandom& random)
{
    std::vector<Aabb2> queries;
    queries.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const Vec2 center{
            random.Range(-70.0F, 70.0F),
            random.Range(-70.0F, 70.0F)};
        const float halfX = random.Range(0.1F, 4.0F);
        const float halfZ = random.Range(0.1F, 4.0F);
        queries.push_back(Aabb2::Create(
            {center.x - halfX, center.z - halfZ},
            {center.x + halfX, center.z + halfZ}));
    }
    return queries;
}

[[nodiscard]] std::vector<Vec2> GeneratePickQueries(
    const std::uint32_t count,
    DeterministicRandom& random)
{
    std::vector<Vec2> queries;
    queries.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        queries.push_back({
            random.Range(-70.0F, 70.0F),
            random.Range(-70.0F, 70.0F)});
    }
    return queries;
}

[[nodiscard]] std::vector<AiWorkItem> GenerateAiWorkItems(
    const std::uint32_t count,
    DeterministicRandom& random)
{
    std::vector<AiWorkItem> items;
    items.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        AiBlackboard blackboard;
        blackboard.selfPosition = {
            random.Range(-50.0F, 50.0F),
            random.Range(-50.0F, 50.0F)};
        blackboard.targetPosition = {
            random.Range(-50.0F, 50.0F),
            random.Range(-50.0F, 50.0F)};
        blackboard.hasTarget = (random.Next() & 1U) != 0U;
        blackboard.cooldownReady = (random.Next() & 1U) != 0U;
        blackboard.attackRange = random.Range(0.5F, 12.0F);
        blackboard.preferredRange = random.Range(5.0F, 15.0F);
        blackboard.retreatRange = random.Range(0.5F, 6.0F);
        items.push_back(AiWorkItem{
            (random.Next() & 1U) == 0U
                ? AiArchetype::Melee
                : AiArchetype::Ranged,
            blackboard});
    }
    return items;
}

[[nodiscard]] Evaluation EvaluateNavLinear(
    const NavMesh& mesh,
    const std::vector<Vec2>& queries)
{
    Evaluation evaluation;
    for (const Vec2 point : queries)
    {
        const NavQueryResult result = mesh.FindContainingTriangleLinear(point);
        HashTriangle(evaluation.checksum, result.triangle);
        evaluation.work += result.candidatesTested;
    }
    return evaluation;
}

[[nodiscard]] Evaluation EvaluateNavGrid(
    const NavMesh& mesh,
    const std::vector<Vec2>& queries)
{
    Evaluation evaluation;
    for (const Vec2 point : queries)
    {
        const NavQueryResult result = mesh.FindContainingTriangleGrid(point);
        HashTriangle(evaluation.checksum, result.triangle);
        evaluation.work += result.candidatesTested;
    }
    return evaluation;
}

template <typename Index>
[[nodiscard]] Evaluation EvaluateAabbQueries(
    const Index& index,
    const std::vector<Aabb2>& queries)
{
    Evaluation evaluation;
    for (const Aabb2& query : queries)
    {
        const SpatialQueryResult result = index.QueryAabb(query);
        HashIds(evaluation.checksum, result.ids);
        evaluation.work += result.boundsTested;
    }
    return evaluation;
}

template <typename Index>
[[nodiscard]] Evaluation EvaluatePickQueries(
    const Index& index,
    const std::vector<Vec2>& queries)
{
    Evaluation evaluation;
    for (const Vec2 point : queries)
    {
        const SpatialQueryResult result = index.PickPoint(point);
        HashIds(evaluation.checksum, result.ids);
        evaluation.work += result.boundsTested;
    }
    return evaluation;
}

template <typename MeleeController, typename RangedController>
[[nodiscard]] Evaluation EvaluateAi(
    const std::vector<AiWorkItem>& items,
    const MeleeController& melee,
    const RangedController& ranged)
{
    Evaluation evaluation;
    for (const AiWorkItem& item : items)
    {
        const AiCommandType command = item.archetype == AiArchetype::Melee
            ? melee.Tick(item.blackboard)
            : ranged.Tick(item.blackboard);
        using CommandValue = std::underlying_type_t<AiCommandType>;
        HashValue(
            evaluation.checksum,
            static_cast<std::uint64_t>(static_cast<CommandValue>(command)));
        ++evaluation.work;
    }
    return evaluation;
}

template <typename Function>
[[nodiscard]] CaseResult MeasureCase(
    std::string name,
    std::string workLabel,
    const std::uint32_t sampleCount,
    const Evaluation expected,
    Function&& function)
{
    const Evaluation warmup = function();
    if (warmup.checksum != expected.checksum || warmup.work != expected.work)
    {
        throw std::runtime_error{"benchmark warmup result changed"};
    }

    std::vector<double> samples;
    samples.reserve(sampleCount);
    for (std::uint32_t sample = 0; sample < sampleCount; ++sample)
    {
        const auto start = std::chrono::steady_clock::now();
        const Evaluation measured = function();
        const auto finish = std::chrono::steady_clock::now();
        if (measured.checksum != expected.checksum || measured.work != expected.work)
        {
            throw std::runtime_error{"benchmark measured result changed"};
        }
        samples.push_back(
            std::chrono::duration<double, std::milli>(finish - start).count());
    }

    std::vector<double> ordered = samples;
    std::ranges::sort(ordered);
    return CaseResult{
        std::move(name),
        std::move(workLabel),
        ordered[ordered.size() / 2U],
        std::move(samples),
        expected.work,
        expected.checksum};
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
#elif defined(__VERSION__)
    return __VERSION__;
#else
    return "unknown";
#endif
}

[[nodiscard]] std::string OperatingSystem()
{
#if defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

[[nodiscard]] std::string BuildConfiguration()
{
#if defined(NDEBUG)
    return "Release";
#else
    return "Debug";
#endif
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
    const SimulationBenchmarkOptions& options,
    const std::size_t navTriangleCount,
    const std::vector<CaseResult>& cases,
    const std::uint64_t resultChecksum)
{
    if (!outputDirectory.parent_path().empty())
    {
        std::filesystem::create_directories(outputDirectory.parent_path());
    }
    if (!std::filesystem::create_directory(outputDirectory))
    {
        throw std::runtime_error{"simulation benchmark output directory already exists"};
    }

    std::ofstream samplesFile{outputDirectory / "samples.csv"};
    if (!samplesFile)
    {
        throw std::runtime_error{"failed to create simulation benchmark samples"};
    }
    samplesFile << "case,sample_index,elapsed_ms\n" << std::fixed
                << std::setprecision(6);
    for (const CaseResult& result : cases)
    {
        for (std::size_t index = 0; index < result.samplesMilliseconds.size(); ++index)
        {
            samplesFile << result.name << ',' << (index + 1U) << ','
                        << result.samplesMilliseconds[index] << '\n';
        }
    }
    samplesFile.close();
    if (!samplesFile)
    {
        throw std::runtime_error{"failed to write simulation benchmark samples"};
    }

    std::ofstream resultFile{outputDirectory / "result.json"};
    if (!resultFile)
    {
        throw std::runtime_error{"failed to create simulation benchmark result"};
    }
    resultFile << std::fixed << std::setprecision(6);
    resultFile << "{\n"
               << "  \"schema_version\": 1,\n"
               << "  \"seed\": " << options.seed << ",\n"
               << "  \"commit_sha\": \"" << EscapeJson(options.commitSha) << "\",\n"
               << "  \"build_configuration\": \"" << BuildConfiguration() << "\",\n"
               << "  \"compiler\": {\"id\": \"" << CompilerId()
               << "\", \"version\": \"" << EscapeJson(CompilerVersion()) << "\"},\n"
               << "  \"cpu\": {\"logical_processors\": "
               << std::thread::hardware_concurrency() << "},\n"
               << "  \"operating_system\": \"" << OperatingSystem() << "\",\n"
               << "  \"sample_count\": " << options.sampleCount << ",\n"
               << "  \"mismatch_count\": 0,\n"
               << "  \"result_checksum\": \"" << HexChecksum(resultChecksum) << "\",\n"
               << "  \"workload\": {\n"
               << "    \"navmesh_cells\": " << (NavColumns * NavRows) << ",\n"
               << "    \"navmesh_triangles\": " << navTriangleCount << ",\n"
               << "    \"nav_queries\": " << options.navQueryCount << ",\n"
               << "    \"spatial_entities\": " << SpatialEntityCount << ",\n"
               << "    \"aabb_queries\": " << options.aabbQueryCount << ",\n"
               << "    \"pick_queries\": " << options.pickQueryCount << ",\n"
               << "    \"ai_decisions\": " << options.aiDecisionCount << "\n"
               << "  },\n"
               << "  \"cases\": {\n";
    for (std::size_t caseIndex = 0; caseIndex < cases.size(); ++caseIndex)
    {
        const CaseResult& result = cases[caseIndex];
        resultFile << "    \"" << result.name << "\": {\n"
                   << "      \"median_ms\": " << result.medianMilliseconds << ",\n"
                   << "      \"" << result.workLabel << "\": " << result.work << ",\n"
                   << "      \"checksum\": \"" << HexChecksum(result.checksum) << "\",\n"
                   << "      \"samples_ms\": [";
        for (std::size_t sample = 0; sample < result.samplesMilliseconds.size(); ++sample)
        {
            if (sample != 0)
            {
                resultFile << ", ";
            }
            resultFile << result.samplesMilliseconds[sample];
        }
        resultFile << "]\n    }";
        resultFile << (caseIndex + 1U == cases.size() ? '\n' : ',') << '\n';
    }
    resultFile << "  }\n}\n";
    resultFile.close();
    if (!resultFile)
    {
        throw std::runtime_error{"failed to write simulation benchmark result"};
    }
}

[[nodiscard]] int RunBenchmark(const SimulationBenchmarkOptions& options)
{
    const std::filesystem::path outputDirectory{options.outputDirectory};
    if (std::filesystem::exists(outputDirectory))
    {
        throw std::runtime_error{"simulation benchmark output directory already exists"};
    }

    std::cout << "workload generation" << std::endl;
    DeterministicRandom random{options.seed};
    const NavMesh navMesh = MakeGridNavMesh();
    const std::vector<Vec2> navQueries = GenerateNavQueries(
        options.navQueryCount,
        random);
    const std::vector<SpatialEntity> entities = GenerateSpatialEntities(random);
    const std::vector<Aabb2> aabbQueries = GenerateAabbQueries(
        options.aabbQueryCount,
        random);
    const std::vector<Vec2> pickQueries = GeneratePickQueries(
        options.pickQueryCount,
        random);
    const std::vector<AiWorkItem> aiItems = GenerateAiWorkItems(
        options.aiDecisionCount,
        random);
    const LinearSpatialIndex linearIndex{entities};
    const LooseQuadtree quadtree{
        Aabb2::Create({-64.0F, -64.0F}, {64.0F, 64.0F}),
        entities,
        LooseQuadtreeConfig{1.5F, 8U, 6U}};
    const FsmAiController meleeFsm{AiArchetype::Melee};
    const FsmAiController rangedFsm{AiArchetype::Ranged};
    const BehaviorTreeAiController meleeTree{AiArchetype::Melee};
    const BehaviorTreeAiController rangedTree{AiArchetype::Ranged};

    std::cout << "equivalence validation" << std::endl;
    std::array<Evaluation, static_cast<std::size_t>(BenchmarkCase::Count)> expected;
    std::uint64_t mismatchCount = 0;
    for (const Vec2 point : navQueries)
    {
        const NavQueryResult linear = navMesh.FindContainingTriangleLinear(point);
        const NavQueryResult grid = navMesh.FindContainingTriangleGrid(point);
        HashTriangle(
            expected[static_cast<std::size_t>(BenchmarkCase::NavLinear)].checksum,
            linear.triangle);
        HashTriangle(
            expected[static_cast<std::size_t>(BenchmarkCase::NavGrid)].checksum,
            grid.triangle);
        expected[static_cast<std::size_t>(BenchmarkCase::NavLinear)].work
            += linear.candidatesTested;
        expected[static_cast<std::size_t>(BenchmarkCase::NavGrid)].work
            += grid.candidatesTested;
        mismatchCount += linear.triangle == grid.triangle ? 0U : 1U;
    }
    for (const Aabb2& query : aabbQueries)
    {
        const SpatialQueryResult linear = linearIndex.QueryAabb(query);
        const SpatialQueryResult tree = quadtree.QueryAabb(query);
        HashIds(
            expected[static_cast<std::size_t>(BenchmarkCase::SpatialLinearAabb)].checksum,
            linear.ids);
        HashIds(
            expected[static_cast<std::size_t>(BenchmarkCase::SpatialQuadtreeAabb)].checksum,
            tree.ids);
        expected[static_cast<std::size_t>(BenchmarkCase::SpatialLinearAabb)].work
            += linear.boundsTested;
        expected[static_cast<std::size_t>(BenchmarkCase::SpatialQuadtreeAabb)].work
            += tree.boundsTested;
        mismatchCount += linear.ids == tree.ids ? 0U : 1U;
    }
    for (const Vec2 point : pickQueries)
    {
        const SpatialQueryResult linear = linearIndex.PickPoint(point);
        const SpatialQueryResult tree = quadtree.PickPoint(point);
        HashIds(
            expected[static_cast<std::size_t>(BenchmarkCase::SpatialLinearPick)].checksum,
            linear.ids);
        HashIds(
            expected[static_cast<std::size_t>(BenchmarkCase::SpatialQuadtreePick)].checksum,
            tree.ids);
        expected[static_cast<std::size_t>(BenchmarkCase::SpatialLinearPick)].work
            += linear.boundsTested;
        expected[static_cast<std::size_t>(BenchmarkCase::SpatialQuadtreePick)].work
            += tree.boundsTested;
        mismatchCount += linear.ids == tree.ids ? 0U : 1U;
    }
    for (const AiWorkItem& item : aiItems)
    {
        const AiCommandType fsmCommand = item.archetype == AiArchetype::Melee
            ? meleeFsm.Tick(item.blackboard)
            : rangedFsm.Tick(item.blackboard);
        const AiCommandType treeCommand = item.archetype == AiArchetype::Melee
            ? meleeTree.Tick(item.blackboard)
            : rangedTree.Tick(item.blackboard);
        using CommandValue = std::underlying_type_t<AiCommandType>;
        HashValue(
            expected[static_cast<std::size_t>(BenchmarkCase::AiFsm)].checksum,
            static_cast<std::uint64_t>(static_cast<CommandValue>(fsmCommand)));
        HashValue(
            expected[static_cast<std::size_t>(BenchmarkCase::AiBehaviorTree)].checksum,
            static_cast<std::uint64_t>(static_cast<CommandValue>(treeCommand)));
        ++expected[static_cast<std::size_t>(BenchmarkCase::AiFsm)].work;
        ++expected[static_cast<std::size_t>(BenchmarkCase::AiBehaviorTree)].work;
        mismatchCount += fsmCommand == treeCommand ? 0U : 1U;
    }

    if (mismatchCount != 0)
    {
        std::cerr << "benchmark equivalence mismatch count: " << mismatchCount << '\n';
        return 3;
    }

    std::cout << "timed cases" << std::endl;
    std::vector<CaseResult> cases;
    cases.reserve(static_cast<std::size_t>(BenchmarkCase::Count));
    const auto measure = [&](
                             const BenchmarkCase benchmarkCase,
                             std::string name,
                             std::string workLabel,
                             auto&& function) {
        std::cout << "  " << name << std::endl;
        cases.push_back(MeasureCase(
            std::move(name),
            std::move(workLabel),
            options.sampleCount,
            expected[static_cast<std::size_t>(benchmarkCase)],
            std::forward<decltype(function)>(function)));
    };
    measure(
        BenchmarkCase::NavLinear,
        "nav_linear",
        "candidates",
        [&] { return EvaluateNavLinear(navMesh, navQueries); });
    measure(
        BenchmarkCase::NavGrid,
        "nav_grid",
        "candidates",
        [&] { return EvaluateNavGrid(navMesh, navQueries); });
    measure(
        BenchmarkCase::SpatialLinearAabb,
        "spatial_linear_aabb",
        "bounds_tested",
        [&] { return EvaluateAabbQueries(linearIndex, aabbQueries); });
    measure(
        BenchmarkCase::SpatialQuadtreeAabb,
        "spatial_quadtree_aabb",
        "bounds_tested",
        [&] { return EvaluateAabbQueries(quadtree, aabbQueries); });
    measure(
        BenchmarkCase::SpatialLinearPick,
        "spatial_linear_pick",
        "bounds_tested",
        [&] { return EvaluatePickQueries(linearIndex, pickQueries); });
    measure(
        BenchmarkCase::SpatialQuadtreePick,
        "spatial_quadtree_pick",
        "bounds_tested",
        [&] { return EvaluatePickQueries(quadtree, pickQueries); });
    measure(
        BenchmarkCase::AiFsm,
        "ai_fsm",
        "evaluations",
        [&] { return EvaluateAi(aiItems, meleeFsm, rangedFsm); });
    measure(
        BenchmarkCase::AiBehaviorTree,
        "ai_behavior_tree",
        "evaluations",
        [&] { return EvaluateAi(aiItems, meleeTree, rangedTree); });

    std::uint64_t resultChecksum = FnvOffset;
    HashValue(resultChecksum, options.seed);
    HashValue(resultChecksum, options.navQueryCount);
    HashValue(resultChecksum, options.aabbQueryCount);
    HashValue(resultChecksum, options.pickQueryCount);
    HashValue(resultChecksum, options.aiDecisionCount);
    for (const CaseResult& result : cases)
    {
        HashValue(resultChecksum, result.checksum);
        HashValue(resultChecksum, result.work);
    }

    WriteOutputs(
        outputDirectory,
        options,
        navMesh.TriangleCount(),
        cases,
        resultChecksum);
    std::cout << "result checksum: " << HexChecksum(resultChecksum) << '\n';
    return 0;
}
} // namespace

int main(const int argc, const char* const* const argv)
{
    std::vector<std::string_view> arguments;
    arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index)
    {
        arguments.emplace_back(argv[index]);
    }

    const auto parsed =
        dxa::simulation_benchmark::ParseSimulationBenchmarkOptions(arguments);
    if (!parsed.options.has_value())
    {
        std::cerr << parsed.error << '\n';
        return 1;
    }

    try
    {
        return RunBenchmark(*parsed.options);
    }
    catch (const std::exception& error)
    {
        std::cerr << "simulation benchmark failed: " << error.what() << '\n';
        return 2;
    }
}
