#include <dxa/engine/assets/AnimationPlayback.hpp>

#include <gtest/gtest.h>

#include <array>
#include <span>

namespace
{
[[nodiscard]] dxa::engine::asset::Matrix4 TranslationY(const float value)
{
    return dxa::engine::asset::Matrix4{
        std::array<float, 16>{
            1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, value, 0.0F, 1.0F}};
}

[[nodiscard]] dxa::engine::asset::ModelAsset MakeAnimatedAsset()
{
    dxa::engine::asset::ModelAsset asset;
    asset.joints.push_back(dxa::engine::asset::Joint{"Root", -1, TranslationY(0.0F)});
    asset.animations.push_back(dxa::engine::asset::AnimationClip{
        "Move",
        1.0F,
        2.0F,
        3,
        {TranslationY(0.0F), TranslationY(0.5F), TranslationY(1.0F)}});
    return asset;
}

TEST(AnimationPlayback, SelectsPreviousBakedSample)
{
    const dxa::engine::asset::ModelAsset asset = MakeAnimatedAsset();

    const std::span<const dxa::engine::asset::Matrix4> palette =
        dxa::engine::asset::SampleAnimationPalette(asset, 0, 0.74);

    ASSERT_EQ(1U, palette.size());
    EXPECT_NEAR(0.5F, palette.front().elements[13], 0.001F);
}

TEST(AnimationPlayback, LoopsAtClipDuration)
{
    const dxa::engine::asset::ModelAsset asset = MakeAnimatedAsset();

    const std::span<const dxa::engine::asset::Matrix4> palette =
        dxa::engine::asset::SampleAnimationPalette(asset, 0, 1.25);

    ASSERT_EQ(1U, palette.size());
    EXPECT_NEAR(0.0F, palette.front().elements[13], 0.001F);
}

TEST(AnimationPlayback, ReturnsEmptyPaletteForStaticModel)
{
    const dxa::engine::asset::ModelAsset asset;

    EXPECT_TRUE(dxa::engine::asset::SampleAnimationPalette(asset, 0, 1.0).empty());
}
} // namespace
