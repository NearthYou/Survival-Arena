#include <dxa/client/ClientOptions.hpp>
#include <dxa/engine/RenderPath.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string_view>

namespace
{
using dxa::client::AdapterType;
using dxa::client::ParseClientOptions;

TEST(ClientOptions, ParsesHeadlessWarpSmokeRun)
{
    constexpr std::array arguments{
        std::string_view{"--warp"},
        std::string_view{"--hidden"},
        std::string_view{"--frames"},
        std::string_view{"3"},
        std::string_view{"--width"},
        std::string_view{"640"},
        std::string_view{"--height"},
        std::string_view{"360"},
        std::string_view{"--verify-render"},
        std::string_view{"--verify-asset-scene"},
        std::string_view{"--no-vsync"}};

    const auto result = ParseClientOptions(arguments);

    ASSERT_TRUE(result.options.has_value()) << result.error;
    EXPECT_EQ(AdapterType::Warp, result.options->adapter);
    EXPECT_TRUE(result.options->hidden);
    EXPECT_FALSE(result.options->vsync);
    EXPECT_TRUE(result.options->verifyRender);
    EXPECT_TRUE(result.options->verifyAssetScene);
    EXPECT_EQ(3U, result.options->frameLimit);
    EXPECT_EQ(640U, result.options->width);
    EXPECT_EQ(360U, result.options->height);
}

TEST(ClientOptions, RejectsZeroWidth)
{
    constexpr std::array arguments{
        std::string_view{"--width"},
        std::string_view{"0"}};

    const auto result = ParseClientOptions(arguments);

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("--width must be between 1 and 16384", result.error);
}

TEST(ClientOptions, RejectsMissingValue)
{
    constexpr std::array arguments{std::string_view{"--frames"}};

    const auto result = ParseClientOptions(arguments);

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("--frames requires a value", result.error);
}

TEST(ClientOptions, RejectsUnknownArgument)
{
    constexpr std::array arguments{std::string_view{"--fast"}};

    const auto result = ParseClientOptions(arguments);

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("unknown argument: --fast", result.error);
}

TEST(ClientOptions, RejectsHiddenRunWithoutFrameLimit)
{
    constexpr std::array arguments{std::string_view{"--hidden"}};

    const auto result = ParseClientOptions(arguments);

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("--hidden requires --frames greater than 0", result.error);
}

TEST(ClientOptions, RejectsAssetSceneCheckWithoutRenderVerification)
{
    constexpr std::array arguments{
        std::string_view{"--frames"},
        std::string_view{"3"},
        std::string_view{"--verify-asset-scene"}};

    const auto result = ParseClientOptions(arguments);

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("--verify-asset-scene requires --verify-render", result.error);
}

TEST(ClientOptions, ParsesDeterministicBenchmarkRun)
{
    constexpr std::array arguments{
        std::string_view{"--benchmark-output"},
        std::string_view{"docs/benchmarks/forward-baseline/run-001"},
        std::string_view{"--benchmark-warmup"},
        std::string_view{"120"},
        std::string_view{"--benchmark-frames"},
        std::string_view{"600"},
        std::string_view{"--benchmark-seed"},
        std::string_view{"20260823"},
        std::string_view{"--commit-sha"},
        std::string_view{"abc1234"},
        std::string_view{"--width"},
        std::string_view{"1920"},
        std::string_view{"--height"},
        std::string_view{"1080"},
        std::string_view{"--hidden"},
        std::string_view{"--no-vsync"}};

    const auto result = ParseClientOptions(arguments);

    ASSERT_TRUE(result.options.has_value()) << result.error;
    ASSERT_TRUE(result.options->benchmark.has_value());
    EXPECT_EQ(
        "docs/benchmarks/forward-baseline/run-001",
        result.options->benchmark->outputDirectory);
    EXPECT_EQ(120U, result.options->benchmark->warmupFrames);
    EXPECT_EQ(600U, result.options->benchmark->measuredFrames);
    EXPECT_EQ(20260823U, result.options->benchmark->seed);
    EXPECT_EQ("abc1234", result.options->benchmark->commitSha);
    EXPECT_EQ(720U, result.options->frameLimit);
    EXPECT_TRUE(result.options->hidden);
    EXPECT_FALSE(result.options->vsync);
}

TEST(ClientOptions, RejectsBenchmarkWithVsync)
{
    constexpr std::array arguments{
        std::string_view{"--benchmark-output"},
        std::string_view{"run-001"}};

    const auto result = ParseClientOptions(arguments);

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("benchmark run requires --no-vsync", result.error);
}

TEST(ClientOptions, RejectsBenchmarkFrameLimitOverride)
{
    constexpr std::array arguments{
        std::string_view{"--benchmark-output"},
        std::string_view{"run-001"},
        std::string_view{"--frames"},
        std::string_view{"1"},
        std::string_view{"--no-vsync"}};

    const auto result = ParseClientOptions(arguments);

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("benchmark run calculates --frames from its measurement window", result.error);
}

TEST(ClientOptions, RejectsBenchmarkOptionWithoutOutputDirectory)
{
    constexpr std::array arguments{
        std::string_view{"--benchmark-seed"},
        std::string_view{"7"},
        std::string_view{"--no-vsync"}};

    const auto result = ParseClientOptions(arguments);

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("benchmark options require --benchmark-output", result.error);
}

TEST(ClientOptions, RejectsZeroMeasuredBenchmarkFrames)
{
    constexpr std::array arguments{
        std::string_view{"--benchmark-output"},
        std::string_view{"run-001"},
        std::string_view{"--benchmark-frames"},
        std::string_view{"0"},
        std::string_view{"--no-vsync"}};

    const auto result = ParseClientOptions(arguments);

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("--benchmark-frames must be greater than 0", result.error);
}

TEST(ClientOptions, RejectsBenchmarkWithoutCommitSha)
{
    constexpr std::array arguments{
        std::string_view{"--benchmark-output"},
        std::string_view{"run-001"},
        std::string_view{"--no-vsync"}};

    const auto result = ParseClientOptions(arguments);

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("benchmark run requires --commit-sha", result.error);
}

TEST(ClientOptions, ParsesHybridDeferredRenderPath)
{
    constexpr std::array arguments{
        std::string_view{"--render-path"},
        std::string_view{"hybrid-deferred"}};

    const auto result = ParseClientOptions(arguments);

    ASSERT_TRUE(result.options.has_value()) << result.error;
    EXPECT_EQ(dxa::engine::RenderPath::HybridDeferred, result.options->renderPath);
}

TEST(ClientOptions, RejectsUnknownRenderPath)
{
    constexpr std::array arguments{
        std::string_view{"--render-path"},
        std::string_view{"path-tracing"}};

    const auto result = ParseClientOptions(arguments);

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("--render-path must be forward or hybrid-deferred", result.error);
}
} // namespace
