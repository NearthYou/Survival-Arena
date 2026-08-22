#pragma once

#include <dxa/engine/assets/AssetFile.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>

namespace dxa::asset_tool
{
struct JointInfluence
{
    std::uint16_t jointIndex = 0;
    float weight = 0.0F;
};

struct PackedInfluences
{
    std::array<std::uint16_t, 4> jointIndices{};
    std::array<float, 4> jointWeights{};
};

class ModelImportError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] PackedInfluences PackVertexInfluences(
    std::span<const JointInfluence> influences);
[[nodiscard]] dxa::engine::asset::ModelAsset ImportModel(
    const std::filesystem::path& sourcePath);
} // namespace dxa::asset_tool
