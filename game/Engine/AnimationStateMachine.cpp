// AnimationStateMachine.cpp
#include "pch.h"
#include "AnimationStateMachine.h"
#include "GameObject.h"
#include "ModelAnimator.h"
#include "NavMesh.h"
#include "NavMeshAgent.h"
#include "Camera.h"
#include "Viewport.h"

#include "PlayerStateMachine.h"

static bool ReferenceAcceptedPlayerAnimation(const shared_ptr<GameObject>& object, AnimationStateType& animation)
{
    if (!object) return false;
    const auto player = object->GetPlayerStateMachine();
    if (!player) return false;
    switch (player->GetCurrentState())
    {
    case PlayerStateType::Wait: animation = AnimationStateType::Wait; break;
    case PlayerStateType::Run: animation = AnimationStateType::Run; break;
    case PlayerStateType::Skill_1: animation = AnimationStateType::Skill_1; break;
    case PlayerStateType::Skill_2: animation = AnimationStateType::Skill_2; break;
    case PlayerStateType::Skill_3: animation = AnimationStateType::Skill_3; break;
    case PlayerStateType::Skill_4: animation = AnimationStateType::Skill_4; break;
    case PlayerStateType::Craft: animation = AnimationStateType::Craft; break;
    case PlayerStateType::BaseAttack:
    {
        // Combat intent still includes movement to the target.
        const auto navigation = object->GetNavMeshAgent();
        animation = navigation && navigation->IsMoving() ? AnimationStateType::Run : AnimationStateType::BaseAttack;
        break;
    }
    case PlayerStateType::Counter: animation = AnimationStateType::Counter; break;
    default: return false;
    }
    return true;
}


// ... 다른 상태들 include

AnimationStateMachine::AnimationStateMachine(AnimationStateType initialState)
    : Super(ComponentType::AnimationStateMachine)
    , m_initialStateType(initialState)
{
    // 이벤트 구독
    EVENT->Subscribe(EventType::ANIMATION_STATE_CHANGE_REQUEST,
        [this](shared_ptr<EventData> eventData) {
            HandleStateChangeRequest(eventData);
        });
}

AnimationStateMachine::~AnimationStateMachine()
{
    EVENT->UnsubscribeAll(EventType::ANIMATION_STATE_CHANGE_REQUEST);
}

void AnimationStateMachine::Start()
{
    Super::Start();

    auto gameObject = GetGameObject();
    if (gameObject)
    {
        m_animator = gameObject->GetModelAnimator();
    }

    // 초기 상태 설정
    if (m_states.find(m_initialStateType) != m_states.end())
    {
        m_currentState = m_states[m_initialStateType];
        if (m_currentState && m_animator)
        {
            m_currentState->Enter(m_animator);
        }
    }
}

void AnimationStateMachine::Update()
{
    Super::Update();
    PrintCurState();
  
    // 대기열의 상태 변경 요청 처리
    while (!m_stateChangeQueue.empty())
    {
        AnimationStateType newState = m_stateChangeQueue.front();
        m_stateChangeQueue.pop();
        ExecuteStateChange(newState);
    }

    // 현재 상태 업데이트
    if (m_currentState && m_animator)
    {
        m_currentState->Update(m_animator);
    }

    // 애니메이션 완료 체크 및 자동 전환
    CheckAnimationCompletion();
    HandleAutoTransitions();
}

void AnimationStateMachine::OnDestroy()
{
    if (m_currentState && m_animator)
    {
        m_currentState->Exit(m_animator);
    }
    m_currentState = nullptr;
    m_states.clear();

    // 이벤트 구독 해제
    EVENT->UnsubscribeAll(EventType::ANIMATION_STATE_CHANGE_REQUEST);

    Super::OnDestroy();
}



void AnimationStateMachine::RequestStateChange(AnimationStateType newState)
{
    // EventManager를 통해 상태 변경 요청
    auto eventData = make_shared<AnimationStateChangeEventData>(
        EventType::ANIMATION_STATE_CHANGE_REQUEST,
        GetGameObject(),
        newState
    );
    EVENT->QueueEvent(eventData);
}

bool AnimationStateMachine::CanChangeState(AnimationStateType newState)
{
    // A real accepted action can cancel the previous visual tail. Idle alone
    // does not truncate it, and duplicate requests do not restart a clip.
    AnimationStateType accepted;
    if (ReferenceAcceptedPlayerAnimation(GetGameObject(), accepted) && accepted == newState &&
        newState != AnimationStateType::Wait && newState != GetCurrentState() &&
        m_states.find(newState) != m_states.end())
        return true;

    if (!m_currentState)
        return true;

    return m_currentState->CanTransitionTo(newState);
}



AnimationStateType AnimationStateMachine::GetCurrentState() const
{
    return m_currentState ? m_currentState->GetType() : m_initialStateType;
}

shared_ptr<AnimationState> AnimationStateMachine::GetCurrentStatePtr() const
{
    return m_currentState;
}

shared_ptr<AnimationState> AnimationStateMachine::GetState(AnimationStateType type) const
{
    auto it = m_states.find(type);
    if (it != m_states.end()) {
        return it->second;  // 찾은 경우 값 반환
    }
    return nullptr;  // 찾지 못한 경우 nullptr 반환
}

bool AnimationStateMachine::IsInState(AnimationStateType state) const
{
    return GetCurrentState() == state;
}




void AnimationStateMachine::RegisterState(AnimationStateType type, shared_ptr<AnimationState> state)
{
    m_states[type] = state;
}



bool AnimationStateMachine::IsCurrentAnimationCompleted() const
{
    if (!m_animator)
        return false;

    return m_animator->IsAnimationFinished() && !m_animator->IsAnimationTransitioning();
}

