#include <dxa/client/ClientOptions.hpp>

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
        std::string_view{"--no-vsync"}};

    const auto result = ParseClientOptions(arguments);

    ASSERT_TRUE(result.options.has_value()) << result.error;
    EXPECT_EQ(AdapterType::Warp, result.options->adapter);
    EXPECT_TRUE(result.options->hidden);
    EXPECT_FALSE(result.options->vsync);
    EXPECT_TRUE(result.options->verifyRender);
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
} // namespace
