#include <dxa/engine/assets/AnimationPlayback.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dxa::engine::asset
{
std::span<const Matrix4> SampleAnimationPalette(
    const ModelAsset& asset,
    const std::size_t clipIndex,
    const double timeSeconds)
{
    if (asset.animations.empty() || asset.joints.empty())
    {
        return {};
    }
    if (clipIndex >= asset.animations.size())
    {
        throw std::out_of_range{"animation clip index is outside the model"};
    }

    const AnimationClip& clip = asset.animations[clipIndex];
    const std::size_t expectedMatrices =
        static_cast<std::size_t>(clip.sampleCount) * asset.joints.size();
    if (clip.sampleCount == 0 || clip.durationSeconds <= 0.0F || clip.sampleRate <= 0.0F
        || clip.jointMatrices.size() != expectedMatrices)
    {
        throw std::invalid_argument{"animation clip palette is inconsistent"};
    }

    const double nonNegativeTime = std::isfinite(timeSeconds)
        ? std::max(0.0, timeSeconds)
        : 0.0;
    const double loopedTime = std::fmod(nonNegativeTime, clip.durationSeconds);
    const std::size_t sampleIndex = std::min(
        static_cast<std::size_t>(loopedTime * clip.sampleRate),
        static_cast<std::size_t>(clip.sampleCount - 1));
    const std::size_t paletteOffset = sampleIndex * asset.joints.size();
    return std::span<const Matrix4>{
        clip.jointMatrices.data() + paletteOffset,
        asset.joints.size()};
}
} // namespace dxa::engine::asset
