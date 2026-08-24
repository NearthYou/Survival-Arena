#pragma once

#include <dxa/engine/InputState.hpp>
#include <dxa/engine/benchmark/StressScene.hpp>

#include <cstdint>
#include <optional>

namespace dxa::engine
{
[[nodiscard]] std::optional<benchmark::SceneVector3>
PointerGroundDestination(
    PointerPosition pointer,
    std::uint32_t width,
    std::uint32_t height,
    const benchmark::StressCamera& camera);
} // namespace dxa::engine
