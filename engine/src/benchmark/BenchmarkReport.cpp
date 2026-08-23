#include <dxa/engine/benchmark/BenchmarkReport.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace dxa::engine::benchmark
{
namespace
{
[[nodiscard]] double NearestRank(
    const std::vector<double>& sortedValues,
    const double percentile)
{
    const double rank = std::ceil(percentile * static_cast<double>(sortedValues.size()));
    const auto index = static_cast<std::size_t>(std::max(1.0, rank)) - 1;
    return sortedValues.at(index);
}

[[nodiscard]] MetricSummary SummarizeValues(std::vector<double> values)
{
    if (values.empty())
    {
        return {};
    }

    std::ranges::sort(values);
    const double sum = std::accumulate(values.begin(), values.end(), 0.0);
    return MetricSummary{
        values.front(),
        values.back(),
        sum / static_cast<double>(values.size()),
        NearestRank(values, 0.50),
        NearestRank(values, 0.95),
        NearestRank(values, 0.99)};
}

[[nodiscard]] std::string EscapeJson(const std::string_view value)
{
    std::ostringstream escaped;
    escaped << std::hex << std::setfill('0');
    for (const char rawCharacter : value)
    {
        const auto character = static_cast<unsigned char>(rawCharacter);
        switch (character)
        {
        case '\"':
            escaped << "\\\"";
            break;
        case '\\':
            escaped << "\\\\";
            break;
        case '\b':
            escaped << "\\b";
            break;
        case '\f':
            escaped << "\\f";
            break;
        case '\n':
            escaped << "\\n";
            break;
        case '\r':
            escaped << "\\r";
            break;
        case '\t':
            escaped << "\\t";
            break;
        default:
            if (character < 0x20)
            {
                escaped << "\\u" << std::setw(4)
                        << static_cast<unsigned int>(character);
            }
            else
            {
                escaped << static_cast<char>(character);
            }
            break;
        }
    }
    return escaped.str();
}

void WriteMetric(
    std::ostream& output,
    const std::string_view name,
    const MetricSummary& metric,
    const bool trailingComma)
{
    output << "    \"" << name << "\": {\n"
           << "      \"min\": " << metric.minimum << ",\n"
           << "      \"max\": " << metric.maximum << ",\n"
           << "      \"mean\": " << metric.mean << ",\n"
           << "      \"p50\": " << metric.p50 << ",\n"
           << "      \"p95\": " << metric.p95 << ",\n"
           << "      \"p99\": " << metric.p99 << "\n"
           << "    }" << (trailingComma ? "," : "") << "\n";
}

void RequireWritableStream(const std::ios& stream, const char* operation)
{
    if (!stream)
    {
        throw std::runtime_error{operation};
    }
}

void AppendGpuValue(
    const std::optional<double> value,
    std::vector<double>& values,
    const char* metricName)
{
    if (!value.has_value())
    {
        return;
    }
    if (!std::isfinite(*value) || *value < 0.0)
    {
        throw std::invalid_argument{
            std::string{"GPU "} + metricName + " samples must be finite and non-negative"};
    }
    values.push_back(*value);
}

void WriteOptionalCsv(std::ostream& output, const std::optional<double> value)
{
    if (value.has_value())
    {
        output << *value;
    }
}
} // namespace

FrameSummary SummarizeFrames(const std::span<const FrameSample> samples)
{
    if (samples.empty())
    {
        throw std::invalid_argument{"benchmark report requires at least one frame sample"};
    }

    std::vector<double> cpuValues;
    std::vector<double> gpuValues;
    std::vector<double> drawCallValues;
    std::vector<double> triangleValues;
    std::vector<double> objectValues;
    std::vector<double> workingSetValues;
    std::vector<double> gpuTotalValues;
    std::vector<double> gpuShadowValues;
    std::vector<double> gpuGBufferValues;
    std::vector<double> gpuLightingValues;
    std::vector<double> gpuTransparentValues;
    std::vector<double> shadowDrawCallValues;
    std::vector<double> gBufferDrawCallValues;
    std::vector<double> lightingDrawCallValues;
    std::vector<double> transparentDrawCallValues;
    std::vector<double> visibleObjectValues;
    std::vector<double> culledObjectValues;
    cpuValues.reserve(samples.size());
    gpuValues.reserve(samples.size());
    drawCallValues.reserve(samples.size());
    triangleValues.reserve(samples.size());
    objectValues.reserve(samples.size());
    workingSetValues.reserve(samples.size());
    gpuTotalValues.reserve(samples.size());
    gpuShadowValues.reserve(samples.size());
    gpuGBufferValues.reserve(samples.size());
    gpuLightingValues.reserve(samples.size());
    gpuTransparentValues.reserve(samples.size());
    shadowDrawCallValues.reserve(samples.size());
    gBufferDrawCallValues.reserve(samples.size());
    lightingDrawCallValues.reserve(samples.size());
    transparentDrawCallValues.reserve(samples.size());
    visibleObjectValues.reserve(samples.size());
    culledObjectValues.reserve(samples.size());

    for (const FrameSample& sample : samples)
    {
        if (!std::isfinite(sample.cpuFrameMilliseconds)
            || sample.cpuFrameMilliseconds < 0.0)
        {
            throw std::invalid_argument{"CPU frame samples must be finite and non-negative"};
        }
        cpuValues.push_back(sample.cpuFrameMilliseconds);
        if (sample.gpuForwardMilliseconds.has_value())
        {
            if (!std::isfinite(*sample.gpuForwardMilliseconds)
                || *sample.gpuForwardMilliseconds < 0.0)
            {
                throw std::invalid_argument{"GPU frame samples must be finite and non-negative"};
            }
            gpuValues.push_back(*sample.gpuForwardMilliseconds);
        }
        drawCallValues.push_back(static_cast<double>(sample.drawCalls));
        triangleValues.push_back(static_cast<double>(sample.triangleCount));
        objectValues.push_back(static_cast<double>(sample.objectCount));
        workingSetValues.push_back(static_cast<double>(sample.workingSetBytes));
        AppendGpuValue(sample.gpuTotalMilliseconds, gpuTotalValues, "total");
        AppendGpuValue(sample.gpuShadowMilliseconds, gpuShadowValues, "shadow");
        AppendGpuValue(sample.gpuGBufferMilliseconds, gpuGBufferValues, "G-Buffer");
        AppendGpuValue(sample.gpuLightingMilliseconds, gpuLightingValues, "lighting");
        AppendGpuValue(
            sample.gpuTransparentMilliseconds,
            gpuTransparentValues,
            "transparent");
        shadowDrawCallValues.push_back(static_cast<double>(sample.shadowDrawCalls));
        gBufferDrawCallValues.push_back(static_cast<double>(sample.gBufferDrawCalls));
        lightingDrawCallValues.push_back(static_cast<double>(sample.lightingDrawCalls));
        transparentDrawCallValues.push_back(
            static_cast<double>(sample.transparentDrawCalls));
        visibleObjectValues.push_back(static_cast<double>(sample.visibleObjectCount));
        culledObjectValues.push_back(static_cast<double>(sample.culledObjectCount));
    }

    const std::size_t effectiveGpuSampleCount = gpuTotalValues.empty()
        ? gpuValues.size()
        : gpuTotalValues.size();
    return FrameSummary{
        samples.size(),
        effectiveGpuSampleCount,
        SummarizeValues(std::move(cpuValues)),
        SummarizeValues(std::move(gpuValues)),
        SummarizeValues(std::move(drawCallValues)),
        SummarizeValues(std::move(triangleValues)),
        SummarizeValues(std::move(objectValues)),
        SummarizeValues(std::move(workingSetValues)),
        gpuTotalValues.size(),
        gpuShadowValues.size(),
        gpuGBufferValues.size(),
        gpuLightingValues.size(),
        gpuTransparentValues.size(),
        SummarizeValues(std::move(gpuTotalValues)),
        SummarizeValues(std::move(gpuShadowValues)),
        SummarizeValues(std::move(gpuGBufferValues)),
        SummarizeValues(std::move(gpuLightingValues)),
        SummarizeValues(std::move(gpuTransparentValues)),
        SummarizeValues(std::move(shadowDrawCallValues)),
        SummarizeValues(std::move(gBufferDrawCallValues)),
        SummarizeValues(std::move(lightingDrawCallValues)),
        SummarizeValues(std::move(transparentDrawCallValues)),
        SummarizeValues(std::move(visibleObjectValues)),
        SummarizeValues(std::move(culledObjectValues))};
}

void WriteBenchmarkReport(
    const std::filesystem::path& outputDirectory,
    const BenchmarkMetadata& metadata,
    const std::span<const FrameSample> samples)
{
    const FrameSummary summary = SummarizeFrames(samples);
    std::error_code error;
    if (std::filesystem::exists(outputDirectory, error))
    {
        throw std::runtime_error{"benchmark output directory already exists"};
    }
    if (error)
    {
        throw std::runtime_error{"failed to inspect benchmark output directory"};
    }

    const std::filesystem::path parent = outputDirectory.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, error);
        if (error)
        {
            throw std::runtime_error{"failed to create benchmark output parent"};
        }
    }
    if (!std::filesystem::create_directory(outputDirectory, error) || error)
    {
        throw std::runtime_error{"failed to create benchmark output directory"};
    }

    std::ofstream csv{outputDirectory / "frames.csv", std::ios::binary};
    RequireWritableStream(csv, "failed to create benchmark CSV");
    csv.imbue(std::locale::classic());
    csv << "frame_index,cpu_frame_ms,gpu_forward_ms,draw_calls,triangle_count,object_count,working_set_bytes,gpu_total_ms,gpu_shadow_ms,gpu_gbuffer_ms,gpu_lighting_ms,gpu_transparent_ms,shadow_draw_calls,gbuffer_draw_calls,lighting_draw_calls,transparent_draw_calls,visible_object_count,culled_object_count\n";
    csv << std::fixed << std::setprecision(6);
    for (const FrameSample& sample : samples)
    {
        csv << sample.frameIndex << ','
            << sample.cpuFrameMilliseconds << ',';
        if (sample.gpuForwardMilliseconds.has_value())
        {
            csv << *sample.gpuForwardMilliseconds;
        }
        csv << ',' << sample.drawCalls
            << ',' << sample.triangleCount
            << ',' << sample.objectCount
            << ',' << sample.workingSetBytes
            << ',';
        WriteOptionalCsv(csv, sample.gpuTotalMilliseconds);
        csv << ',';
        WriteOptionalCsv(csv, sample.gpuShadowMilliseconds);
        csv << ',';
        WriteOptionalCsv(csv, sample.gpuGBufferMilliseconds);
        csv << ',';
        WriteOptionalCsv(csv, sample.gpuLightingMilliseconds);
        csv << ',';
        WriteOptionalCsv(csv, sample.gpuTransparentMilliseconds);
        csv << ',' << sample.shadowDrawCalls
            << ',' << sample.gBufferDrawCalls
            << ',' << sample.lightingDrawCalls
            << ',' << sample.transparentDrawCalls
            << ',' << sample.visibleObjectCount
            << ',' << sample.culledObjectCount
            << '\n';
    }
    csv.flush();
    RequireWritableStream(csv, "failed to write benchmark CSV");

    std::ofstream json{outputDirectory / "summary.json", std::ios::binary};
    RequireWritableStream(json, "failed to create benchmark summary JSON");
    json.imbue(std::locale::classic());
    json << std::fixed << std::setprecision(6)
         << "{\n"
         << "  \"schema_version\": 2,\n"
         << "  \"render_path\": \"" << dxa::engine::ToString(metadata.renderPath)
         << "\",\n"
         << "  \"seed\": " << metadata.seed << ",\n"
         << "  \"resolution\": {\"width\": " << metadata.width
         << ", \"height\": " << metadata.height << "},\n"
         << "  \"warmup_frames\": " << metadata.warmupFrames << ",\n"
         << "  \"measured_frames\": " << metadata.measuredFrames << ",\n"
         << "  \"commit_sha\": \"" << EscapeJson(metadata.commitSha) << "\",\n"
         << "  \"adapter\": \"" << EscapeJson(metadata.adapter) << "\",\n"
         << "  \"command\": \"" << EscapeJson(metadata.command) << "\",\n"
         << "  \"started_at\": \"" << EscapeJson(metadata.startedAt) << "\",\n"
         << "  \"sample_count\": " << summary.sampleCount << ",\n"
         << "  \"gpu_sample_count\": " << summary.gpuSampleCount << ",\n"
         << "  \"gpu_missing_samples\": "
         << summary.sampleCount - summary.gpuSampleCount << ",\n"
         << "  \"gpu_total_sample_count\": " << summary.gpuTotalSampleCount << ",\n"
         << "  \"gpu_shadow_sample_count\": " << summary.gpuShadowSampleCount << ",\n"
         << "  \"gpu_gbuffer_sample_count\": " << summary.gpuGBufferSampleCount << ",\n"
         << "  \"gpu_lighting_sample_count\": " << summary.gpuLightingSampleCount << ",\n"
         << "  \"gpu_transparent_sample_count\": "
         << summary.gpuTransparentSampleCount << ",\n"
         << "  \"metrics\": {\n";
    WriteMetric(json, "cpu_frame_ms", summary.cpuFrameMilliseconds, true);
    WriteMetric(json, "gpu_forward_ms", summary.gpuForwardMilliseconds, true);
    WriteMetric(json, "gpu_total_ms", summary.gpuTotalMilliseconds, true);
    WriteMetric(json, "gpu_shadow_ms", summary.gpuShadowMilliseconds, true);
    WriteMetric(json, "gpu_gbuffer_ms", summary.gpuGBufferMilliseconds, true);
    WriteMetric(json, "gpu_lighting_ms", summary.gpuLightingMilliseconds, true);
    WriteMetric(json, "gpu_transparent_ms", summary.gpuTransparentMilliseconds, true);
    WriteMetric(json, "draw_calls", summary.drawCalls, true);
    WriteMetric(json, "shadow_draw_calls", summary.shadowDrawCalls, true);
    WriteMetric(json, "gbuffer_draw_calls", summary.gBufferDrawCalls, true);
    WriteMetric(json, "lighting_draw_calls", summary.lightingDrawCalls, true);
    WriteMetric(json, "transparent_draw_calls", summary.transparentDrawCalls, true);
    WriteMetric(json, "triangle_count", summary.triangleCount, true);
    WriteMetric(json, "object_count", summary.objectCount, true);
    WriteMetric(json, "visible_object_count", summary.visibleObjectCount, true);
    WriteMetric(json, "culled_object_count", summary.culledObjectCount, true);
    WriteMetric(json, "working_set_bytes", summary.workingSetBytes, false);
    json << "  }\n"
         << "}\n";
    json.flush();
    RequireWritableStream(json, "failed to write benchmark summary JSON");
}
} // namespace dxa::engine::benchmark
