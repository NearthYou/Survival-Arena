#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace dxa::engine::benchmark
{
struct FrameSample
{
    std::uint64_t frameIndex = 0;
    double cpuFrameMilliseconds = 0.0;
    std::optional<double> gpuForwardMilliseconds;
    std::uint32_t drawCalls = 0;
    std::uint64_t triangleCount = 0;
    std::uint32_t objectCount = 0;
    std::uint64_t workingSetBytes = 0;
};

struct MetricSummary
{
    double minimum = 0.0;
    double maximum = 0.0;
    double mean = 0.0;
    double p50 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
};

struct FrameSummary
{
    std::size_t sampleCount = 0;
    std::size_t gpuSampleCount = 0;
    MetricSummary cpuFrameMilliseconds;
    MetricSummary gpuForwardMilliseconds;
    MetricSummary drawCalls;
    MetricSummary triangleCount;
    MetricSummary objectCount;
    MetricSummary workingSetBytes;
};

struct BenchmarkMetadata
{
    std::uint32_t seed = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t warmupFrames = 0;
    std::uint32_t measuredFrames = 0;
    std::string commitSha;
    std::string adapter;
    std::string command;
    std::string startedAt;
};

[[nodiscard]] FrameSummary SummarizeFrames(std::span<const FrameSample> samples);

void WriteBenchmarkReport(
    const std::filesystem::path& outputDirectory,
    const BenchmarkMetadata& metadata,
    std::span<const FrameSample> samples);
} // namespace dxa::engine::benchmark
