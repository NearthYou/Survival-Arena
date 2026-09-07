#pragma once
#include "PlayerStateMachine.h"
#include "Item.h"
#include "Recipe.h"

class BiancaCraftState :
    public PlayerState
{
    using Super = PlayerState;

public:
    BiancaCraftState(shared_ptr<ModelAnimator> modelAnimator);
    ~BiancaCraftState();

    virtual void Enter();
    virtual void Update();
    virtual void Exit();
    virtual bool CanTransitionTo(PlayerStateType newState);
    bool IsMovable() const override { return false; }
    void UpdateNormalSkill();

    void SetCraftingTime(float craftingTime) { m_craftingTime = craftingTime; }

    void SetCraftTimeByGrade(ITEMGRADE grade);

    bool IsSkillComplete() const { return m_isSkillComplete; } // 새로 추가

private:
    float m_skillTime = 0.0f;  // 대기 상태 지속 시간
    bool m_isAnimationStarted = false;

    bool m_isSkillComplete = false;  // 추가: 스킬 완료 플래그

    float m_craftingTime = 1.f;
    
    shared_ptr<ModelAnimator> m_modelAnimator;
    ITEMGRADE m_craftingItemGrade = ITEMGRADE::COMMON;

    friend class PlayerStateMachine;
};

