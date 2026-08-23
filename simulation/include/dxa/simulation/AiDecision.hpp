#pragma once

#include <dxa/simulation/Math2.hpp>

namespace dxa::simulation
{
enum class AiArchetype
{
    Melee,
    Ranged
};

enum class AiCommandType
{
    Idle,
    MoveToTarget,
    MoveAwayFromTarget,
    Attack
};

struct AiBlackboard
{
    Vec2 selfPosition;
    Vec2 targetPosition;
    bool hasTarget = false;
    bool cooldownReady = false;
    float attackRange = 1.5F;
    float preferredRange = 8.0F;
    float retreatRange = 3.0F;
};

class FsmAiController
{
public:
    explicit FsmAiController(AiArchetype archetype) noexcept;

    [[nodiscard]] AiCommandType Tick(const AiBlackboard& blackboard) const;

private:
    AiArchetype archetype_;
};
} // namespace dxa::simulation
