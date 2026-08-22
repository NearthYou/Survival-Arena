#include <dxa/engine/assets/AssetFile.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <string_view>

namespace dxa::engine::asset
{
namespace
{
constexpr std::array<std::uint8_t, 4> Magic{'D', 'X', 'A', 'M'};
constexpr std::size_t MaximumVertices = 1'000'000;
constexpr std::size_t MaximumIndices = 3'000'000;
constexpr std::size_t MaximumMeshParts = 65'536;
constexpr std::size_t MaximumMaterials = 4'096;
constexpr std::size_t MaximumAnimations = 1'024;
constexpr std::size_t MaximumSamplesPerClip = 65'536;
constexpr std::size_t MaximumStringBytes = 4'096;
constexpr std::size_t MaximumFileBytes = 256U * 1024U * 1024U;

class Writer
{
public:
    void WriteU16(const std::uint16_t value)
    {
        bytes_.push_back(static_cast<std::uint8_t>(value & 0xFFU));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    }

    void WriteU32(const std::uint32_t value)
    {
        for (std::uint32_t shift = 0; shift < 32U; shift += 8U)
        {
            bytes_.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
        }
    }

    void WriteI32(const std::int32_t value)
    {
        WriteU32(std::bit_cast<std::uint32_t>(value));
    }

    void WriteFloat(const float value)
    {
        WriteU32(std::bit_cast<std::uint32_t>(value));
    }

    void WriteBytes(const std::span<const std::uint8_t> bytes)
    {
        bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    }

    void WriteString(const std::string_view value)
    {
        WriteU32(static_cast<std::uint32_t>(value.size()));
        const auto* begin = reinterpret_cast<const std::uint8_t*>(value.data());
        WriteBytes(std::span<const std::uint8_t>{begin, value.size()});
    }

