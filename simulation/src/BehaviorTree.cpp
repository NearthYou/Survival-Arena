#include <dxa/simulation/BehaviorTree.hpp>

#include "AiDecisionRules.hpp"

#include <stdexcept>
#include <utility>

namespace dxa::simulation
{
namespace
{
[[nodiscard]] std::unique_ptr<BehaviorNode> HasTargetCondition()
{
    return ConditionNode::Create([](const BehaviorContext& context) {
        return context.blackboard->hasTarget;
    });
}

[[nodiscard]] std::unique_ptr<BehaviorNode> AttackRangeCondition()
{
    return ConditionNode::Create([](const BehaviorContext& context) {
        return context.distanceToTarget <= context.blackboard->attackRange;
    });
}

[[nodiscard]] std::unique_ptr<BehaviorNode> CooldownCondition()
{
    return ConditionNode::Create([](const BehaviorContext& context) {
        return context.blackboard->cooldownReady;
    });
}

[[nodiscard]] std::unique_ptr<BehaviorNode> RetreatRangeCondition()
{
    return ConditionNode::Create([](const BehaviorContext& context) {
        return context.distanceToTarget < context.blackboard->retreatRange;
    });
}

[[nodiscard]] std::unique_ptr<BehaviorNode> CommandAction(
    const AiCommandType command)
{
    return ActionNode::Create([command](BehaviorContext& context) {
        context.command = command;
        return BehaviorStatus::Success;
    });
}

[[nodiscard]] std::unique_ptr<BehaviorNode> AttackSequence()
{
    auto sequence = std::make_unique<SequenceNode>();
    sequence->Add(HasTargetCondition());
    sequence->Add(AttackRangeCondition());
    sequence->Add(CooldownCondition());
    sequence->Add(CommandAction(AiCommandType::Attack));
    return sequence;
}

[[nodiscard]] std::unique_ptr<BehaviorNode> ChaseSequence()
{
    auto sequence = std::make_unique<SequenceNode>();
    sequence->Add(HasTargetCondition());
    sequence->Add(CommandAction(AiCommandType::MoveToTarget));
    return sequence;
}

[[nodiscard]] std::unique_ptr<BehaviorNode> RetreatSequence()
{
    auto sequence = std::make_unique<SequenceNode>();
    sequence->Add(HasTargetCondition());
    sequence->Add(RetreatRangeCondition());
    sequence->Add(CommandAction(AiCommandType::MoveAwayFromTarget));
    return sequence;
}
} // namespace

ActionNode::ActionNode(Function function)
    : function_{std::move(function)}
{
}

std::unique_ptr<BehaviorNode> ActionNode::Create(Function function)
{
    if (!function)
    {
        throw std::invalid_argument{"Behavior action requires a function"};
    }
    return std::unique_ptr<BehaviorNode>{new ActionNode{std::move(function)}};
}

BehaviorStatus ActionNode::Tick(BehaviorContext& context) const
{
    return function_(context);
}

ConditionNode::ConditionNode(Function function)
    : function_{std::move(function)}
{
}

std::unique_ptr<BehaviorNode> ConditionNode::Create(Function function)
{
    if (!function)
    {
        throw std::invalid_argument{"Behavior condition requires a function"};
    }
    return std::unique_ptr<BehaviorNode>{new ConditionNode{std::move(function)}};
}

BehaviorStatus ConditionNode::Tick(BehaviorContext& context) const
{
    return function_(context) ? BehaviorStatus::Success : BehaviorStatus::Failure;
}

void SequenceNode::Add(std::unique_ptr<BehaviorNode> child)
{
    if (child == nullptr)
    {
        throw std::invalid_argument{"Behavior sequence child cannot be null"};
    }
    children_.push_back(std::move(child));
}

BehaviorStatus SequenceNode::Tick(BehaviorContext& context) const
{
    for (const auto& child : children_)
    {
        const BehaviorStatus status = child->Tick(context);
        if (status != BehaviorStatus::Success)
        {
            return status;
        }
    }
    return BehaviorStatus::Success;
}

void SelectorNode::Add(std::unique_ptr<BehaviorNode> child)
{
    if (child == nullptr)
    {
        throw std::invalid_argument{"Behavior selector child cannot be null"};
    }
    children_.push_back(std::move(child));
}

BehaviorStatus SelectorNode::Tick(BehaviorContext& context) const
{
    for (const auto& child : children_)
    {
        const BehaviorStatus status = child->Tick(context);
        if (status != BehaviorStatus::Failure)
        {
            return status;
        }
    }
    return BehaviorStatus::Failure;
}

BehaviorTreeAiController::BehaviorTreeAiController(const AiArchetype archetype)
{
    auto root = std::make_unique<SelectorNode>();
    if (archetype == AiArchetype::Ranged)
    {
        root->Add(RetreatSequence());
    }
    root->Add(AttackSequence());
    root->Add(ChaseSequence());
    root->Add(CommandAction(AiCommandType::Idle));
    root_ = std::move(root);
}

BehaviorTreeAiController::~BehaviorTreeAiController() = default;

AiCommandType BehaviorTreeAiController::Tick(
    const AiBlackboard& blackboard) const
{
    detail::ValidateAiBlackboard(blackboard);
    BehaviorContext context;
    context.blackboard = &blackboard;
    if (blackboard.hasTarget)
    {
        context.distanceToTarget = detail::TargetDistance(blackboard);
    }

    if (root_->Tick(context) != BehaviorStatus::Success)
    {
        throw std::logic_error{"AI behavior tree did not select a command"};
    }
    return context.command;
}
} // namespace dxa::simulation
