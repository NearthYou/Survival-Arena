#include <dxa/simulation/BehaviorTree.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
using dxa::simulation::ActionNode;
using dxa::simulation::BehaviorContext;
using dxa::simulation::BehaviorStatus;
using dxa::simulation::ConditionNode;
using dxa::simulation::SelectorNode;
using dxa::simulation::SequenceNode;

TEST(BehaviorTree, SelectorStopsAtFirstSuccessfulChild)
{
    std::vector<int> calls;
    SelectorNode selector;
    selector.Add(ActionNode::Create([&calls](BehaviorContext&) {
        calls.push_back(1);
        return BehaviorStatus::Failure;
    }));
    selector.Add(ActionNode::Create([&calls](BehaviorContext&) {
        calls.push_back(2);
        return BehaviorStatus::Success;
    }));
    selector.Add(ActionNode::Create([&calls](BehaviorContext&) {
        calls.push_back(3);
        return BehaviorStatus::Success;
    }));
    BehaviorContext context;

    EXPECT_EQ(BehaviorStatus::Success, selector.Tick(context));
    EXPECT_EQ((std::vector<int>{1, 2}), calls);
}

TEST(BehaviorTree, SequenceStopsAtFirstFailedChild)
{
    std::vector<int> calls;
    SequenceNode sequence;
    sequence.Add(ActionNode::Create([&calls](BehaviorContext&) {
        calls.push_back(1);
        return BehaviorStatus::Success;
    }));
    sequence.Add(ActionNode::Create([&calls](BehaviorContext&) {
        calls.push_back(2);
        return BehaviorStatus::Failure;
    }));
    sequence.Add(ActionNode::Create([&calls](BehaviorContext&) {
        calls.push_back(3);
        return BehaviorStatus::Success;
    }));
    BehaviorContext context;

    EXPECT_EQ(BehaviorStatus::Failure, sequence.Tick(context));
    EXPECT_EQ((std::vector<int>{1, 2}), calls);
}

TEST(BehaviorTree, CompositeEmptyAndExhaustedResultsAreStable)
{
    SequenceNode allSuccess;
    allSuccess.Add(ActionNode::Create([](BehaviorContext&) {
        return BehaviorStatus::Success;
    }));
    allSuccess.Add(ConditionNode::Create([](const BehaviorContext&) {
        return true;
    }));
    SelectorNode allFailure;
    allFailure.Add(ActionNode::Create([](BehaviorContext&) {
        return BehaviorStatus::Failure;
    }));
    allFailure.Add(ConditionNode::Create([](const BehaviorContext&) {
        return false;
    }));
    SequenceNode emptySequence;
    SelectorNode emptySelector;
    BehaviorContext context;

    EXPECT_EQ(BehaviorStatus::Success, allSuccess.Tick(context));
    EXPECT_EQ(BehaviorStatus::Failure, allFailure.Tick(context));
    EXPECT_EQ(BehaviorStatus::Success, emptySequence.Tick(context));
    EXPECT_EQ(BehaviorStatus::Failure, emptySelector.Tick(context));
}

TEST(BehaviorTree, CompositesPropagateRunningWithoutTickingLaterChildren)
{
    std::vector<int> sequenceCalls;
    SequenceNode sequence;
    sequence.Add(ActionNode::Create([&sequenceCalls](BehaviorContext&) {
        sequenceCalls.push_back(1);
        return BehaviorStatus::Running;
    }));
    sequence.Add(ActionNode::Create([&sequenceCalls](BehaviorContext&) {
        sequenceCalls.push_back(2);
        return BehaviorStatus::Success;
    }));

    std::vector<int> selectorCalls;
    SelectorNode selector;
    selector.Add(ActionNode::Create([&selectorCalls](BehaviorContext&) {
        selectorCalls.push_back(1);
        return BehaviorStatus::Running;
    }));
    selector.Add(ActionNode::Create([&selectorCalls](BehaviorContext&) {
        selectorCalls.push_back(2);
        return BehaviorStatus::Success;
    }));
    BehaviorContext context;

    EXPECT_EQ(BehaviorStatus::Running, sequence.Tick(context));
    EXPECT_EQ((std::vector<int>{1}), sequenceCalls);
    EXPECT_EQ(BehaviorStatus::Running, selector.Tick(context));
    EXPECT_EQ((std::vector<int>{1}), selectorCalls);
}

TEST(BehaviorTree, RejectsNullChildrenAndEmptyFunctions)
{
    SelectorNode selector;
    SequenceNode sequence;

    EXPECT_THROW(selector.Add(nullptr), std::invalid_argument);
    EXPECT_THROW(sequence.Add(nullptr), std::invalid_argument);
    EXPECT_THROW(
        (void)ActionNode::Create(ActionNode::Function{}),
        std::invalid_argument);
    EXPECT_THROW(
        (void)ConditionNode::Create(ConditionNode::Function{}),
        std::invalid_argument);
}
} // namespace
