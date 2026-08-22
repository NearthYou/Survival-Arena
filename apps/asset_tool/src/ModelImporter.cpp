#include <dxa/asset_tool/ModelImporter.hpp>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace dxa::asset_tool
{
namespace
{
[[nodiscard]] std::string ToUtf8(const std::filesystem::path& path)
{
    const std::u8string encoded = path.u8string();
    return std::string{
        reinterpret_cast<const char*>(encoded.data()),
        encoded.size()};
}

[[nodiscard]] std::string TextureRuntimeName(const aiString& importedPath)
{
    const std::string value = importedPath.C_Str();
    if (value.empty())
    {
        return {};
    }
    if (value.front() == '*')
    {
        throw ModelImportError{"embedded textures are not supported by the asset cooker"};
    }

    std::filesystem::path path{value};
    path = path.filename();
    path.replace_extension(".dds");
    return path.generic_string();
}

[[nodiscard]] dxa::engine::asset::Material ImportMaterial(const aiMaterial& source)
{
    dxa::engine::asset::Material material;

    aiString name;
    if (source.Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
    {
        material.name = name.C_Str();
    }

    aiColor4D color{1.0F, 1.0F, 1.0F, 1.0F};
    if (aiGetMaterialColor(&source, AI_MATKEY_BASE_COLOR, &color) != AI_SUCCESS)
    {
        (void)aiGetMaterialColor(&source, AI_MATKEY_COLOR_DIFFUSE, &color);
    }
    material.baseColor = dxa::engine::asset::Float4{color.r, color.g, color.b, color.a};

    aiString texturePath;
    if (source.GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) != AI_SUCCESS)
    {
        (void)source.GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
    }
    material.baseColorTexture = TextureRuntimeName(texturePath);
    return material;
}

void AppendMesh(const aiMesh& source, dxa::engine::asset::ModelAsset& destination)
{
    const std::size_t baseVertex = destination.vertices.size();
    const std::uint32_t firstIndex = static_cast<std::uint32_t>(destination.indices.size());

    destination.vertices.reserve(destination.vertices.size() + source.mNumVertices);
    for (std::uint32_t vertexIndex = 0; vertexIndex < source.mNumVertices; ++vertexIndex)
    {
        const aiVector3D position = source.mVertices[vertexIndex];
        const aiVector3D normal = source.mNormals[vertexIndex];
        const aiVector3D texcoord = source.HasTextureCoords(0)
            ? source.mTextureCoords[0][vertexIndex]
            : aiVector3D{};
        destination.vertices.push_back(dxa::engine::asset::Vertex{
            dxa::engine::asset::Float3{position.x, position.y, position.z},
            dxa::engine::asset::Float3{normal.x, normal.y, normal.z},
            dxa::engine::asset::Float2{texcoord.x, texcoord.y},
            {},
            {}});
    }

    for (std::uint32_t faceIndex = 0; faceIndex < source.mNumFaces; ++faceIndex)
    {
        const aiFace& face = source.mFaces[faceIndex];
        if (face.mNumIndices != 3)
        {
            throw ModelImportError{"Assimp returned a non-triangle face after triangulation"};
        }
        for (std::uint32_t corner = 0; corner < face.mNumIndices; ++corner)
        {
            destination.indices.push_back(
                static_cast<std::uint32_t>(baseVertex + face.mIndices[corner]));
        }
    }

    destination.meshParts.push_back(dxa::engine::asset::MeshPart{
        firstIndex,
        static_cast<std::uint32_t>(destination.indices.size()) - firstIndex,
        source.mMaterialIndex});
}
} // namespace

PackedInfluences PackVertexInfluences(const std::span<const JointInfluence> influences)
{
    std::array<float, dxa::engine::asset::MaximumSkinJoints> weights{};
    for (const JointInfluence influence : influences)
    {
        if (influence.jointIndex >= weights.size() || !std::isfinite(influence.weight)
            || influence.weight < 0.0F)
        {
            throw ModelImportError{"joint influence is outside the supported range"};
        }
        weights[influence.jointIndex] += influence.weight;
    }

    std::vector<JointInfluence> ranked;
    ranked.reserve(weights.size());
    for (std::size_t jointIndex = 0; jointIndex < weights.size(); ++jointIndex)
    {
        if (weights[jointIndex] > 0.0F)
        {
            ranked.push_back(JointInfluence{
                static_cast<std::uint16_t>(jointIndex),
                weights[jointIndex]});
        }
    }
    std::ranges::sort(ranked, [](const JointInfluence left, const JointInfluence right) {
        if (left.weight == right.weight)
        {
            return left.jointIndex < right.jointIndex;
        }
        return left.weight > right.weight;
    });
    if (ranked.size() > 4)
    {
        ranked.resize(4);
    }

    float totalWeight = 0.0F;
    for (const JointInfluence influence : ranked)
    {
        totalWeight += influence.weight;
    }

    PackedInfluences packed;
    for (std::size_t influenceIndex = 0; influenceIndex < ranked.size(); ++influenceIndex)
    {
        packed.jointIndices[influenceIndex] = ranked[influenceIndex].jointIndex;
        packed.jointWeights[influenceIndex] = ranked[influenceIndex].weight / totalWeight;
    }
    return packed;
}

dxa::engine::asset::ModelAsset ImportModel(const std::filesystem::path& sourcePath)
{
    Assimp::Importer importer;
    constexpr unsigned int Flags =
        aiProcess_Triangulate
        | aiProcess_JoinIdenticalVertices
        | aiProcess_GenSmoothNormals
        | aiProcess_ImproveCacheLocality
        | aiProcess_SortByPType
        | aiProcess_ConvertToLeftHanded;
    const aiScene* scene = importer.ReadFile(ToUtf8(sourcePath), Flags);
    if (scene == nullptr || scene->mRootNode == nullptr
        || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) != 0)
    {
        throw ModelImportError{"Assimp import failed: " + std::string{importer.GetErrorString()}};
    }

    dxa::engine::asset::ModelAsset model;
    model.materials.reserve(scene->mNumMaterials);
    for (std::uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
    {
        model.materials.push_back(ImportMaterial(*scene->mMaterials[materialIndex]));
    }
    model.meshParts.reserve(scene->mNumMeshes);
    for (std::uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
    {
        const aiMesh& mesh = *scene->mMeshes[meshIndex];
        if ((mesh.mPrimitiveTypes & aiPrimitiveType_TRIANGLE) == 0 || mesh.mNumVertices == 0)
        {
            continue;
        }
        AppendMesh(mesh, model);
    }
    if (model.vertices.empty() || model.indices.empty())
    {
        throw ModelImportError{"Assimp scene does not contain a renderable triangle mesh"};
    }
    return model;
}
} // namespace dxa::asset_tool
