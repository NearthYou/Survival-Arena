#pragma once

#include <dxa/engine/benchmark/StressScene.hpp>

#include <array>
#include <optional>

namespace dxa::engine
{
struct SceneCharacterState
{
    benchmark::SceneVector3 position;
    bool active = true;

    [[nodiscard]] bool operator==(const SceneCharacterState&) const = default;
};

struct RuntimeInputFrame
{
    std::optional<benchmark::SceneVector3> moveDestination;
};

struct RuntimeSceneFrame
{
    RuntimeSceneFrame() noexcept
    {
        for (SceneCharacterState& state : players)
        {
            state.active = false;
        }
        for (SceneCharacterState& state : ai)
        {
            state.active = false;
        }
    }

    benchmark::SceneVector3 controlledPlayer;
    std::array<SceneCharacterState, benchmark::PlayerCount> players{};
    std::array<SceneCharacterState, benchmark::AiCount> ai{};
    float zoneRadius = 128.0F;
};

class IRuntimeSceneController
{
public:
    virtual ~IRuntimeSceneController() = default;
    virtual void FixedUpdate(const RuntimeInputFrame& input) = 0;
    [[nodiscard]] virtual RuntimeSceneFrame SampleScene() = 0;
    [[nodiscard]] virtual bool ShouldClose() const noexcept
    {
        return false;
    }
};
} // namespace dxa::engine
