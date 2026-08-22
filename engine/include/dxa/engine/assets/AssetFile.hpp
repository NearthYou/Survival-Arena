#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace dxa::engine::asset
{
inline constexpr std::uint16_t ModelAssetVersion = 1;
inline constexpr std::size_t MaximumSkinJoints = 64;
inline constexpr std::size_t MaximumAnimationSamples = 65'536;

struct Float2
{
    float x = 0.0F;
    float y = 0.0F;

    [[nodiscard]] bool operator==(const Float2&) const = default;
};

struct Float3
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    [[nodiscard]] bool operator==(const Float3&) const = default;
};

struct Float4
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float w = 0.0F;

    [[nodiscard]] bool operator==(const Float4&) const = default;
};

struct Matrix4
{
    std::array<float, 16> elements{};

    [[nodiscard]] bool operator==(const Matrix4&) const = default;
};

struct Vertex
{
    Float3 position;
    Float3 normal;
    Float2 texcoord;
    std::array<std::uint16_t, 4> jointIndices{};
    std::array<float, 4> jointWeights{};

    [[nodiscard]] bool operator==(const Vertex&) const = default;
};

struct MeshPart
{
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t materialIndex = 0;

    [[nodiscard]] bool operator==(const MeshPart&) const = default;
};

struct Material
{
    std::string name;
    Float4 baseColor{1.0F, 1.0F, 1.0F, 1.0F};
    std::string baseColorTexture;

    [[nodiscard]] bool operator==(const Material&) const = default;
};

struct Joint
{
    std::string name;
    std::int32_t parentIndex = -1;
    Matrix4 inverseBind;

    [[nodiscard]] bool operator==(const Joint&) const = default;
};

struct AnimationClip
{
    std::string name;
    float durationSeconds = 0.0F;
    float sampleRate = 0.0F;
    std::uint32_t sampleCount = 0;
    std::vector<Matrix4> jointMatrices;

    [[nodiscard]] bool operator==(const AnimationClip&) const = default;
};

struct ModelAsset
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<MeshPart> meshParts;
    std::vector<Material> materials;
    std::vector<Joint> joints;
    std::vector<AnimationClip> animations;

    [[nodiscard]] bool operator==(const ModelAsset&) const = default;
};

class AssetFormatError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] std::vector<std::uint8_t> EncodeModelAsset(const ModelAsset& asset);
[[nodiscard]] ModelAsset DecodeModelAsset(std::span<const std::uint8_t> bytes);
void SaveModelAsset(const std::filesystem::path& path, const ModelAsset& asset);
[[nodiscard]] ModelAsset LoadModelAsset(const std::filesystem::path& path);
} // namespace dxa::engine::asset
