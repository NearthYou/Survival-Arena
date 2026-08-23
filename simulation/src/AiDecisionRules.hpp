#pragma once

#include <dxa/simulation/AiDecision.hpp>

namespace dxa::simulation::detail
{
void ValidateAiBlackboard(const AiBlackboard& blackboard);
[[nodiscard]] float TargetDistance(const AiBlackboard& blackboard);
} // namespace dxa::simulation::detail
