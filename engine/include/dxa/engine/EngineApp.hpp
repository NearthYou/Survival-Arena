#pragma once

#include <dxa/engine/GraphicsTypes.hpp>
#include <dxa/engine/RenderPath.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace dxa::engine
{
struct BenchmarkRunOptions
{
    std::filesystem::path outputDirectory;
    std::uint32_t warmupFrames = 0;
    std::uint32_t measuredFrames = 0;
    std::uint32_t seed = 0;
    std::string commitSha;
    std::string command;
    std::string startedAt;
};

struct EngineRunOptions
{
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::uint32_t frameLimit = 0;
    bool hidden = false;
    bool vsync = true;
    bool verifyRender = false;
    GraphicsDriver driver = GraphicsDriver::Hardware;
    bool verifyAssetScene = false;
    std::optional<BenchmarkRunOptions> benchmark;
    RenderPath renderPath = RenderPath::Forward;
};

class EngineApp
{
public:
    [[nodiscard]] int Run(
        const EngineRunOptions& options,
        const std::filesystem::path& shaderPath,
        const std::filesystem::path& assetRoot = {}) const;
};
} // namespace dxa::engine
