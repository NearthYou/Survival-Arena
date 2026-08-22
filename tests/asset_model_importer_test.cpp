#include <dxa/asset_tool/ModelImporter.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <system_error>

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
} // namespace
