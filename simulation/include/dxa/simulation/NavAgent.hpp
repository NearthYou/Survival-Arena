#pragma once

#include <dxa/simulation/NavMesh.hpp>

#include <cstddef>
#include <vector>

namespace dxa::simulation
{
enum class NavAgentState
{
    Idle,
    Moving,
    Arrived,
    InvalidDestination
};

class NavAgent
{
public:
    NavAgent(
        const NavMesh& mesh,
        Vec2 position,
        float speed,
        float stoppingDistance);

    [[nodiscard]] bool SetDestination(Vec2 destination);
    void Tick(float deltaSeconds);

    [[nodiscard]] Vec2 Position() const noexcept;
    [[nodiscard]] NavAgentState State() const noexcept;

private:
    const NavMesh& mesh_;
    Vec2 position_;
    float speed_ = 0.0F;
    float stoppingDistance_ = 0.0F;
    NavAgentState state_ = NavAgentState::Idle;
    std::vector<Vec2> waypoints_;
    std::size_t nextWaypoint_ = 0;
};
} // namespace dxa::simulation