float AnimationStateMachine::GetCurrentAnimationProgress() const
{
    if (!m_animator)
        return 0.0f;

    // ModelAnimator에서 진행률 계산 로직 필요
    // 임시로 0.0f 반환
    return 0.0f;
}



void AnimationStateMachine::PrintCurState()
{
    if (GetGameObject()->GetName().compare(L"Nicky") != 0)
        return;

    if (INPUT->GetButtonDown(KEY_TYPE::A))
    {
        switch (m_currentState->GetType())
        {
        case AnimationStateType::Skill_1:
            cout << "AnimationCurState : Q 스킬 상태\n";
            break;
        case AnimationStateType::Skill_2:
            cout << "AnimationCurState : W 스킬 상태\n";
            break;
        case AnimationStateType::Skill_3:
            cout << "AnimationCurState : E 스킬 상태\n";
            break;
        case AnimationStateType::Skill_4:
            cout << "AnimationCurState : R 스킬 상태\n";
            break;
        case AnimationStateType::Run:
            cout << "AnimationCurState : Run 상태\n";
            break;
        case AnimationStateType::Wait:
            cout << "AnimationCurState : Wait 상태\n";
            break;
        case AnimationStateType::Trace:
            cout << "AnimationCurState : Trace 상태\n";
            break;
        case AnimationStateType::Counter:
            cout << "AnimationCurState : Counter 상태\n";
            break;
        default:
            cout << "Type : " << (uint32)m_currentState->GetType() << endl;
            break;
        } 
    }
}



void AnimationStateMachine::ExecuteStateChange(AnimationStateType newState)
{
    if (!CanChangeState(newState))
        return;

    AnimationStateType oldState = GetCurrentState();

    // 현재 상태 종료
    if (m_currentState && m_animator)
    {
        m_currentState->Exit(m_animator);
    }

    // 새 상태 시작
    m_currentState = m_states[newState];
    if (m_currentState && m_animator)
    {
        m_currentState->Enter(m_animator);
    }

    // 상태 변경 완료 이벤트 발생
    auto completedEventData = make_shared<StateEventData>(
        EventType::ANIMATION_STATE_CHANGED,
        GetGameObject(),
        static_cast<int>(oldState),
        static_cast<int>(newState)
    );
    EVENT->TriggerEvent(completedEventData);

    if (m_enableDebugLog)
    {
        cout << "AnimationState changed: " << static_cast<int>(oldState)
            << " -> " << static_cast<int>(newState) << endl;
    }
}

void AnimationStateMachine::CheckAnimationCompletion()
{
    if (!m_animator || !m_currentState)
        return;

    // 현재 애니메이션이 완료되었는지 확인
    if (IsCurrentAnimationCompleted())
    {
        // 애니메이션 완료 이벤트 발생
        auto eventData = make_shared<EventData>(EventType::ANIMATION_END);
        EVENT->TriggerEvent(eventData);
    }
}

void AnimationStateMachine::HandleAutoTransitions()
{
    // Recover requests that arrived before the gameplay transition was applied.
    // Non-player machines retain their original transition behavior.
    AnimationStateType accepted;
    if (ReferenceAcceptedPlayerAnimation(GetGameObject(), accepted) && m_states.find(accepted) != m_states.end())
    {
        if (!m_referenceHasAcceptedAnimation || m_referenceAcceptedAnimation != accepted)
        {
            m_referenceAcceptedAnimation = accepted;
            m_referenceHasAcceptedAnimation = true;
            m_referenceAnimationPending = true;
        }
        // Completion events may lag the visual tail. Do not replay an action
        // whose accepted animation already ran while that event is pending.
        if (accepted == GetCurrentState())
            m_referenceAnimationPending = false;
        else if (m_referenceAnimationPending && CanChangeState(accepted))
            RequestStateChange(accepted);
        return;
    }
    m_referenceHasAcceptedAnimation = false;
    m_referenceAnimationPending = false;

    if (!m_currentState)
        return;

    AnimationStateType currentType = GetCurrentState();
    auto it = m_autoTransitions.find(currentType);

    if (it != m_autoTransitions.end() && IsCurrentAnimationCompleted())
    {
        RequestStateChange(it->second);
    }
}

void AnimationStateMachine::HandleStateChangeRequest(shared_ptr<EventData> eventData)
{
    auto stateChangeData = static_pointer_cast<AnimationStateChangeEventData>(eventData);

    // 자신의 GameObject인지 확인
    if (stateChangeData->m_target != GetGameObject())
        return;

    //ChangeStateImmediate(stateChangeData->m_newState);
    // 
    // 상태 변경 대기열에 추가
    m_stateChangeQueue.push(stateChangeData->m_newState);
}


//
//void AnimationStateMachine::ChangeStateImmediate(AnimationStateType newState)
//{
//    if (!CanChangeState(newState))
//        return;
//
//    AnimationStateType oldState = GetCurrentState();
//
//    // 현재 상태 종료
//    if (m_currentState)
//    {
//        m_currentState->Exit(m_animator);
//    }
//
//    // 새 상태 시작
//    m_currentState = m_states[newState];
//    if (m_currentState)
//    {
//        m_currentState->Enter(m_animator);
//    }
//
//    // 상태 변경 완료 이벤트 발생
//    auto completedEventData = make_shared<StateEventData>(
//        EventType::ANIMATION_STATE_CHANGED,
//        GetGameObject(),
//        static_cast<int>(oldState),
//        static_cast<int>(newState)
//    );
//    EVENT->TriggerEvent(completedEventData);
//}