    [[nodiscard]] std::vector<std::uint8_t> Take()
    {
        return std::move(bytes_);
    }

private:
    std::vector<std::uint8_t> bytes_;
};

class Reader
{
public:
    explicit Reader(const std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::uint16_t ReadU16()
    {
        Require(2);
        const std::uint16_t value =
            static_cast<std::uint16_t>(bytes_[offset_])
            | static_cast<std::uint16_t>(bytes_[offset_ + 1]) << 8U;
        offset_ += 2;
        return value;
    }

    [[nodiscard]] std::uint32_t ReadU32()
    {
        Require(4);
        std::uint32_t value = 0;
        for (std::uint32_t byteIndex = 0; byteIndex < 4; ++byteIndex)
        {
            value |= static_cast<std::uint32_t>(bytes_[offset_ + byteIndex])
                << (byteIndex * 8U);
        }
        offset_ += 4;
        return value;
    }

    [[nodiscard]] std::int32_t ReadI32()
    {
        return std::bit_cast<std::int32_t>(ReadU32());
    }

    [[nodiscard]] float ReadFloat()
    {
        return std::bit_cast<float>(ReadU32());
    }

    [[nodiscard]] std::string ReadString()
    {
        const std::size_t size = ReadCount(MaximumStringBytes, "string");
        Require(size);
        const char* begin = reinterpret_cast<const char*>(bytes_.data() + offset_);
        std::string value{begin, size};
        offset_ += size;
        return value;
    }

    [[nodiscard]] std::size_t ReadCount(const std::size_t maximum, const char* label)
    {
        const std::uint32_t value = ReadU32();
        if (value > maximum)
        {
            throw AssetFormatError{std::string{label} + " count exceeds the format limit"};
        }
        return value;
    }

    void ExpectBytes(const std::span<const std::uint8_t> expected)
    {
        Require(expected.size());
        if (!std::equal(expected.begin(), expected.end(), bytes_.begin() + offset_))
        {
            throw AssetFormatError{"model asset magic does not match"};
        }
        offset_ += expected.size();
    }

    void RequireEnd() const
    {
        if (offset_ != bytes_.size())
        {
            throw AssetFormatError{"model asset contains trailing bytes"};
        }
    }

private:
    void Require(const std::size_t count) const
    {
        if (count > bytes_.size() - offset_)
        {
            throw AssetFormatError{"model asset payload is truncated"};
        }
    }

    std::span<const std::uint8_t> bytes_;
    std::size_t offset_ = 0;
};

[[nodiscard]] bool IsFinite(const Float2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool IsFinite(const Float3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool IsFinite(const Float4 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z)
        && std::isfinite(value.w);
}

[[nodiscard]] bool IsFinite(const Matrix4& value)
{
    return std::ranges::all_of(value.elements, [](const float element) {
        return std::isfinite(element);
    });
}

void RequireCount(const std::size_t count, const std::size_t maximum, const char* label)
{
    if (count > maximum || count > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::invalid_argument{std::string{label} + " count exceeds the format limit"};
    }
}

void RequireString(const std::string& value, const char* label)
{
    if (value.size() > MaximumStringBytes)
    {
        throw std::invalid_argument{std::string{label} + " exceeds the format limit"};
    }
}

void ValidateModelAsset(const ModelAsset& asset)
{
    RequireCount(asset.vertices.size(), MaximumVertices, "vertex");
    RequireCount(asset.indices.size(), MaximumIndices, "index");
    RequireCount(asset.meshParts.size(), MaximumMeshParts, "mesh part");
    RequireCount(asset.materials.size(), MaximumMaterials, "material");
    RequireCount(asset.joints.size(), MaximumSkinJoints, "joint");
    RequireCount(asset.animations.size(), MaximumAnimations, "animation");

    for (const Vertex& vertex : asset.vertices)
    {
        if (!IsFinite(vertex.position) || !IsFinite(vertex.normal) || !IsFinite(vertex.texcoord))
        {
            throw std::invalid_argument{"vertex contains a non-finite value"};
        }

        float weightSum = 0.0F;
        for (std::size_t influence = 0; influence < vertex.jointWeights.size(); ++influence)
        {
            const float weight = vertex.jointWeights[influence];
            if (!std::isfinite(weight) || weight < 0.0F)
            {
                throw std::invalid_argument{"vertex contains an invalid joint weight"};
            }
            if (weight > 0.0F && vertex.jointIndices[influence] >= asset.joints.size())
            {
                throw std::invalid_argument{"vertex references a joint outside the skeleton"};
            }
            weightSum += weight;
        }

        const bool unskinned = std::abs(weightSum) <= 0.0001F;
        const bool normalized = std::abs(weightSum - 1.0F) <= 0.001F;
        if (!unskinned && !normalized)
        {
            throw std::invalid_argument{"vertex joint weights must sum to zero or one"};
        }
    }

    for (const std::uint32_t index : asset.indices)
    {
        if (index >= asset.vertices.size())
        {
            throw std::invalid_argument{"index references a vertex outside the model"};
        }
    }

    for (const MeshPart& part : asset.meshParts)
    {
        if (part.firstIndex > asset.indices.size()
            || part.indexCount > asset.indices.size() - part.firstIndex)
        {
            throw std::invalid_argument{"mesh part index range is outside the model"};
        }
        if (part.materialIndex >= asset.materials.size())
        {
            throw std::invalid_argument{"mesh part references a material outside the model"};
        }
    }

    for (const Material& material : asset.materials)
    {
        RequireString(material.name, "material name");
        RequireString(material.baseColorTexture, "base color texture path");
        if (!IsFinite(material.baseColor))
        {
            throw std::invalid_argument{"material contains a non-finite base color"};
        }
    }

    for (std::size_t jointIndex = 0; jointIndex < asset.joints.size(); ++jointIndex)
    {
        const Joint& joint = asset.joints[jointIndex];
        RequireString(joint.name, "joint name");
        if (joint.parentIndex < -1
            || joint.parentIndex >= static_cast<std::int32_t>(asset.joints.size())
            || joint.parentIndex == static_cast<std::int32_t>(jointIndex))
        {
            throw std::invalid_argument{"joint parent index is invalid"};
        }
        if (!IsFinite(joint.inverseBind))
        {
            throw std::invalid_argument{"joint contains a non-finite inverse bind matrix"};
        }
    }

    for (const AnimationClip& clip : asset.animations)
    {
        RequireString(clip.name, "animation name");
        RequireCount(clip.sampleCount, MaximumSamplesPerClip, "animation sample");
        if (asset.joints.empty() || clip.sampleCount == 0 || !std::isfinite(clip.durationSeconds)
            || clip.durationSeconds <= 0.0F || !std::isfinite(clip.sampleRate)
            || clip.sampleRate <= 0.0F)
        {
            throw std::invalid_argument{"animation timing or skeleton is invalid"};
        }

        const std::size_t expectedMatrices =
            static_cast<std::size_t>(clip.sampleCount) * asset.joints.size();
        if (clip.jointMatrices.size() != expectedMatrices)
        {
            throw std::invalid_argument{"animation palette size does not match samples and joints"};
        }
        if (!std::ranges::all_of(clip.jointMatrices, [](const Matrix4& matrix) {
                return IsFinite(matrix);
            }))
        {
            throw std::invalid_argument{"animation contains a non-finite matrix"};
        }
    }
}

void WriteMatrix(Writer& writer, const Matrix4& matrix)
{
    for (const float element : matrix.elements)
    {
        writer.WriteFloat(element);
    }
}

[[nodiscard]] Matrix4 ReadMatrix(Reader& reader)
{
    Matrix4 matrix;
    for (float& element : matrix.elements)
    {
        element = reader.ReadFloat();
    }
    return matrix;
}

[[nodiscard]] std::uint32_t ToU32(const std::size_t value)
{
    return static_cast<std::uint32_t>(value);
}
} // namespace

std::vector<std::uint8_t> EncodeModelAsset(const ModelAsset& asset)
{
    ValidateModelAsset(asset);

    Writer writer;
    writer.WriteBytes(Magic);
    writer.WriteU16(ModelAssetVersion);
    writer.WriteU16(0);
    writer.WriteU32(ToU32(asset.vertices.size()));
    writer.WriteU32(ToU32(asset.indices.size()));
    writer.WriteU32(ToU32(asset.meshParts.size()));
    writer.WriteU32(ToU32(asset.materials.size()));
    writer.WriteU32(ToU32(asset.joints.size()));
    writer.WriteU32(ToU32(asset.animations.size()));

    for (const Vertex& vertex : asset.vertices)
    {
        writer.WriteFloat(vertex.position.x);
        writer.WriteFloat(vertex.position.y);
        writer.WriteFloat(vertex.position.z);
        writer.WriteFloat(vertex.normal.x);
        writer.WriteFloat(vertex.normal.y);
        writer.WriteFloat(vertex.normal.z);
        writer.WriteFloat(vertex.texcoord.x);
        writer.WriteFloat(vertex.texcoord.y);
        for (const std::uint16_t jointIndex : vertex.jointIndices)
        {
            writer.WriteU16(jointIndex);
        }
        for (const float weight : vertex.jointWeights)
        {
            writer.WriteFloat(weight);
        }
    }

    for (const std::uint32_t index : asset.indices)
    {
        writer.WriteU32(index);
    }
    for (const MeshPart& part : asset.meshParts)
    {
        writer.WriteU32(part.firstIndex);
        writer.WriteU32(part.indexCount);
        writer.WriteU32(part.materialIndex);
    }
    for (const Material& material : asset.materials)
    {
        writer.WriteString(material.name);
        writer.WriteFloat(material.baseColor.x);
        writer.WriteFloat(material.baseColor.y);
        writer.WriteFloat(material.baseColor.z);
        writer.WriteFloat(material.baseColor.w);
        writer.WriteString(material.baseColorTexture);
    }
    for (const Joint& joint : asset.joints)
    {
        writer.WriteString(joint.name);
        writer.WriteI32(joint.parentIndex);
        WriteMatrix(writer, joint.inverseBind);
    }
    for (const AnimationClip& clip : asset.animations)
    {
        writer.WriteString(clip.name);
        writer.WriteFloat(clip.durationSeconds);
        writer.WriteFloat(clip.sampleRate);
        writer.WriteU32(clip.sampleCount);
        for (const Matrix4& matrix : clip.jointMatrices)
        {
            WriteMatrix(writer, matrix);
        }
    }
    return writer.Take();
}

ModelAsset DecodeModelAsset(const std::span<const std::uint8_t> bytes)
{
    if (bytes.size() > MaximumFileBytes)
    {
        throw AssetFormatError{"model asset exceeds the file size limit"};
    }

    try
    {
        Reader reader{bytes};
        reader.ExpectBytes(Magic);
        const std::uint16_t version = reader.ReadU16();
        if (version != ModelAssetVersion)
        {
            throw AssetFormatError{"model asset version is not supported"};
        }
        if (reader.ReadU16() != 0)
        {
            throw AssetFormatError{"model asset reserved header bits are not zero"};
        }

        const std::size_t vertexCount = reader.ReadCount(MaximumVertices, "vertex");
        const std::size_t indexCount = reader.ReadCount(MaximumIndices, "index");
        const std::size_t meshPartCount = reader.ReadCount(MaximumMeshParts, "mesh part");
        const std::size_t materialCount = reader.ReadCount(MaximumMaterials, "material");
        const std::size_t jointCount = reader.ReadCount(MaximumSkinJoints, "joint");
        const std::size_t animationCount = reader.ReadCount(MaximumAnimations, "animation");

        ModelAsset asset;
        asset.vertices.resize(vertexCount);
        for (Vertex& vertex : asset.vertices)
        {
            vertex.position = Float3{reader.ReadFloat(), reader.ReadFloat(), reader.ReadFloat()};
            vertex.normal = Float3{reader.ReadFloat(), reader.ReadFloat(), reader.ReadFloat()};
            vertex.texcoord = Float2{reader.ReadFloat(), reader.ReadFloat()};
            for (std::uint16_t& jointIndex : vertex.jointIndices)
            {
                jointIndex = reader.ReadU16();
            }
            for (float& weight : vertex.jointWeights)
            {
                weight = reader.ReadFloat();
            }
        }

        asset.indices.resize(indexCount);
        for (std::uint32_t& index : asset.indices)
        {
            index = reader.ReadU32();
        }
        asset.meshParts.resize(meshPartCount);
        for (MeshPart& part : asset.meshParts)
        {
            part = MeshPart{reader.ReadU32(), reader.ReadU32(), reader.ReadU32()};
        }
        asset.materials.resize(materialCount);
        for (Material& material : asset.materials)
        {
            material.name = reader.ReadString();
            material.baseColor = Float4{
                reader.ReadFloat(), reader.ReadFloat(), reader.ReadFloat(), reader.ReadFloat()};
            material.baseColorTexture = reader.ReadString();
        }
        asset.joints.resize(jointCount);
        for (Joint& joint : asset.joints)
        {
            joint.name = reader.ReadString();
            joint.parentIndex = reader.ReadI32();
            joint.inverseBind = ReadMatrix(reader);
        }
        asset.animations.resize(animationCount);
        for (AnimationClip& clip : asset.animations)
        {
            clip.name = reader.ReadString();
            clip.durationSeconds = reader.ReadFloat();
            clip.sampleRate = reader.ReadFloat();
            clip.sampleCount = static_cast<std::uint32_t>(
                reader.ReadCount(MaximumSamplesPerClip, "animation sample"));
            const std::size_t matrixCount =
                static_cast<std::size_t>(clip.sampleCount) * jointCount;
            clip.jointMatrices.resize(matrixCount);
            for (Matrix4& matrix : clip.jointMatrices)
            {
                matrix = ReadMatrix(reader);
            }
        }
        reader.RequireEnd();
        ValidateModelAsset(asset);
        return asset;
    }
    catch (const AssetFormatError&)
    {
        throw;
    }
    catch (const std::invalid_argument& error)
    {
        throw AssetFormatError{error.what()};
    }
}

void SaveModelAsset(const std::filesystem::path& path, const ModelAsset& asset)
{
    const std::vector<std::uint8_t> bytes = EncodeModelAsset(asset);
    std::ofstream file{path, std::ios::binary | std::ios::trunc};
    if (!file)
    {
        throw std::runtime_error{"failed to open model asset for writing"};
    }
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file)
    {
        throw std::runtime_error{"failed to write model asset"};
    }
}

ModelAsset LoadModelAsset(const std::filesystem::path& path)
{
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    if (!file)
    {
        throw std::runtime_error{"failed to open model asset for reading"};
    }
    const std::streampos end = file.tellg();
    if (end < 0 || static_cast<std::uintmax_t>(end) > MaximumFileBytes)
    {
        throw AssetFormatError{"model asset exceeds the file size limit"};
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file && !bytes.empty())
    {
        throw std::runtime_error{"failed to read model asset"};
    }
    return DecodeModelAsset(bytes);
}
} // namespace dxa::engine::asset
