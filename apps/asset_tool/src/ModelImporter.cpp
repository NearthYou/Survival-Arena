#include <dxa/asset_tool/ModelImporter.hpp>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/postprocess.h>
#include <assimp/quaternion.h>
#include <assimp/scene.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dxa::asset_tool
{
namespace
{
using JointIndexMap = std::unordered_map<std::string, std::uint16_t>;

[[nodiscard]] dxa::engine::asset::Matrix4 ToAssetMatrix(const aiMatrix4x4& source)
{
    return dxa::engine::asset::Matrix4{
        std::array<float, 16>{
            source.a1, source.b1, source.c1, source.d1,
            source.a2, source.b2, source.c2, source.d2,
            source.a3, source.b3, source.c3, source.d3,
            source.a4, source.b4, source.c4, source.d4}};
}

[[nodiscard]] aiMatrix4x4 ToAssimpMatrix(const dxa::engine::asset::Matrix4& source)
{
    return aiMatrix4x4{
        source.elements[0], source.elements[4], source.elements[8], source.elements[12],
        source.elements[1], source.elements[5], source.elements[9], source.elements[13],
        source.elements[2], source.elements[6], source.elements[10], source.elements[14],
        source.elements[3], source.elements[7], source.elements[11], source.elements[15]};
}

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

[[nodiscard]] const aiNode* FindNode(
    const aiNode& node,
    const std::string_view name)
{
    if (name == node.mName.C_Str())
    {
        return &node;
    }
    for (std::uint32_t childIndex = 0; childIndex < node.mNumChildren; ++childIndex)
    {
        if (const aiNode* found = FindNode(*node.mChildren[childIndex], name); found != nullptr)
        {
            return found;
        }
    }
    return nullptr;
}

[[nodiscard]] JointIndexMap BuildSkeleton(
    const aiScene& scene,
    dxa::engine::asset::ModelAsset& destination)
{
    JointIndexMap jointIndices;
    for (std::uint32_t meshIndex = 0; meshIndex < scene.mNumMeshes; ++meshIndex)
    {
        const aiMesh& mesh = *scene.mMeshes[meshIndex];
        for (std::uint32_t boneIndex = 0; boneIndex < mesh.mNumBones; ++boneIndex)
        {
            const aiBone& bone = *mesh.mBones[boneIndex];
            const std::string name = bone.mName.C_Str();
            if (jointIndices.contains(name))
            {
                continue;
            }
            if (destination.joints.size() >= dxa::engine::asset::MaximumSkinJoints)
            {
                throw ModelImportError{"model skeleton exceeds 64 joints"};
            }
            const auto jointIndex = static_cast<std::uint16_t>(destination.joints.size());
            jointIndices.emplace(name, jointIndex);
            destination.joints.push_back(dxa::engine::asset::Joint{
                name,
                -1,
                ToAssetMatrix(bone.mOffsetMatrix)});
        }
    }

    for (std::size_t jointIndex = 0; jointIndex < destination.joints.size(); ++jointIndex)
    {
        const aiNode* node = FindNode(*scene.mRootNode, destination.joints[jointIndex].name);
        if (node == nullptr)
        {
            throw ModelImportError{"skeleton joint is missing from the Assimp node hierarchy"};
        }
        for (const aiNode* parent = node->mParent; parent != nullptr; parent = parent->mParent)
        {
            const auto found = jointIndices.find(parent->mName.C_Str());
            if (found != jointIndices.end())
            {
                destination.joints[jointIndex].parentIndex = found->second;
                break;
            }
        }
    }
    return jointIndices;
}

void AppendMesh(
    const aiMesh& source,
    const JointIndexMap& jointIndices,
    dxa::engine::asset::ModelAsset& destination)
{
    const std::size_t baseVertex = destination.vertices.size();
    const std::uint32_t firstIndex = static_cast<std::uint32_t>(destination.indices.size());
    std::vector<std::vector<JointInfluence>> influences(source.mNumVertices);

    for (std::uint32_t boneIndex = 0; boneIndex < source.mNumBones; ++boneIndex)
    {
        const aiBone& bone = *source.mBones[boneIndex];
        const auto found = jointIndices.find(bone.mName.C_Str());
        if (found == jointIndices.end())
        {
            throw ModelImportError{"mesh bone is missing from the skeleton"};
        }
        for (std::uint32_t weightIndex = 0; weightIndex < bone.mNumWeights; ++weightIndex)
        {
            const aiVertexWeight weight = bone.mWeights[weightIndex];
            if (weight.mVertexId >= influences.size())
            {
                throw ModelImportError{"bone weight references a vertex outside the mesh"};
            }
            influences[weight.mVertexId].push_back(JointInfluence{found->second, weight.mWeight});
        }
    }

    destination.vertices.reserve(destination.vertices.size() + source.mNumVertices);
    for (std::uint32_t vertexIndex = 0; vertexIndex < source.mNumVertices; ++vertexIndex)
    {
        const aiVector3D position = source.mVertices[vertexIndex];
        const aiVector3D normal = source.mNormals[vertexIndex];
        const aiVector3D texcoord = source.HasTextureCoords(0)
            ? source.mTextureCoords[0][vertexIndex]
            : aiVector3D{};
        const PackedInfluences packed = PackVertexInfluences(influences[vertexIndex]);
        destination.vertices.push_back(dxa::engine::asset::Vertex{
            dxa::engine::asset::Float3{position.x, position.y, position.z},
            dxa::engine::asset::Float3{normal.x, normal.y, normal.z},
            dxa::engine::asset::Float2{texcoord.x, texcoord.y},
            packed.jointIndices,
            packed.jointWeights});
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

[[nodiscard]] const aiNodeAnim* FindChannel(
    const aiAnimation& animation,
    const aiString& nodeName)
{
    for (std::uint32_t channelIndex = 0; channelIndex < animation.mNumChannels; ++channelIndex)
    {
        const aiNodeAnim* channel = animation.mChannels[channelIndex];
        if (channel->mNodeName == nodeName)
        {
            return channel;
        }
    }
    return nullptr;
}

[[nodiscard]] aiVector3D SampleVectorKeys(
    const aiVectorKey* keys,
    const std::uint32_t keyCount,
    const double time,
    const aiVector3D fallback)
{
    if (keyCount == 0)
    {
        return fallback;
    }
    if (keyCount == 1 || time <= keys[0].mTime)
    {
        return keys[0].mValue;
    }
    if (time >= keys[keyCount - 1].mTime)
    {
        return keys[keyCount - 1].mValue;
    }
    for (std::uint32_t keyIndex = 0; keyIndex + 1 < keyCount; ++keyIndex)
    {
        if (time <= keys[keyIndex + 1].mTime)
        {
            const aiVectorKey& left = keys[keyIndex];
            const aiVectorKey& right = keys[keyIndex + 1];
            const float factor = static_cast<float>(
                (time - left.mTime) / (right.mTime - left.mTime));
            return left.mValue + (right.mValue - left.mValue) * factor;
        }
    }
    return keys[keyCount - 1].mValue;
}

[[nodiscard]] aiQuaternion SampleRotationKeys(
    const aiQuatKey* keys,
    const std::uint32_t keyCount,
    const double time,
    const aiQuaternion& fallback)
{
    if (keyCount == 0)
    {
        return fallback;
    }
    if (keyCount == 1 || time <= keys[0].mTime)
    {
        return keys[0].mValue;
    }
    if (time >= keys[keyCount - 1].mTime)
    {
        return keys[keyCount - 1].mValue;
    }
    for (std::uint32_t keyIndex = 0; keyIndex + 1 < keyCount; ++keyIndex)
    {
        if (time <= keys[keyIndex + 1].mTime)
        {
            const aiQuatKey& left = keys[keyIndex];
            const aiQuatKey& right = keys[keyIndex + 1];
            const float factor = static_cast<float>(
                (time - left.mTime) / (right.mTime - left.mTime));
            aiQuaternion result;
            aiQuaternion::Interpolate(result, left.mValue, right.mValue, factor);
            result.Normalize();
            return result;
        }
    }
    return keys[keyCount - 1].mValue;
}

[[nodiscard]] aiMatrix4x4 SampleLocalTransform(
    const aiNode& node,
    const aiAnimation& animation,
    const double time)
{
    const aiNodeAnim* channel = FindChannel(animation, node.mName);
    if (channel == nullptr)
    {
        return node.mTransformation;
    }

    aiVector3D scaling;
    aiQuaternion rotation;
    aiVector3D position;
    node.mTransformation.Decompose(scaling, rotation, position);
    position = SampleVectorKeys(channel->mPositionKeys, channel->mNumPositionKeys, time, position);
    scaling = SampleVectorKeys(channel->mScalingKeys, channel->mNumScalingKeys, time, scaling);
    rotation = SampleRotationKeys(channel->mRotationKeys, channel->mNumRotationKeys, time, rotation);
    return aiMatrix4x4{scaling, rotation, position};
}

void EvaluateNodePalette(
    const aiNode& node,
    const aiMatrix4x4& parentTransform,
    const aiMatrix4x4& globalInverseTransform,
    const aiAnimation& animation,
    const double time,
    const JointIndexMap& jointIndices,
    const std::vector<dxa::engine::asset::Joint>& joints,
    std::vector<dxa::engine::asset::Matrix4>& palette,
    std::vector<bool>& visited)
{
    const aiMatrix4x4 globalTransform =
        parentTransform * SampleLocalTransform(node, animation, time);
    const auto found = jointIndices.find(node.mName.C_Str());
    if (found != jointIndices.end())
    {
        const std::size_t jointIndex = found->second;
        const aiMatrix4x4 offset = ToAssimpMatrix(joints[jointIndex].inverseBind);
        palette[jointIndex] = ToAssetMatrix(globalInverseTransform * globalTransform * offset);
        visited[jointIndex] = true;
    }

    for (std::uint32_t childIndex = 0; childIndex < node.mNumChildren; ++childIndex)
    {
        EvaluateNodePalette(
            *node.mChildren[childIndex],
            globalTransform,
            globalInverseTransform,
            animation,
            time,
            jointIndices,
            joints,
            palette,
            visited);
    }
}

[[nodiscard]] dxa::engine::asset::AnimationClip BakeAnimation(
    const aiScene& scene,
    const aiAnimation& animation,
    const JointIndexMap& jointIndices,
    const std::vector<dxa::engine::asset::Joint>& joints,
    const float sampleRate)
{
    const double ticksPerSecond = animation.mTicksPerSecond > 0.0
        ? animation.mTicksPerSecond
        : 25.0;
    const double durationSeconds = animation.mDuration / ticksPerSecond;
    if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0)
    {
        throw ModelImportError{"animation duration is invalid"};
    }
    const double sampleCountValue = std::ceil(durationSeconds * sampleRate) + 1.0;
    if (sampleCountValue > dxa::engine::asset::MaximumAnimationSamples)
    {
        throw ModelImportError{"animation sample count exceeds the format limit"};
    }
    const auto sampleCount = static_cast<std::uint32_t>(sampleCountValue);

    dxa::engine::asset::AnimationClip clip;
    clip.name = animation.mName.length > 0 ? animation.mName.C_Str() : "Animation";
    clip.durationSeconds = static_cast<float>(durationSeconds);
    clip.sampleRate = sampleRate;
    clip.sampleCount = sampleCount;
    clip.jointMatrices.reserve(static_cast<std::size_t>(sampleCount) * joints.size());

    aiMatrix4x4 globalInverseTransform = scene.mRootNode->mTransformation;
    globalInverseTransform.Inverse();
    for (std::uint32_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        const double sampleSeconds = std::min(
            static_cast<double>(sampleIndex) / sampleRate,
            durationSeconds);
        std::vector<dxa::engine::asset::Matrix4> palette(jointIndices.size());
        std::vector<bool> visited(jointIndices.size(), false);
        EvaluateNodePalette(
            *scene.mRootNode,
            aiMatrix4x4{},
            globalInverseTransform,
            animation,
            sampleSeconds * ticksPerSecond,
            jointIndices,
            joints,
            palette,
            visited);
        if (!std::ranges::all_of(visited, [](const bool value) { return value; }))
        {
            throw ModelImportError{"animation hierarchy did not visit every skeleton joint"};
        }
        clip.jointMatrices.insert(clip.jointMatrices.end(), palette.begin(), palette.end());
    }
    return clip;
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

dxa::engine::asset::ModelAsset ImportModel(
    const std::filesystem::path& sourcePath,
    const ModelImportOptions& options)
{
    if (!std::isfinite(options.animationSampleRate) || options.animationSampleRate <= 0.0F
        || options.animationSampleRate > 240.0F)
    {
        throw ModelImportError{"animation sample rate must be between 0 and 240 Hz"};
    }

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
    const JointIndexMap jointIndices = BuildSkeleton(*scene, model);
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
        AppendMesh(mesh, jointIndices, model);
    }
    if (model.vertices.empty() || model.indices.empty())
    {
        throw ModelImportError{"Assimp scene does not contain a renderable triangle mesh"};
    }
    if (!model.joints.empty())
    {
        model.animations.reserve(scene->mNumAnimations);
        for (std::uint32_t animationIndex = 0; animationIndex < scene->mNumAnimations; ++animationIndex)
        {
            model.animations.push_back(BakeAnimation(
                *scene,
                *scene->mAnimations[animationIndex],
                jointIndices,
                model.joints,
                options.animationSampleRate));
        }
    }
    return model;
}
} // namespace dxa::asset_tool
