#include <dxa/asset_tool/ModelImporter.hpp>

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace
{
class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path()
            / ("dxa-model-importer-" + std::to_string(suffix));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void WriteTextFile(const std::filesystem::path& path, const std::string& contents)
{
    std::ofstream file{path, std::ios::binary | std::ios::trunc};
    ASSERT_TRUE(file) << path;
    file << contents;
    ASSERT_TRUE(file) << path;
}

void AppendU16(std::vector<std::uint8_t>& bytes, const std::uint16_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void AppendFloat(std::vector<std::uint8_t>& bytes, const float value)
{
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    for (std::uint32_t shift = 0; shift < 32U; shift += 8U)
    {
        bytes.push_back(static_cast<std::uint8_t>((bits >> shift) & 0xFFU));
    }
}

void AppendFloats(
    std::vector<std::uint8_t>& bytes,
    const std::initializer_list<float> values)
{
    for (const float value : values)
    {
        AppendFloat(bytes, value);
    }
}

void WriteBinaryFile(
    const std::filesystem::path& path,
    const std::span<const std::uint8_t> contents)
{
    std::ofstream file{path, std::ios::binary | std::ios::trunc};
    ASSERT_TRUE(file) << path;
    file.write(
        reinterpret_cast<const char*>(contents.data()),
        static_cast<std::streamsize>(contents.size()));
    ASSERT_TRUE(file) << path;
}

void WriteAnimatedGltf(const std::filesystem::path& directory)
{
    std::vector<std::uint8_t> binary;
    binary.reserve(272);
    AppendFloats(binary, {-1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 0.0F});
    AppendFloats(binary, {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F});
    AppendFloats(binary, {0.0F, 1.0F, 0.5F, 0.0F, 1.0F, 1.0F});
    for (std::size_t vertex = 0; vertex < 3; ++vertex)
    {
        AppendU16(binary, 0);
        AppendU16(binary, 0);
        AppendU16(binary, 0);
        AppendU16(binary, 0);
    }
    for (std::size_t vertex = 0; vertex < 3; ++vertex)
    {
        AppendFloats(binary, {1.0F, 0.0F, 0.0F, 0.0F});
    }
    AppendU16(binary, 0);
    AppendU16(binary, 1);
    AppendU16(binary, 2);
    AppendU16(binary, 0);
    AppendFloats(
        binary,
        {1.0F, 0.0F, 0.0F, 0.0F,
         0.0F, 1.0F, 0.0F, 0.0F,
         0.0F, 0.0F, 1.0F, 0.0F,
         0.0F, 0.0F, 0.0F, 1.0F});
    AppendFloats(binary, {0.0F, 1.0F});
    AppendFloats(binary, {0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F});
    ASSERT_EQ(272U, binary.size());
    WriteBinaryFile(directory / "animated.bin", binary);

    WriteTextFile(
        directory / "animated.gltf",
        R"json({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0, 1]}],
  "nodes": [
    {"name": "Root"},
    {"name": "Runner", "mesh": 0, "skin": 0}
  ],
  "buffers": [{"uri": "animated.bin", "byteLength": 272}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36, "target": 34962},
    {"buffer": 0, "byteOffset": 36, "byteLength": 36, "target": 34962},
    {"buffer": 0, "byteOffset": 72, "byteLength": 24, "target": 34962},
    {"buffer": 0, "byteOffset": 96, "byteLength": 24, "target": 34962},
    {"buffer": 0, "byteOffset": 120, "byteLength": 48, "target": 34962},
    {"buffer": 0, "byteOffset": 168, "byteLength": 6, "target": 34963},
    {"buffer": 0, "byteOffset": 176, "byteLength": 64},
    {"buffer": 0, "byteOffset": 240, "byteLength": 8},
    {"buffer": 0, "byteOffset": 248, "byteLength": 24}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [-1,0,0], "max": [1,1,0]},
    {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3"},
    {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2"},
    {"bufferView": 3, "componentType": 5123, "count": 3, "type": "VEC4"},
    {"bufferView": 4, "componentType": 5126, "count": 3, "type": "VEC4"},
    {"bufferView": 5, "componentType": 5123, "count": 3, "type": "SCALAR"},
    {"bufferView": 6, "componentType": 5126, "count": 1, "type": "MAT4"},
    {"bufferView": 7, "componentType": 5126, "count": 2, "type": "SCALAR", "min": [0], "max": [1]},
    {"bufferView": 8, "componentType": 5126, "count": 2, "type": "VEC3"}
  ],
  "materials": [{"name": "RunnerMaterial", "pbrMetallicRoughness": {"baseColorFactor": [0.8,0.9,1.0,1.0]}}],
  "meshes": [{"primitives": [{
    "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2, "JOINTS_0": 3, "WEIGHTS_0": 4},
    "indices": 5,
    "material": 0
  }]}],
  "skins": [{"inverseBindMatrices": 6, "joints": [0], "skeleton": 0}],
  "animations": [{
    "name": "Move",
    "samplers": [{"input": 7, "output": 8, "interpolation": "LINEAR"}],
    "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}]
  }]
})json");
}

TEST(ModelImporter, ImportsObjGeometryMaterialAndTextureReference)
{
    TemporaryDirectory directory;
    WriteTextFile(
        directory.Path() / "triangle.mtl",
        "newmtl RunnerMaterial\n"
        "Kd 0.25 0.50 0.75\n"
        "d 1.0\n"
        "map_Kd textures/runner.png\n");
    WriteTextFile(
        directory.Path() / "triangle.obj",
        "mtllib triangle.mtl\n"
        "o Triangle\n"
        "v -1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "vt 0.0 1.0\n"
        "vt 0.5 0.0\n"
        "vt 1.0 1.0\n"
        "vn 0.0 0.0 1.0\n"
        "usemtl RunnerMaterial\n"
        "f 1/1/1 2/2/1 3/3/1\n");

    const dxa::engine::asset::ModelAsset model =
        dxa::asset_tool::ImportModel(directory.Path() / "triangle.obj");

    ASSERT_EQ(3U, model.vertices.size());
    ASSERT_EQ(3U, model.indices.size());
    ASSERT_EQ(1U, model.meshParts.size());
    const auto& part = model.meshParts.front();
    ASSERT_LT(part.materialIndex, model.materials.size());
    const auto& material = model.materials[part.materialIndex];
    EXPECT_EQ("RunnerMaterial", material.name);
    EXPECT_NEAR(0.25F, material.baseColor.x, 0.001F);
    EXPECT_NEAR(0.50F, material.baseColor.y, 0.001F);
    EXPECT_NEAR(0.75F, material.baseColor.z, 0.001F);
    EXPECT_EQ("runner.dds", material.baseColorTexture);
    EXPECT_TRUE(model.joints.empty());
    EXPECT_TRUE(model.animations.empty());
}

TEST(ModelImporter, PacksStrongestFourInfluencesAndNormalizesWeights)
{
    constexpr std::array influences{
        dxa::asset_tool::JointInfluence{0, 0.1F},
        dxa::asset_tool::JointInfluence{1, 0.2F},
        dxa::asset_tool::JointInfluence{2, 0.3F},
        dxa::asset_tool::JointInfluence{3, 0.4F},
        dxa::asset_tool::JointInfluence{4, 0.5F}};

    const dxa::asset_tool::PackedInfluences packed =
        dxa::asset_tool::PackVertexInfluences(influences);

    EXPECT_EQ((std::array<std::uint16_t, 4>{4, 3, 2, 1}), packed.jointIndices);
    EXPECT_NEAR(0.3571428F, packed.jointWeights[0], 0.0001F);
    EXPECT_NEAR(0.2857142F, packed.jointWeights[1], 0.0001F);
    EXPECT_NEAR(0.2142857F, packed.jointWeights[2], 0.0001F);
    EXPECT_NEAR(0.1428571F, packed.jointWeights[3], 0.0001F);
}

TEST(ModelImporter, RejectsMissingSourceFile)
{
    TemporaryDirectory directory;

    EXPECT_THROW(
        (void)dxa::asset_tool::ImportModel(directory.Path() / "missing.fbx"),
        dxa::asset_tool::ModelImportError);
}

TEST(ModelImporter, BakesSkinnedAnimationAtRequestedSampleRate)
{
    TemporaryDirectory directory;
    WriteAnimatedGltf(directory.Path());

    const dxa::engine::asset::ModelAsset model = dxa::asset_tool::ImportModel(
        directory.Path() / "animated.gltf",
        dxa::asset_tool::ModelImportOptions{2.0F});

    ASSERT_EQ(1U, model.joints.size());
    EXPECT_EQ("Root", model.joints.front().name);
    EXPECT_EQ(-1, model.joints.front().parentIndex);
    ASSERT_FALSE(model.vertices.empty());
    EXPECT_EQ(0, model.vertices.front().jointIndices[0]);
    EXPECT_NEAR(1.0F, model.vertices.front().jointWeights[0], 0.0001F);

    ASSERT_EQ(1U, model.animations.size());
    const auto& clip = model.animations.front();
    EXPECT_EQ("Move", clip.name);
    EXPECT_NEAR(1.0F, clip.durationSeconds, 0.001F);
    EXPECT_NEAR(2.0F, clip.sampleRate, 0.001F);
    ASSERT_EQ(3U, clip.sampleCount);
    ASSERT_EQ(3U, clip.jointMatrices.size());
    EXPECT_NEAR(0.0F, clip.jointMatrices[0].elements[13], 0.001F);
    EXPECT_NEAR(0.5F, clip.jointMatrices[1].elements[13], 0.001F);
    EXPECT_NEAR(1.0F, clip.jointMatrices[2].elements[13], 0.001F);
}
} // namespace
