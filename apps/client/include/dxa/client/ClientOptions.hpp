#pragma once

#include <dxa/engine/RenderPath.hpp>

#include <cstdint>
#include <charconv>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace dxa::client
{
enum class AdapterType
{
    Hardware,
    Warp
};

struct NetworkClientOptions
{
    std::string lobbyHost = "127.0.0.1";
    std::uint16_t lobbyPort = 7000U;
    std::uint8_t expectedPlayers = 2U;
};

struct ClientOptions
{
    struct BenchmarkOptions
    {
        std::string outputDirectory;
        std::uint32_t warmupFrames = 120;
        std::uint32_t measuredFrames = 600;
        std::uint32_t seed = 20260823;
        std::string commitSha;
    };

    AdapterType adapter = AdapterType::Hardware;
    bool hidden = false;
    bool vsync = true;
    bool verifyRender = false;
    bool verifyAssetScene = false;
    std::uint32_t frameLimit = 0;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    dxa::engine::RenderPath renderPath = dxa::engine::RenderPath::Forward;
    std::optional<BenchmarkOptions> benchmark;
    std::optional<NetworkClientOptions> network;
};

struct ClientOptionsParseResult
{
    std::optional<ClientOptions> options;
    std::string error;
};

namespace detail
{
[[nodiscard]] inline std::optional<std::uint32_t> ParseUnsigned(
    const std::string_view value) noexcept
{
    std::uint32_t parsed = 0;
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end)
    {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] inline ClientOptionsParseResult Error(std::string message)
{
    return ClientOptionsParseResult{std::nullopt, std::move(message)};
}
} // namespace detail

[[nodiscard]] inline ClientOptionsParseResult ParseClientOptions(
    const std::span<const std::string_view> arguments)
{
    ClientOptions options;
    ClientOptions::BenchmarkOptions benchmark;
    bool benchmarkOptionSeen = false;
    bool benchmarkOutputSet = false;
    bool frameLimitExplicit = false;
    NetworkClientOptions network;
    bool networkCreateSeen = false;
    bool networkOptionSeen = false;
    bool expectedPlayersSeen = false;
    bool lobbyHostSeen = false;
    bool lobbyPortSeen = false;

    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::string_view argument = arguments[index];
        if (argument == "--warp")
        {
            options.adapter = AdapterType::Warp;
        }
        else if (argument == "--hidden")
        {
            options.hidden = true;
        }
        else if (argument == "--no-vsync")
        {
            options.vsync = false;
        }
        else if (argument == "--verify-render")
        {
            options.verifyRender = true;
        }
        else if (argument == "--verify-asset-scene")
        {
            options.verifyAssetScene = true;
        }
        else if (argument == "--network-create")
        {
            if (networkCreateSeen)
            {
                return detail::Error("duplicate --network-create");
            }
            networkCreateSeen = true;
        }
        else if (argument == "--lobby-host")
        {
            if (lobbyHostSeen)
            {
                return detail::Error("duplicate --lobby-host");
            }
            if (index + 1 >= arguments.size())
            {
                return detail::Error("--lobby-host requires a value");
            }
            lobbyHostSeen = true;
            networkOptionSeen = true;
            network.lobbyHost = arguments[++index];
            if (network.lobbyHost.empty() || network.lobbyHost.size() > 255U)
            {
                return detail::Error(
                    "--lobby-host must contain 1 to 255 bytes");
            }
        }
        else if (argument == "--render-path")
        {
            if (index + 1 >= arguments.size())
            {
                return detail::Error("--render-path requires a value");
            }
            const std::string_view value = arguments[++index];
            if (value == "forward")
            {
                options.renderPath = dxa::engine::RenderPath::Forward;
            }
            else if (value == "hybrid-deferred")
            {
                options.renderPath = dxa::engine::RenderPath::HybridDeferred;
            }
            else
            {
                return detail::Error("--render-path must be forward or hybrid-deferred");
            }
        }
        else if (
            argument == "--frames"
            || argument == "--width"
            || argument == "--height"
            || argument == "--benchmark-warmup"
            || argument == "--benchmark-frames"
            || argument == "--benchmark-seed"
            || argument == "--expected-players"
            || argument == "--lobby-port")
        {
            if (index + 1 >= arguments.size())
            {
                return detail::Error(std::string{argument} + " requires a value");
            }

            const auto parsed = detail::ParseUnsigned(arguments[++index]);
            if (!parsed.has_value())
            {
                return detail::Error(std::string{argument} + " must be an unsigned integer");
            }

            if (argument == "--frames")
            {
                options.frameLimit = *parsed;
                frameLimitExplicit = true;
            }
            else if (argument == "--benchmark-warmup")
            {
                benchmark.warmupFrames = *parsed;
                benchmarkOptionSeen = true;
            }
            else if (argument == "--benchmark-frames")
            {
                if (*parsed == 0)
                {
                    return detail::Error("--benchmark-frames must be greater than 0");
                }
                benchmark.measuredFrames = *parsed;
                benchmarkOptionSeen = true;
            }
            else if (argument == "--benchmark-seed")
            {
                benchmark.seed = *parsed;
                benchmarkOptionSeen = true;
            }
            else if (argument == "--expected-players")
            {
                if (expectedPlayersSeen)
                {
                    return detail::Error("duplicate --expected-players");
                }
                expectedPlayersSeen = true;
                networkOptionSeen = true;
                if (*parsed < 2U || *parsed > 24U)
                {
                    return detail::Error(
                        "--expected-players must be between 2 and 24");
                }
                network.expectedPlayers = static_cast<std::uint8_t>(*parsed);
            }
            else if (argument == "--lobby-port")
            {
                if (lobbyPortSeen)
                {
                    return detail::Error("duplicate --lobby-port");
                }
                lobbyPortSeen = true;
                networkOptionSeen = true;
                if (*parsed == 0U || *parsed > 65535U)
                {
                    return detail::Error(
                        "--lobby-port must be between 1 and 65535");
                }
                network.lobbyPort = static_cast<std::uint16_t>(*parsed);
            }
            else
            {
                constexpr std::uint32_t MaximumDimension = 16384;
                if (*parsed == 0 || *parsed > MaximumDimension)
                {
                    return detail::Error(
                        std::string{argument} + " must be between 1 and 16384");
                }

                if (argument == "--width")
                {
                    options.width = *parsed;
                }
                else
                {
                    options.height = *parsed;
                }
            }
        }
        else if (argument == "--benchmark-output" || argument == "--commit-sha")
        {
            if (index + 1 >= arguments.size())
            {
                return detail::Error(std::string{argument} + " requires a value");
            }

            const std::string_view value = arguments[++index];
            if (value.empty())
            {
                return detail::Error(std::string{argument} + " requires a non-empty value");
            }

            benchmarkOptionSeen = true;
            if (argument == "--benchmark-output")
            {
                benchmark.outputDirectory = value;
                benchmarkOutputSet = true;
            }
            else
            {
                benchmark.commitSha = value;
            }
        }
        else
        {
            return detail::Error("unknown argument: " + std::string{argument});
        }
    }

    if (options.hidden && options.frameLimit == 0 && !benchmarkOutputSet)
    {
        return detail::Error("--hidden requires --frames greater than 0");
    }

    if (options.verifyRender && options.frameLimit == 0 && !benchmarkOutputSet)
    {
        return detail::Error("--verify-render requires --frames greater than 0");
    }

    if (options.verifyAssetScene && !options.verifyRender)
    {
        return detail::Error("--verify-asset-scene requires --verify-render");
    }

    if (benchmarkOptionSeen && !benchmarkOutputSet)
    {
        return detail::Error("benchmark options require --benchmark-output");
    }

    if (networkOptionSeen && !networkCreateSeen)
    {
        return detail::Error(
            "network options require --network-create");
    }

    if (networkCreateSeen
        && options.renderPath != dxa::engine::RenderPath::HybridDeferred)
    {
        return detail::Error(
            "--network-create requires --render-path hybrid-deferred");
    }

    if (networkCreateSeen && benchmarkOutputSet)
    {
        return detail::Error("network mode cannot run a benchmark");
    }

    if (benchmarkOutputSet)
    {
        if (options.vsync)
        {
            return detail::Error("benchmark run requires --no-vsync");
        }
        if (frameLimitExplicit)
        {
            return detail::Error(
                "benchmark run calculates --frames from its measurement window");
        }
        if (benchmark.commitSha.empty())
        {
            return detail::Error("benchmark run requires --commit-sha");
        }
        if (benchmark.warmupFrames > std::numeric_limits<std::uint32_t>::max()
                - benchmark.measuredFrames)
        {
            return detail::Error("benchmark frame window is too large");
        }

        options.frameLimit = benchmark.warmupFrames + benchmark.measuredFrames;
        options.benchmark = std::move(benchmark);
    }

    if (networkCreateSeen)
    {
        options.network = std::move(network);
    }

    return ClientOptionsParseResult{options, {}};
}
} // namespace dxa::client
