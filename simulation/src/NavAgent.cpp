#include <dxa/simulation/NavAgent.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace dxa::simulation
{
NavAgent::NavAgent(
    const NavMesh& mesh,
    const Vec2 position,
    const float speed,
    const float stoppingDistance)
    : mesh_{mesh},
      position_{position},
      speed_{speed},
      stoppingDistance_{stoppingDistance}
{
    if (!IsFinite(position)
        || !std::isfinite(speed)
        || speed <= 0.0F
        || !std::isfinite(stoppingDistance)
        || stoppingDistance < 0.0F)
    {
        throw std::invalid_argument{"NavAgent requires finite movement values"};
    }
    if (!mesh_.FindContainingTriangleGrid(position_).triangle.has_value())
    {
        throw std::invalid_argument{"NavAgent position must be on the NavMesh"};
    }
}

bool NavAgent::SetDestination(const Vec2 destination)
{
    if (!IsFinite(destination))
    {
        waypoints_.clear();
        nextWaypoint_ = 0;
        state_ = NavAgentState::InvalidDestination;
        return false;
    }

    const std::optional<NavPath> path = mesh_.FindPath(position_, destination);
    if (!path.has_value())
    {
        waypoints_.clear();
        nextWaypoint_ = 0;
        state_ = NavAgentState::InvalidDestination;
        return false;
    }

    if (Distance(position_, destination) <= stoppingDistance_)
    {
        position_ = destination;
        waypoints_.clear();
        nextWaypoint_ = 0;
        state_ = NavAgentState::Arrived;
        return true;
    }

    waypoints_ = path->waypoints;
    nextWaypoint_ = 1;
    state_ = NavAgentState::Moving;
    return true;
}

void NavAgent::Tick(const float deltaSeconds)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0F)
    {
        throw std::invalid_argument{"NavAgent delta must be finite and non-negative"};
    }
    if (deltaSeconds == 0.0F || state_ != NavAgentState::Moving)
    {
        return;
    }

    float movementRemaining = speed_ * deltaSeconds;
    while (movementRemaining > 0.0F && nextWaypoint_ < waypoints_.size())
    {
        const Vec2 target = waypoints_[nextWaypoint_];
        const Vec2 toTarget = target - position_;
        const float distance = Length(toTarget);
        if (distance == 0.0F)
        {
            ++nextWaypoint_;
            continue;
        }

        if (movementRemaining < distance)
        {
            position_ = position_ + toTarget * (movementRemaining / distance);
            movementRemaining = 0.0F;
            continue;
        }

        position_ = target;
        movementRemaining -= distance;
        ++nextWaypoint_;
    }

    if (nextWaypoint_ == waypoints_.size())
    {
        state_ = NavAgentState::Arrived;
    }
}

Vec2 NavAgent::Position() const noexcept
{
    return position_;
}

NavAgentState NavAgent::State() const noexcept
{
    return state_;
}
} // namespace dxa::simulation
