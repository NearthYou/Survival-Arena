#pragma once

#include <cstdint>
#include <filesystem>

namespace dxa::engine
{
enum class GraphicsDriver
{
    Hardware,
    Warp
};

struct EngineRunOptions
{
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::uint32_t frameLimit = 0;
    bool hidden = false;
    bool vsync = true;
    GraphicsDriver driver = GraphicsDriver::Hardware;
};

class EngineApp
{
public:
    [[nodiscard]] int Run(
        const EngineRunOptions& options,
        const std::filesystem::path& shaderPath) const;
};
} // namespace dxa::engine

