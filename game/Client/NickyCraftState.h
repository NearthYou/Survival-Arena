#pragma once

#include "PlayerStateMachine.h"
#include "Item.h"
#include "Recipe.h"

class NickyCraftState
	: public PlayerState
{
    using Super = PlayerState;

public:
    NickyCraftState(shared_ptr<ModelAnimator> modelAnimator);
    ~NickyCraftState();

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(PlayerStateType newState);

    void SetCraftingTime(float craftingTime) { m_craftingTime = craftingTime; }

    void UpdateNormalSkill();

    void SetCraftTimeByGrade(ITEMGRADE grade);
    bool IsSkillComplete() const { return m_isSkillComplete; } // 새로 추가
    bool IsMovable() const override { return false; }
private:
    float m_skillTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isAnimationStarted = false;
    bool m_isSkillComplete = false;  // 추가: 스킬 완료 플래그
    
    float m_craftingTime = 1.f;

    shared_ptr<ModelAnimator> m_modelAnimator;

    ITEMGRADE m_craftingItemGrade = ITEMGRADE::COMMON;

    friend class PlayerStateMachine;
};

