#pragma once

// 애니메이션 상태 정의
enum class AnimationStateType
{
    Wait = 0,
    Run,

    BaseAttack,
    Skill_1,
    Skill_2,
    Skill_3,
    Skill_4,
    Charging,
    Releasing,
    Cooldown,
    Death,
    Dying, //죽은 상태 무한 반복. 
    Appear,
    Craft,
    Trace,
    Counter,
};

class ModelAnimator;

// 애니메이션 상태 기본 클래스
class AnimationState
{
public:
    AnimationState(AnimationStateType type) : m_type(type) {}
    virtual ~AnimationState() = default;

    virtual void Enter(shared_ptr<ModelAnimator> animator) = 0;
    virtual void Update(shared_ptr<ModelAnimator> animator) = 0;
    virtual void Exit(shared_ptr<ModelAnimator> animator) = 0;
    virtual bool CanTransitionTo(AnimationStateType nextState) = 0;

    AnimationStateType GetType() const { return m_type; }

    void SetExpectedDuration(float duration) { m_expectedDuration = duration; }

protected:
    AnimationStateType m_type;
    float m_expectedDuration;
};