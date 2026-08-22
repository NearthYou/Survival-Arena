#pragma once

#include <dxa/engine/assets/AssetFile.hpp>

#include <cstddef>
#include <span>

namespace dxa::engine::asset
{
[[nodiscard]] std::span<const Matrix4> SampleAnimationPalette(
    const ModelAsset& asset,
    std::size_t clipIndex,
    double timeSeconds);
} // namespace dxa::engine::asset
