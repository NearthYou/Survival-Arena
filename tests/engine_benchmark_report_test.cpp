#include <dxa/engine/benchmark/BenchmarkReport.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
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

TEST(BenchmarkReport, UsesNearestRankForPercentilesAndSkipsMissingGpuSamples)
{
    const std::array samples{
        FrameSample{1, 1.0, 4.0, 10, 30, 20, 1000},
        FrameSample{2, 2.0, std::nullopt, 11, 33, 21, 2000},
        FrameSample{3, 3.0, 8.0, 12, 36, 22, 3000},
        FrameSample{4, 4.0, 10.0, 13, 39, 23, 4000},
        FrameSample{5, 100.0, std::nullopt, 14, 42, 24, 5000}};

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
        20260823,
        1920,
        1080,
        120,
        2,
        "abc1234",
        "NVIDIA GeForce RTX 3050 Ti Laptop GPU",
        "dxa_client --benchmark-output run-001 --commit-sha abc1234",
        "2026-08-23T12:34:56+09:00"};
    const std::array samples{
        FrameSample{121, 5.25, 3.5, 1002, 3006, 1124, 104857600},
        FrameSample{122, 6.75, std::nullopt, 1002, 3006, 1124, 105906176}};

    WriteBenchmarkReport(output, metadata, samples);

    const std::string csv = ReadText(output / "frames.csv");
    EXPECT_NE(std::string::npos, csv.find(
        "frame_index,cpu_frame_ms,gpu_forward_ms,draw_calls,triangle_count,object_count,working_set_bytes\n"));
    EXPECT_NE(std::string::npos, csv.find("121,5.250000,3.500000,1002,3006,1124,104857600\n"));
    EXPECT_NE(std::string::npos, csv.find("122,6.750000,,1002,3006,1124,105906176\n"));

    const std::string json = ReadText(output / "summary.json");
    EXPECT_NE(std::string::npos, json.find("\"schema_version\": 1"));
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
} // namespace
