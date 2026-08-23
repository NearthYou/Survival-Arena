#include <dxa/simulation/AiDecision.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace
{
using dxa::simulation::AiArchetype;
using dxa::simulation::AiBlackboard;
using dxa::simulation::AiCommandType;
using dxa::simulation::FsmAiController;

[[nodiscard]] AiBlackboard NoTarget()
{
    return AiBlackboard{};
}

[[nodiscard]] AiBlackboard TargetAt(const float distance)
{
    AiBlackboard blackboard;
    blackboard.hasTarget = true;
    blackboard.cooldownReady = true;
    blackboard.targetPosition = {distance, 0.0F};
    return blackboard;
}

TEST(AiFsm, MeleeBaselineChoosesIdleChaseAndAttack)
{
    const FsmAiController controller{AiArchetype::Melee};

    EXPECT_EQ(AiCommandType::Idle, controller.Tick(NoTarget()));
    EXPECT_EQ(AiCommandType::MoveToTarget, controller.Tick(TargetAt(5.0F)));
    EXPECT_EQ(AiCommandType::Attack, controller.Tick(TargetAt(1.0F)));
    EXPECT_EQ(AiCommandType::Attack, controller.Tick(TargetAt(1.5F)));
}

TEST(AiFsm, RangedBaselineUsesTheSameThreeCommands)
{
    const FsmAiController controller{AiArchetype::Ranged};

    EXPECT_EQ(AiCommandType::Idle, controller.Tick(NoTarget()));
    EXPECT_EQ(AiCommandType::MoveToTarget, controller.Tick(TargetAt(5.0F)));
    EXPECT_EQ(AiCommandType::Attack, controller.Tick(TargetAt(1.0F)));
}

TEST(AiFsm, CooldownBlocksAttackWithoutStoppingChase)
{
    AiBlackboard input = TargetAt(1.0F);
    input.cooldownReady = false;

    EXPECT_EQ(
        AiCommandType::MoveToTarget,
        FsmAiController{AiArchetype::Melee}.Tick(input));
}

TEST(AiFsm, RejectsNonFinitePositionsAndInvalidRanges)
{
    const FsmAiController controller{AiArchetype::Melee};
    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    AiBlackboard input = TargetAt(2.0F);
    input.selfPosition.x = notANumber;
    EXPECT_THROW((void)controller.Tick(input), std::invalid_argument);

    input = TargetAt(2.0F);
    input.targetPosition.z = infinity;
    EXPECT_THROW((void)controller.Tick(input), std::invalid_argument);

    input = TargetAt(2.0F);
    input.attackRange = -0.1F;
    EXPECT_THROW((void)controller.Tick(input), std::invalid_argument);

    input = TargetAt(2.0F);
    input.preferredRange = notANumber;
    EXPECT_THROW((void)controller.Tick(input), std::invalid_argument);

    input = TargetAt(2.0F);
    input.retreatRange = infinity;
    EXPECT_THROW((void)controller.Tick(input), std::invalid_argument);
}
} // namespace
