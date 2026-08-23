#include <dxa/engine/benchmark/BenchmarkReport.hpp>
#include <dxa/engine/RenderPath.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>

namespace
{
using dxa::engine::benchmark::BenchmarkMetadata;
using dxa::engine::benchmark::FrameSample;
using dxa::engine::benchmark::SummarizeFrames;
using dxa::engine::benchmark::WriteBenchmarkReport;

class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path()
            / ("dxa-benchmark-report-" + std::to_string(suffix));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

[[nodiscard]] std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

[[nodiscard]] FrameSample ForwardFrameSample(
    const std::uint64_t frameIndex,
    const double cpuFrameMilliseconds,
    const std::optional<double> gpuForwardMilliseconds,
    const std::uint32_t drawCalls,
    const std::uint64_t triangleCount,
    const std::uint32_t objectCount,
    const std::uint64_t workingSetBytes)
{
    FrameSample sample{};
    sample.frameIndex = frameIndex;
    sample.cpuFrameMilliseconds = cpuFrameMilliseconds;
    sample.gpuForwardMilliseconds = gpuForwardMilliseconds;
    sample.drawCalls = drawCalls;
    sample.triangleCount = triangleCount;
    sample.objectCount = objectCount;
    sample.workingSetBytes = workingSetBytes;
    return sample;
}

TEST(BenchmarkReport, UsesNearestRankForPercentilesAndSkipsMissingGpuSamples)
{
    const std::array samples{
        ForwardFrameSample(1, 1.0, 4.0, 10, 30, 20, 1000),
        ForwardFrameSample(2, 2.0, std::nullopt, 11, 33, 21, 2000),
        ForwardFrameSample(3, 3.0, 8.0, 12, 36, 22, 3000),
        ForwardFrameSample(4, 4.0, 10.0, 13, 39, 23, 4000),
        ForwardFrameSample(5, 100.0, std::nullopt, 14, 42, 24, 5000)};

    const auto summary = SummarizeFrames(samples);

    EXPECT_EQ(5U, summary.sampleCount);
    EXPECT_EQ(3U, summary.gpuSampleCount);
    EXPECT_DOUBLE_EQ(3.0, summary.cpuFrameMilliseconds.p50);
    EXPECT_DOUBLE_EQ(100.0, summary.cpuFrameMilliseconds.p95);
    EXPECT_DOUBLE_EQ(100.0, summary.cpuFrameMilliseconds.p99);
    EXPECT_DOUBLE_EQ(8.0, summary.gpuForwardMilliseconds.p50);
    EXPECT_DOUBLE_EQ(10.0, summary.gpuForwardMilliseconds.p95);
    EXPECT_EQ(14U, summary.drawCalls.maximum);
    EXPECT_EQ(5000U, summary.workingSetBytes.maximum);
}

TEST(BenchmarkReport, WritesRawCsvAndSummaryJsonWithoutOverwritingARun)
{
    TemporaryDirectory temporary;
    const std::filesystem::path output = temporary.Path() / "run-001";
    const BenchmarkMetadata metadata{
        .seed = 20260823,
        .width = 1920,
        .height = 1080,
        .warmupFrames = 120,
        .measuredFrames = 2,
        .commitSha = "abc1234",
        .adapter = "NVIDIA GeForce RTX 3050 Ti Laptop GPU",
        .command = "dxa_client --benchmark-output run-001 --commit-sha abc1234",
        .startedAt = "2026-08-23T12:34:56+09:00",
        .renderPath = dxa::engine::RenderPath::Forward};
    const std::array samples{
        ForwardFrameSample(121, 5.25, 3.5, 1002, 3006, 1124, 104857600),
        ForwardFrameSample(
            122,
            6.75,
            std::nullopt,
            1002,
            3006,
            1124,
            105906176)};

    WriteBenchmarkReport(output, metadata, samples);

    const std::string csv = ReadText(output / "frames.csv");
    EXPECT_NE(std::string::npos, csv.find(
        "frame_index,cpu_frame_ms,gpu_forward_ms,draw_calls,triangle_count,object_count,working_set_bytes,"));
    EXPECT_NE(std::string::npos, csv.find("121,5.250000,3.500000,1002,3006,1124,104857600,"));
    EXPECT_NE(std::string::npos, csv.find("122,6.750000,,1002,3006,1124,105906176,"));

    const std::string json = ReadText(output / "summary.json");
    EXPECT_NE(std::string::npos, json.find("\"schema_version\": 2"));
    EXPECT_NE(std::string::npos, json.find("\"seed\": 20260823"));
    EXPECT_NE(std::string::npos, json.find("\"commit_sha\": \"abc1234\""));
    EXPECT_NE(std::string::npos, json.find("\"gpu_missing_samples\": 1"));
    EXPECT_NE(std::string::npos, json.find(
        "\"command\": \"dxa_client --benchmark-output run-001 --commit-sha abc1234\""));

    EXPECT_THROW(WriteBenchmarkReport(output, metadata, samples), std::runtime_error);
}

TEST(BenchmarkReport, RejectsAnEmptySampleSet)
{
    EXPECT_THROW((void)SummarizeFrames({}), std::invalid_argument);
}

TEST(BenchmarkReport, WritesHybridPassMetricsWithoutRemovingBaselineColumns)
{
    TemporaryDirectory temporary;
    const std::filesystem::path output = temporary.Path() / "hybrid-run";
    const BenchmarkMetadata metadata{
        .seed = 20260823,
        .width = 1920,
        .height = 1080,
        .warmupFrames = 120,
        .measuredFrames = 1,
        .commitSha = "def5678",
        .adapter = "NVIDIA GeForce RTX 3050 Ti Laptop GPU",
        .command = "dxa_client --render-path hybrid-deferred",
        .startedAt = "2026-08-23T13:00:00+09:00",
        .renderPath = dxa::engine::RenderPath::HybridDeferred};
    const std::array samples{
        FrameSample{
            .frameIndex = 121,
            .cpuFrameMilliseconds = 4.0,
            .gpuForwardMilliseconds = std::nullopt,
            .drawCalls = 1400,
            .triangleCount = 5000,
            .objectCount = 1124,
            .workingSetBytes = 104857600,
            .gpuTotalMilliseconds = 3.0,
            .gpuShadowMilliseconds = 0.5,
            .gpuGBufferMilliseconds = 1.0,
            .gpuLightingMilliseconds = 1.25,
            .gpuTransparentMilliseconds = 0.25,
            .shadowDrawCalls = 125,
            .gBufferDrawCalls = 1273,
            .lightingDrawCalls = 1,
            .transparentDrawCalls = 1,
            .visibleObjectCount = 700,
            .culledObjectCount = 424}};

    WriteBenchmarkReport(output, metadata, samples);

    const std::string csv = ReadText(output / "frames.csv");
    EXPECT_NE(std::string::npos, csv.find(
        "frame_index,cpu_frame_ms,gpu_forward_ms,draw_calls,triangle_count,object_count,working_set_bytes,gpu_total_ms,gpu_shadow_ms,gpu_gbuffer_ms,gpu_lighting_ms,gpu_transparent_ms,shadow_draw_calls,gbuffer_draw_calls,lighting_draw_calls,transparent_draw_calls,visible_object_count,culled_object_count\n"));
    EXPECT_NE(std::string::npos, csv.find(
        "121,4.000000,,1400,5000,1124,104857600,3.000000,0.500000,1.000000,1.250000,0.250000,125,1273,1,1,700,424\n"));

    const std::string json = ReadText(output / "summary.json");
    EXPECT_NE(std::string::npos, json.find("\"render_path\": \"hybrid-deferred\""));
    EXPECT_NE(std::string::npos, json.find("\"gpu_total_ms\""));
    EXPECT_NE(std::string::npos, json.find("\"gpu_shadow_ms\""));
    EXPECT_NE(std::string::npos, json.find("\"visible_object_count\""));
}
} // namespace
