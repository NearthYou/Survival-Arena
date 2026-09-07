#pragma once

// SkillConfig.h
enum class SkillTargetType
{
    None,           // 타겟 불필요 (비앙카 R)
    Single,         // 단일 타겟 (니키 R)
};

enum class SkillCastType
{
    Instant,        // 즉시 발동
    Channeling,     // 채널링 (니키 Q처럼)
};

struct SkillMetaData
{
    SkillTargetType targetType = SkillTargetType::None;
    SkillCastType castType = SkillCastType::Instant;
    float range = 0.0f;
    bool canCastWhileMoving = false;    //스킬 중 이동할 수 있는지

    // 향후 추가될 수 있는 속성들...
};

class SkillConfig
{
public:
    static const SkillMetaData& GetSkillMetaData(uint32 characterIndex, int skillIndex);
    static void InitializeConfigs();
private:
    static unordered_map<wstring, vector<SkillMetaData>> s_skillConfigs;
    

};