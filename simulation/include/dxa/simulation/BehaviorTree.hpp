#pragma once

#include <dxa/simulation/AiDecision.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace dxa::simulation
{
enum class BehaviorStatus
{
    Success,
    Failure,
    Running
};

struct BehaviorContext
{
    const AiBlackboard* blackboard = nullptr;
    float distanceToTarget = 0.0F;
    AiCommandType command = AiCommandType::Idle;
};

class BehaviorNode
{
public:
    virtual ~BehaviorNode() = default;

    [[nodiscard]] virtual BehaviorStatus Tick(
        BehaviorContext& context) const = 0;
};

class ActionNode final : public BehaviorNode
{
public:
    using Function = std::function<BehaviorStatus(BehaviorContext&)>;

    [[nodiscard]] static std::unique_ptr<BehaviorNode> Create(Function function);
    [[nodiscard]] BehaviorStatus Tick(BehaviorContext& context) const override;

private:
    explicit ActionNode(Function function);

    Function function_;
};

class ConditionNode final : public BehaviorNode
{
public:
    using Function = std::function<bool(const BehaviorContext&)>;

    [[nodiscard]] static std::unique_ptr<BehaviorNode> Create(Function function);
    [[nodiscard]] BehaviorStatus Tick(BehaviorContext& context) const override;

private:
    explicit ConditionNode(Function function);

    Function function_;
};

class SequenceNode final : public BehaviorNode
{
public:
    void Add(std::unique_ptr<BehaviorNode> child);
    [[nodiscard]] BehaviorStatus Tick(BehaviorContext& context) const override;

private:
    std::vector<std::unique_ptr<BehaviorNode>> children_;
};

class SelectorNode final : public BehaviorNode
{
public:
    void Add(std::unique_ptr<BehaviorNode> child);
    [[nodiscard]] BehaviorStatus Tick(BehaviorContext& context) const override;

private:
    std::vector<std::unique_ptr<BehaviorNode>> children_;
};
} // namespace dxa::simulation
