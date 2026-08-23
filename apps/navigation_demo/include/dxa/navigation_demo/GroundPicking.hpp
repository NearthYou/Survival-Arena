#pragma once

#include <dxa/engine/InputState.hpp>
#include <dxa/engine/benchmark/StressScene.hpp>
#include <dxa/simulation/Math2.hpp>

#include <cstdint>
#include <optional>

namespace dxa::navigation_demo
{
[[nodiscard]] std::optional<dxa::simulation::Vec2> PointerGroundDestination(
    dxa::engine::PointerPosition pointer,
    std::uint32_t width,
    std::uint32_t height,
    const dxa::engine::benchmark::StressCamera& camera);
} // namespace dxa::navigation_demo
