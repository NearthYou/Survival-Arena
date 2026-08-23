#include <dxa/simulation/AiDecision.hpp>

#include "AiDecisionRules.hpp"

#include <cmath>
#include <stdexcept>

namespace dxa::simulation
{
namespace detail
{
void ValidateAiBlackboard(const AiBlackboard& blackboard)
{
    if (!IsFinite(blackboard.selfPosition)
        || !IsFinite(blackboard.targetPosition)
        || !std::isfinite(blackboard.attackRange)
        || blackboard.attackRange < 0.0F
        || !std::isfinite(blackboard.preferredRange)
        || blackboard.preferredRange < 0.0F
        || !std::isfinite(blackboard.retreatRange)
        || blackboard.retreatRange < 0.0F)
    {
        throw std::invalid_argument{"AI blackboard contains invalid movement values"};
    }
}

float TargetDistance(const AiBlackboard& blackboard)
{
    return Distance(blackboard.selfPosition, blackboard.targetPosition);
}
} // namespace detail

FsmAiController::FsmAiController(const AiArchetype archetype) noexcept
    : archetype_{archetype}
{
}

AiCommandType FsmAiController::Tick(const AiBlackboard& blackboard) const
{
    detail::ValidateAiBlackboard(blackboard);
    if (!blackboard.hasTarget)
    {
        return AiCommandType::Idle;
    }

    const float distanceToTarget = detail::TargetDistance(blackboard);
    if (!std::isfinite(distanceToTarget))
    {
        throw std::invalid_argument{"AI target distance must be finite"};
    }
    if (archetype_ == AiArchetype::Ranged
        && distanceToTarget < blackboard.retreatRange)
    {
        return AiCommandType::MoveAwayFromTarget;
    }
    if (distanceToTarget <= blackboard.attackRange
        && blackboard.cooldownReady)
    {
        return AiCommandType::Attack;
    }

    return AiCommandType::MoveToTarget;
}
} // namespace dxa::simulation
