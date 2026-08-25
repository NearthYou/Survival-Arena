#include <dxa/client/ClientOptions.hpp>
#include <dxa/client/NetworkClientController.hpp>
#include <dxa/engine/EngineApp.hpp>

#include <Windows.h>

#include <spdlog/spdlog.h>

#include <cstdint>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
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

[[nodiscard]] std::string CommandLine(
    const int argc,
    const char* const* const argv)
{
    std::ostringstream command;
    for (int index = 0; index < argc; ++index)
    {
        if (index != 0)
        {
            command << ' ';
        }
        command << std::quoted(argv[index]);
    }
    return command.str();
}

[[nodiscard]] std::string UtcTimestamp()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm utc{};
    if (gmtime_s(&utc, &now) != 0)
    {
        throw std::runtime_error{"gmtime_s failed"};
    }
    std::ostringstream formatted;
    formatted << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return formatted.str();
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
        "client start: adapter={}, size={}x{}, frames={}, hidden={}, vsync={}, asset_scene={}, benchmark={}, render_path={}, network={}",
        options.adapter == dxa::client::AdapterType::Warp ? "warp" : "hardware",
        options.width,
        options.height,
        options.frameLimit,
        options.hidden,
        options.vsync,
        options.verifyAssetScene,
        options.benchmark.has_value(),
        dxa::engine::ToString(options.renderPath),
        options.network.has_value());

    std::optional<dxa::engine::BenchmarkRunOptions> engineBenchmark;
    if (options.benchmark.has_value())
    {
        engineBenchmark = dxa::engine::BenchmarkRunOptions{
            std::filesystem::path{options.benchmark->outputDirectory},
            options.benchmark->warmupFrames,
            options.benchmark->measuredFrames,
            options.benchmark->seed,
            options.benchmark->commitSha,
            CommandLine(argc, argv),
            UtcTimestamp()};
    }

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
        std::move(engineBenchmark),
        options.renderPath};

    try
    {
        std::unique_ptr<dxa::client::NetworkClientController> network;
        if (options.network.has_value())
        {
            network = std::make_unique<
                dxa::client::NetworkClientController>(*options.network);
            network->Start();
        }
        const std::filesystem::path shaderPath =
            ExecutableDirectory() / L"shaders" / L"forward.hlsl";
        const std::filesystem::path assetRoot = ExecutableDirectory() / L"assets";
        const int exitCode = dxa::engine::EngineApp{}.Run(
            engineOptions,
            shaderPath,
            assetRoot,
            network.get());
        if (network)
        {
            network->Stop();
        }
        spdlog::info("client stop: exit={}", exitCode);
        return exitCode;
    }
    catch (const std::exception& error)
    {
        spdlog::error("client failed: {}", error.what());
        return 2;
    }
}
