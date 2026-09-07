#include "pch.h"
#include "SkillConfig.h"

unordered_map<wstring, vector<SkillMetaData>> SkillConfig::s_skillConfigs;
unordered_map<uint32, wstring> s_indexToCharacterName;

void SkillConfig::InitializeConfigs()
{
    // Nicky 스킬 설정
    s_skillConfigs[L"Nicky"] = {
        // Q 스킬 - 차징, 타겟 불필요
        {SkillTargetType::None, SkillCastType::Channeling, 15.0f, false},
        // W 스킬 - 즉시, 타겟 불필요  
        {SkillTargetType::None, SkillCastType::Instant, 0.0f, false},
        // E 스킬 - 즉시, 타겟 불필요
        {SkillTargetType::None, SkillCastType::Instant, 10.0f, false},
        // R 스킬 - 즉시, 단일 타겟 필요
        {SkillTargetType::Single, SkillCastType::Instant, 20.0f, false}
    };

    // Bianca 스킬 설정
    s_skillConfigs[L"Bianca"] = {
        // Q 스킬 - 즉시, 지역 타겟
        {SkillTargetType::None, SkillCastType::Instant, 10.0f, false},
        // W 스킬 - 즉시, 타겟 불필요
        {SkillTargetType::None, SkillCastType::Instant, 0.0f, false},
        // E 스킬 - 즉시, 타겟 불필요  
        {SkillTargetType::None, SkillCastType::Channeling, 8.0f, false},
        // R 스킬 - 즉시, 타겟 불필요
        {SkillTargetType::None, SkillCastType::Instant, 15.0f, true}
    };

    s_indexToCharacterName[0] = L"Bianca";
    s_indexToCharacterName[1] = L"Nicky";
}

const SkillMetaData& SkillConfig::GetSkillMetaData(uint32 characterIndex, int skillIndex)
{
    static SkillMetaData defaultMeta;

    auto charIt = s_skillConfigs.find(s_indexToCharacterName[characterIndex]);

    if (charIt != s_skillConfigs.end() &&
        skillIndex >= 0 && skillIndex < charIt->second.size())
    {
        return charIt->second[skillIndex];
    }

    return defaultMeta;
}