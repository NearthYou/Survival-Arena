#pragma once

#include "pch.h"

enum class AnimationStateType;
enum class PlayerStateType;
enum class MonsterStateType;

// 이벤트 타입 정의
enum class EventType : uint32
{
    // 플레이어 관련
    PLAYER_SKILL_START,
    PLAYER_SKILL_END,
    PLAYER_ATTACK_START,
    PLAYER_ATTACK_END,
    PLAYER_DAMAGED,
    PLAYER_DEATH,

    // 몬스터 관련
    MONSTER_DAMAGED,
    MONSTER_DEATH,
    MONSTER_EXP_REWARD,
    MONSTER_SPAWN,
    MONSTER_AGGRO_START,
    MONSTER_AGGRO_END,

    // 애니메이션 관련
    ANIMATION_START,
    ANIMATION_END,
    ANIMATION_TRANSITION,

    // 상태 관련
    STATE_ENTER,
    STATE_EXIT,
    STATE_UPDATE,

    // 상태 변경 이벤트 추가
    ANIMATION_STATE_CHANGE_REQUEST,
    PLAYER_STATE_CHANGE_REQUEST,
    MONSTER_STATE_CHANGE_REQUEST,

    // 상태 변경 완료 알림
    ANIMATION_STATE_CHANGED,
    PLAYER_STATE_CHANGED,
    MONSTER_STATE_CHANGED,

    //end
    END
};

// 이벤트 데이터 기본 클래스
class EventData
{
public:
    EventData(EventType type) : m_eventType(type), m_timestamp(TIME->GetGameTime()) {}
    virtual ~EventData() = default;

    EventType GetType() const { return m_eventType; }
    float GetTimestamp() const { return m_timestamp; }

private:
    EventType m_eventType;
    float m_timestamp;
};

// 구체적인 이벤트 데이터들
class SkillEventData : public EventData
{
public:
    SkillEventData(EventType type, int skillIndex, shared_ptr<GameObject> caster, shared_ptr<GameObject> target = nullptr)
        : EventData(type), m_skillIndex(skillIndex), m_caster(caster), m_target(target)
    {

    }

    int m_skillIndex;
    shared_ptr<GameObject> m_caster;
    shared_ptr<GameObject> m_target;
};

class DamageEventData : public EventData
{
public:
    DamageEventData(EventType type, shared_ptr<GameObject> attacker, shared_ptr<GameObject> victim, int damage)
        : EventData(type), m_attacker(attacker), m_victim(victim), m_damage(damage)
    {

    }

    shared_ptr<GameObject> m_attacker;
    shared_ptr<GameObject> m_victim;
    int m_damage;
};

class StateEventData : public EventData
{
public:
    StateEventData(EventType type, shared_ptr<GameObject> owner, int fromState, int toState)
        : EventData(type), m_owner(owner), m_fromState(fromState), m_toState(toState)
    {

    }

    shared_ptr<GameObject> m_owner;
    int m_fromState;
    int m_toState;
};

// 애니메이션 상태 변경 이벤트
class AnimationStateChangeEventData : public EventData
{
public:
    AnimationStateChangeEventData(EventType type, shared_ptr<GameObject> target, AnimationStateType newState)
        : EventData(type), m_target(target), m_newState(newState)
    {
    }

    shared_ptr<GameObject> m_target;
    AnimationStateType m_newState;
};

// 플레이어 상태 변경 이벤트
class PlayerStateChangeEventData : public EventData
{
public:
    PlayerStateChangeEventData(EventType type, shared_ptr<GameObject> target, PlayerStateType newState)
        : EventData(type), m_target(target), m_newState(newState)
    {
    }

    shared_ptr<GameObject> m_target;
    PlayerStateType m_newState;
};

// 몬스터 상태 변경 이벤트
class MonsterStateChangeEventData : public EventData
{
public:
    MonsterStateChangeEventData(EventType type, shared_ptr<GameObject> target, MonsterStateType newState)
        : EventData(type), m_target(target), m_newState(newState)
    {
    }

    shared_ptr<GameObject> m_target;
    MonsterStateType m_newState;
};

// 경험치 보상 이벤트 데이터
class ExpRewardEventData : public EventData
{
public:
    ExpRewardEventData(shared_ptr<GameObject> killer, int expAmount, shared_ptr<GameObject> killedMonster = nullptr)
        : EventData(EventType::MONSTER_EXP_REWARD), m_killer(killer), m_expAmount(expAmount), m_killedMonster(killedMonster)
    {
    }

    shared_ptr<GameObject> m_killer;
    int m_expAmount;
    shared_ptr<GameObject> m_killedMonster;
};