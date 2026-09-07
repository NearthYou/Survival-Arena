#pragma once
#include <algorithm>

namespace SkillProgression
{
inline int RankSteps(int rank, int maxRank)
{
    return std::clamp(rank, 1, (std::max)(1, maxRank)) - 1;
}

inline float Cooldown(float base, int rank, int maxRank, float equipmentReduction)
{
    const int percent = (std::max)(50, 100 - 10 * RankSteps(rank, maxRank));
    const float itemFactor = 1.f - std::clamp(equipmentReduction, 0.f, 0.8f);
    return (std::max)(0.1f, base * percent / 100.f * itemFactor);
}

inline int StaminaCost(int base, int rank, int maxRank)
{
    const int percent = (std::max)(50, 100 - 10 * RankSteps(rank, maxRank));
    return ((std::max)(0, base) * percent + 99) / 100;
}

inline float DamageMultiplier(int rank, int maxRank)
{
    return (100 + 10 * RankSteps(rank, maxRank)) / 100.f;
}
}
