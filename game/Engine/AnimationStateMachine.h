#pragma once
#include "Component.h"
#include "AnimationState.h"

// 애니메이션 상태 머신
class AnimationStateMachine : public Component
{
    using Super = Component;

public:
    AnimationStateMachine(AnimationStateType initialState = AnimationStateType::Wait);
    ~AnimationStateMachine();

    //Component 메서드 오버라이드
    virtual void Start() override;
    virtual void Update() override;
    virtual void OnDestroy() override;

    // 상태 관리
    void RequestStateChange(AnimationStateType newState);
    bool CanChangeState(AnimationStateType newState);

    // 상태 조회
    AnimationStateType GetCurrentState() const;
    shared_ptr<AnimationState> GetCurrentStatePtr() const;
    shared_ptr<AnimationState> GetState(AnimationStateType type) const;
    bool IsInState(AnimationStateType state) const;

    // 상태 등록
    void RegisterState(AnimationStateType type, shared_ptr<AnimationState> state);

    // 애니메이션 완료 체크
    bool IsCurrentAnimationCompleted() const;
    float GetCurrentAnimationProgress() const;

    
    void PrintCurState();

private:
    // 상태 전환 실제 실행
    void ExecuteStateChange(AnimationStateType newState);

    // 애니메이션 완료 감지 및 자동 전환
    void CheckAnimationCompletion();
    void HandleAutoTransitions();

    void HandleStateChangeRequest(shared_ptr<EventData> eventData);

private:
    unordered_map<AnimationStateType, shared_ptr<AnimationState>> m_states;
    shared_ptr<AnimationState> m_currentState;
    shared_ptr<ModelAnimator> m_animator;
 
    AnimationStateType m_initialStateType;
    // Stop recovering an accepted action after its animation has been observed.
    AnimationStateType m_referenceAcceptedAnimation = AnimationStateType::Wait;
    bool m_referenceHasAcceptedAnimation = false;
    bool m_referenceAnimationPending = false;

    // 상태 전환 대기열
    queue<AnimationStateType> m_stateChangeQueue;

    // 자동 전환 설정
    unordered_map<AnimationStateType, AnimationStateType> m_autoTransitions;

    // 디버그
    bool m_enableDebugLog = false;
};