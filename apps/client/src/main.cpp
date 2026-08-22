#include <dxa/client/ClientOptions.hpp>
#include <dxa/engine/EngineApp.hpp>

#include <Windows.h>

#include <spdlog/spdlog.h>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
[[nodiscard]] std::filesystem::path ExecutableDirectory()
{
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size())
    {
        throw std::runtime_error("GetModuleFileNameW failed");
    }
    path.resize(length);
    return std::filesystem::path{path}.parent_path();
}
} // namespace

int main(const int argc, const char* const* argv)
{
    std::vector<std::string_view> arguments;
    arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index)
    {
        arguments.emplace_back(argv[index]);
    }

    const auto parsed = dxa::client::ParseClientOptions(arguments);
    if (!parsed.options.has_value())
    {
        std::cerr << parsed.error << '\n';
        return 1;
    }

    const dxa::client::ClientOptions& options = *parsed.options;
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::info(
        "client start: adapter={}, size={}x{}, frames={}, hidden={}, vsync={}, asset_scene={}, benchmark={}",
        options.adapter == dxa::client::AdapterType::Warp ? "warp" : "hardware",
        options.width,
        options.height,
        options.frameLimit,
        options.hidden,
        options.vsync,
        options.verifyAssetScene,
        options.benchmark.has_value());

    const dxa::engine::EngineRunOptions engineOptions{
        options.width,
        options.height,
        options.frameLimit,
        options.hidden,
        options.vsync,
        options.verifyRender,
        options.adapter == dxa::client::AdapterType::Warp
            ? dxa::engine::GraphicsDriver::Warp
            : dxa::engine::GraphicsDriver::Hardware,
        options.verifyAssetScene,
        options.benchmark.has_value()
            ? std::optional<std::uint32_t>{options.benchmark->seed}
            : std::nullopt};

    try
    {
        const std::filesystem::path shaderPath =
            ExecutableDirectory() / L"shaders" / L"forward.hlsl";
        const std::filesystem::path assetRoot = ExecutableDirectory() / L"assets";
        const int exitCode = dxa::engine::EngineApp{}.Run(engineOptions, shaderPath, assetRoot);
        spdlog::info("client stop: exit={}", exitCode);
        return exitCode;
    }
    catch (const std::exception& error)
    {
        spdlog::error("client failed: {}", error.what());
        return 2;
    }
}
