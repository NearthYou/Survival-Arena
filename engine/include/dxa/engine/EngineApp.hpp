#pragma once

#include <dxa/engine/GraphicsTypes.hpp>

#include <cstdint>
#include <filesystem>

namespace dxa::engine
{
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
