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

TEST(ClientOptions, ParsesNetworkCreateAndRequiresHybridPath)
{
    constexpr std::array validArguments{
        std::string_view{"--render-path"},
        std::string_view{"hybrid-deferred"},
        std::string_view{"--network-create"},
        std::string_view{"--expected-players"},
        std::string_view{"2"},
        std::string_view{"--lobby-host"},
        std::string_view{"127.0.0.1"},
        std::string_view{"--lobby-port"},
        std::string_view{"7000"}};

    const auto valid = ParseClientOptions(validArguments);

    ASSERT_TRUE(valid.options.has_value()) << valid.error;
    ASSERT_TRUE(valid.options->network.has_value());
    EXPECT_EQ(2U, valid.options->network->expectedPlayers);
    EXPECT_EQ("127.0.0.1", valid.options->network->lobbyHost);
    EXPECT_EQ(7000U, valid.options->network->lobbyPort);

    constexpr std::array invalidArguments{
        std::string_view{"--network-create"}};
    EXPECT_FALSE(ParseClientOptions(invalidArguments).options.has_value());
}

TEST(ClientOptions, NetworkResultCanCloseHiddenClientWithoutFrameLimit)
{
    constexpr std::array arguments{
        std::string_view{"--warp"},
        std::string_view{"--hidden"},
        std::string_view{"--verify-render"},
        std::string_view{"--render-path"},
        std::string_view{"hybrid-deferred"},
        std::string_view{"--network-create"},
        std::string_view{"--expected-players"},
        std::string_view{"24"},
        std::string_view{"--exit-on-match-result"}};

    const auto parsed = ParseClientOptions(arguments);

    ASSERT_TRUE(parsed.options.has_value()) << parsed.error;
    ASSERT_TRUE(parsed.options->network.has_value());
    EXPECT_TRUE(parsed.options->network->exitOnMatchResult);
    EXPECT_EQ(0U, parsed.options->frameLimit);
}

TEST(ClientOptions, EnforcesNetworkPlayerAndOptionBoundaries)
{
    constexpr std::array twoPlayers{
        std::string_view{"--render-path"},
        std::string_view{"hybrid-deferred"},
        std::string_view{"--network-create"},
        std::string_view{"--expected-players"},
        std::string_view{"2"}};
    constexpr std::array twentyFourPlayers{
        std::string_view{"--render-path"},
        std::string_view{"hybrid-deferred"},
        std::string_view{"--network-create"},
        std::string_view{"--expected-players"},
        std::string_view{"24"}};
    EXPECT_TRUE(ParseClientOptions(twoPlayers).options.has_value());
    EXPECT_TRUE(ParseClientOptions(twentyFourPlayers).options.has_value());

    constexpr std::array onePlayer{
        std::string_view{"--render-path"},
        std::string_view{"hybrid-deferred"},
        std::string_view{"--network-create"},
        std::string_view{"--expected-players"},
        std::string_view{"1"}};
    constexpr std::array twentyFivePlayers{
        std::string_view{"--render-path"},
        std::string_view{"hybrid-deferred"},
        std::string_view{"--network-create"},
        std::string_view{"--expected-players"},
        std::string_view{"25"}};
    constexpr std::array duplicateCreate{
        std::string_view{"--render-path"},
        std::string_view{"hybrid-deferred"},
        std::string_view{"--network-create"},
        std::string_view{"--network-create"}};
    constexpr std::array networkWithoutCreate{
        std::string_view{"--expected-players"},
        std::string_view{"2"}};
    constexpr std::array exitWithoutCreate{
        std::string_view{"--exit-on-match-result"}};
    constexpr std::array duplicateExit{
        std::string_view{"--render-path"},
        std::string_view{"hybrid-deferred"},
        std::string_view{"--network-create"},
        std::string_view{"--exit-on-match-result"},
        std::string_view{"--exit-on-match-result"}};
    EXPECT_FALSE(ParseClientOptions(onePlayer).options.has_value());
    EXPECT_FALSE(ParseClientOptions(twentyFivePlayers).options.has_value());
    EXPECT_FALSE(ParseClientOptions(duplicateCreate).options.has_value());
    EXPECT_FALSE(ParseClientOptions(networkWithoutCreate).options.has_value());
    EXPECT_FALSE(ParseClientOptions(exitWithoutCreate).options.has_value());
    EXPECT_FALSE(ParseClientOptions(duplicateExit).options.has_value());
}

TEST(ClientOptions, RejectsNetworkBenchmarkCombination)
{
    constexpr std::array arguments{
        std::string_view{"--render-path"},
        std::string_view{"hybrid-deferred"},
        std::string_view{"--network-create"},
        std::string_view{"--benchmark-output"},
        std::string_view{"run-001"},
        std::string_view{"--commit-sha"},
        std::string_view{"abc1234"},
        std::string_view{"--no-vsync"}};

    EXPECT_FALSE(ParseClientOptions(arguments).options.has_value());
}
} // namespace
