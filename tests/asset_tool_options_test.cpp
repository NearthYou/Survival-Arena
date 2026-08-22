#include <dxa/asset_tool/AssetToolOptions.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string_view>

namespace
{
using dxa::asset_tool::AssetCommand;
using dxa::asset_tool::ParseAssetToolOptions;

TEST(AssetToolOptions, ParsesModelCookCommand)
{
    constexpr std::array arguments{
        std::wstring_view{L"model"},
        std::wstring_view{L"--input"},
        std::wstring_view{L"\uCE90\uB9AD\uD130.fbx"},
        std::wstring_view{L"--output"},
        std::wstring_view{L"runner.dxam"},
        std::wstring_view{L"--sample-rate"},
        std::wstring_view{L"24"}};

    const auto result = ParseAssetToolOptions(arguments);

    ASSERT_TRUE(result.options.has_value()) << result.error;
    EXPECT_EQ(AssetCommand::Model, result.options->command);
    EXPECT_EQ(L"\uCE90\uB9AD\uD130.fbx", result.options->inputPath.native());
    EXPECT_EQ(L"runner.dxam", result.options->outputPath.native());
    EXPECT_NEAR(24.0F, result.options->animationSampleRate, 0.001F);
}

TEST(AssetToolOptions, ParsesTextureCookCommand)
{
    constexpr std::array arguments{
        std::wstring_view{L"texture"},
        std::wstring_view{L"--input"},
        std::wstring_view{L"atlas.png"},
        std::wstring_view{L"--output"},
        std::wstring_view{L"atlas.dds"}};

    const auto result = ParseAssetToolOptions(arguments);

    ASSERT_TRUE(result.options.has_value()) << result.error;
    EXPECT_EQ(AssetCommand::Texture, result.options->command);
}

TEST(AssetToolOptions, RejectsMissingOutput)
{
    constexpr std::array arguments{
        std::wstring_view{L"model"},
        std::wstring_view{L"--input"},
        std::wstring_view{L"runner.fbx"}};

    const auto result = ParseAssetToolOptions(arguments);

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("--output is required", result.error);
}

TEST(AssetToolOptions, RejectsSampleRateForTextureCommand)
{
    constexpr std::array arguments{
        std::wstring_view{L"texture"},
        std::wstring_view{L"--input"},
        std::wstring_view{L"atlas.png"},
        std::wstring_view{L"--output"},
        std::wstring_view{L"atlas.dds"},
        std::wstring_view{L"--sample-rate"},
        std::wstring_view{L"30"}};

    const auto result = ParseAssetToolOptions(arguments);

    EXPECT_FALSE(result.options.has_value());
    EXPECT_EQ("--sample-rate is only valid for model cooking", result.error);
}
} // namespace
