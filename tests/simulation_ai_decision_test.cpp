#include <dxa/simulation/AiDecision.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace
{
using dxa::simulation::AiArchetype;
using dxa::simulation::AiBlackboard;
using dxa::simulation::AiCommandType;
using dxa::simulation::BehaviorTreeAiController;
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

[[nodiscard]] std::vector<AiBlackboard> AllDecisionScenarios()
{
    std::vector<AiBlackboard> scenarios;
    scenarios.push_back(NoTarget());
    scenarios.push_back(TargetAt(5.0F));
    scenarios.push_back(TargetAt(1.0F));

    AiBlackboard cooldownBlocked = TargetAt(1.0F);
    cooldownBlocked.cooldownReady = false;
    scenarios.push_back(cooldownBlocked);

    AiBlackboard retreatBoundary = TargetAt(3.0F);
    retreatBoundary.attackRange = 10.0F;
    scenarios.push_back(retreatBoundary);

    AiBlackboard attackOutsideRetreat = TargetAt(4.0F);
    attackOutsideRetreat.attackRange = 10.0F;
    scenarios.push_back(attackOutsideRetreat);

    AiBlackboard closeAttack = TargetAt(1.0F);
    closeAttack.retreatRange = 0.5F;
    scenarios.push_back(closeAttack);
    return scenarios;
}

TEST(AiFsm, MeleeBaselineChoosesIdleChaseAndAttack)
{
    const FsmAiController controller{AiArchetype::Melee};

    EXPECT_EQ(AiCommandType::Idle, controller.Tick(NoTarget()));
    EXPECT_EQ(AiCommandType::MoveToTarget, controller.Tick(TargetAt(5.0F)));
    EXPECT_EQ(AiCommandType::Attack, controller.Tick(TargetAt(1.0F)));
    EXPECT_EQ(AiCommandType::Attack, controller.Tick(TargetAt(1.5F)));
}

TEST(AiFsm, RangedUsesIdleChaseAndAttackOutsideRetreatRange)
{
    const FsmAiController controller{AiArchetype::Ranged};
    AiBlackboard attackInput = TargetAt(1.0F);
    attackInput.retreatRange = 0.5F;

    EXPECT_EQ(AiCommandType::Idle, controller.Tick(NoTarget()));
    EXPECT_EQ(AiCommandType::MoveToTarget, controller.Tick(TargetAt(5.0F)));
    EXPECT_EQ(AiCommandType::Attack, controller.Tick(attackInput));
}

TEST(AiFsm, CooldownBlocksAttackWithoutStoppingChase)
{
    AiBlackboard input = TargetAt(1.0F);
    input.cooldownReady = false;

    EXPECT_EQ(
        AiCommandType::MoveToTarget,
        FsmAiController{AiArchetype::Melee}.Tick(input));
}

TEST(AiFsm, RangedRetreatsBeforeConsideringAttack)
{
    AiBlackboard input = TargetAt(1.0F);
    input.retreatRange = 3.0F;
    input.attackRange = 10.0F;

    EXPECT_EQ(
        AiCommandType::MoveAwayFromTarget,
        FsmAiController{AiArchetype::Ranged}.Tick(input));
    EXPECT_EQ(
        AiCommandType::Attack,
        FsmAiController{AiArchetype::Melee}.Tick(input));

    input.cooldownReady = false;
    EXPECT_EQ(
        AiCommandType::MoveAwayFromTarget,
        FsmAiController{AiArchetype::Ranged}.Tick(input));
}

TEST(AiFsm, RangedCanAttackAtRetreatBoundary)
{
    AiBlackboard input = TargetAt(3.0F);
    input.retreatRange = 3.0F;
    input.attackRange = 10.0F;

    EXPECT_EQ(
        AiCommandType::Attack,
        FsmAiController{AiArchetype::Ranged}.Tick(input));
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

TEST(AiBehaviorTree, MatchesFsmForMeleeAndRangedScenarios)
{
    const std::vector<AiBlackboard> scenarios = AllDecisionScenarios();
    for (const AiArchetype archetype : {
             AiArchetype::Melee,
             AiArchetype::Ranged})
    {
        const FsmAiController fsm{archetype};
        const BehaviorTreeAiController tree{archetype};
        for (std::size_t index = 0; index < scenarios.size(); ++index)
        {
            EXPECT_EQ(fsm.Tick(scenarios[index]), tree.Tick(scenarios[index]))
                << "scenario=" << index;
        }
    }
}
} // namespace
