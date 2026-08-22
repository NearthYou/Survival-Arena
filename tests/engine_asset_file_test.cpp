#include <dxa/engine/assets/AssetFile.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace
{
using dxa::engine::asset::AnimationClip;
using dxa::engine::asset::AssetFormatError;
using dxa::engine::asset::Float2;
using dxa::engine::asset::Float3;
using dxa::engine::asset::Float4;
using dxa::engine::asset::Joint;
using dxa::engine::asset::Material;
using dxa::engine::asset::Matrix4;
using dxa::engine::asset::MeshPart;
using dxa::engine::asset::ModelAsset;
using dxa::engine::asset::Vertex;

constexpr Matrix4 IdentityMatrix{
    std::array<float, 16>{
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F}};

[[nodiscard]] ModelAsset MakeSkinnedTriangle()
{
    ModelAsset asset;
    asset.vertices = {
        Vertex{
            Float3{-1.0F, 0.0F, 0.0F},
            Float3{0.0F, 1.0F, 0.0F},
            Float2{0.0F, 1.0F},
            std::array<std::uint16_t, 4>{0, 0, 0, 0},
            std::array<float, 4>{1.0F, 0.0F, 0.0F, 0.0F}},
        Vertex{
            Float3{0.0F, 1.0F, 0.0F},
            Float3{0.0F, 1.0F, 0.0F},
            Float2{0.5F, 0.0F},
            std::array<std::uint16_t, 4>{0, 0, 0, 0},
            std::array<float, 4>{1.0F, 0.0F, 0.0F, 0.0F}},
        Vertex{
            Float3{1.0F, 0.0F, 0.0F},
            Float3{0.0F, 1.0F, 0.0F},
            Float2{1.0F, 1.0F},
            std::array<std::uint16_t, 4>{0, 0, 0, 0},
            std::array<float, 4>{1.0F, 0.0F, 0.0F, 0.0F}}};
    asset.indices = {0, 1, 2};
    asset.meshParts = {MeshPart{0, 3, 0}};
    asset.materials = {
        Material{"runner", Float4{0.8F, 0.9F, 1.0F, 1.0F}, "runner_albedo.dds"}};
    asset.joints = {Joint{"Root", -1, IdentityMatrix}};
    asset.animations = {
        AnimationClip{
            "Idle",
            1.0F,
            2.0F,
            2,
            std::vector<Matrix4>{
                IdentityMatrix,
                Matrix4{
                    std::array<float, 16>{
                        1.0F, 0.0F, 0.0F, 0.0F,
                        0.0F, 1.0F, 0.0F, 0.0F,
                        0.0F, 0.0F, 1.0F, 0.0F,
                        0.0F, 0.25F, 0.0F, 1.0F}}}}};
    return asset;
}

TEST(AssetFile, RoundTripsSkinnedModelData)
{
    const ModelAsset expected = MakeSkinnedTriangle();

    const std::vector<std::uint8_t> encoded =
        dxa::engine::asset::EncodeModelAsset(expected);
    const ModelAsset decoded = dxa::engine::asset::DecodeModelAsset(encoded);

    EXPECT_EQ(expected, decoded);
}

TEST(AssetFile, WritesVersionAndCountsInLittleEndian)
{
    const std::vector<std::uint8_t> encoded =
        dxa::engine::asset::EncodeModelAsset(MakeSkinnedTriangle());

    ASSERT_GE(encoded.size(), 12U);
    constexpr std::array<std::uint8_t, 12> ExpectedHeader{
        'D', 'X', 'A', 'M',
        1, 0,
        0, 0,
        3, 0, 0, 0};
    EXPECT_TRUE(std::equal(ExpectedHeader.begin(), ExpectedHeader.end(), encoded.begin()));
}

TEST(AssetFile, RejectsTruncatedPayload)
{
    std::vector<std::uint8_t> encoded =
        dxa::engine::asset::EncodeModelAsset(MakeSkinnedTriangle());
    encoded.pop_back();

    EXPECT_THROW(
        (void)dxa::engine::asset::DecodeModelAsset(encoded),
        AssetFormatError);
}

TEST(AssetFile, RejectsIndexOutsideVertexRange)
{
    ModelAsset asset = MakeSkinnedTriangle();
    asset.indices.back() = 3;

    EXPECT_THROW(
        (void)dxa::engine::asset::EncodeModelAsset(asset),
        std::invalid_argument);
}

TEST(AssetFile, RejectsIncompleteAnimationPalette)
{
    ModelAsset asset = MakeSkinnedTriangle();
    asset.animations.front().jointMatrices.pop_back();

    EXPECT_THROW(
        (void)dxa::engine::asset::EncodeModelAsset(asset),
        std::invalid_argument);
}

TEST(AssetFile, RejectsUnusedJointIndexOutsideSkeleton)
{
    ModelAsset asset = MakeSkinnedTriangle();
    asset.vertices.front().jointIndices[1] = 63;
    asset.vertices.front().jointWeights[1] = 0.0F;

    EXPECT_THROW(
        (void)dxa::engine::asset::EncodeModelAsset(asset),
        std::invalid_argument);
}

TEST(AssetFile, RejectsTexturePathOutsideModelDirectory)
{
    ModelAsset asset = MakeSkinnedTriangle();
    asset.materials.front().baseColorTexture = "../outside.dds";

    EXPECT_THROW(
        (void)dxa::engine::asset::EncodeModelAsset(asset),
        std::invalid_argument);
}
} // namespace
