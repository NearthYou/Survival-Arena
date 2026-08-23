#include <dxa/simulation/AiDecision.hpp>

#include <cmath>
#include <stdexcept>

namespace dxa::simulation
{
namespace
{
void ValidateBlackboard(const AiBlackboard& blackboard)
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
} // namespace

FsmAiController::FsmAiController(const AiArchetype archetype) noexcept
    : archetype_{archetype}
{
}

AiCommandType FsmAiController::Tick(const AiBlackboard& blackboard) const
{
    ValidateBlackboard(blackboard);
    if (!blackboard.hasTarget)
    {
        return AiCommandType::Idle;
    }

    const float distanceToTarget = Distance(
        blackboard.selfPosition,
        blackboard.targetPosition);
    if (!std::isfinite(distanceToTarget))
    {
        throw std::invalid_argument{"AI target distance must be finite"};
    }
    if (distanceToTarget <= blackboard.attackRange
        && blackboard.cooldownReady)
    {
        return AiCommandType::Attack;
    }

    static_cast<void>(archetype_);
    return AiCommandType::MoveToTarget;
}
} // namespace dxa::simulation
