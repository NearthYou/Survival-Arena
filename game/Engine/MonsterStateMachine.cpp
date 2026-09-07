// MonsterStateMachine.cpp
#include "pch.h"
#include "MonsterStateMachine.h"
#include "GameObject.h"
#include "AnimationStateMachine.h"
#include "NavMeshAgent.h"
#include "IMonster.h"

MonsterStateMachine::MonsterStateMachine()
    : Super(ComponentType::MonsterStateMachine)
{
    // 몬스터 상태 변경 요청 이벤트 구독
    EVENT->Subscribe(EventType::MONSTER_STATE_CHANGE_REQUEST,
        [this](shared_ptr<EventData> eventData) {
            HandleStateChangeRequest(eventData);
        });

    // 애니메이션 상태 변경 완료 이벤트 구독 (연동)
    EVENT->Subscribe(EventType::ANIMATION_STATE_CHANGED,
        [this](shared_ptr<EventData> eventData) {
            HandleAnimationStateChanged(eventData);
        });
}

MonsterStateMachine::~MonsterStateMachine()
{
    EVENT->UnsubscribeAll(EventType::MONSTER_STATE_CHANGE_REQUEST);
    EVENT->UnsubscribeAll(EventType::ANIMATION_STATE_CHANGED);
}

void MonsterStateMachine::Start()
{
    Super::Start();

    // 컴포넌트 참조 가져오기
    auto gameObject = GetGameObject();
    if (gameObject)
    {
        m_animationStateMachine = gameObject->GetAnimationStateMachine();
        m_navMeshAgent = gameObject->GetNavMeshAgent();
    }

    // 초기 상태 설정 (Appear 또는 Wait)
    if (m_states.find(MonsterStateType::Appear) != m_states.end())
    {
        m_currentState = m_states[MonsterStateType::Appear];
        if (m_currentState)
        {
            m_currentState->Enter();
        }
    }
}

void MonsterStateMachine::Update()
{
    Super::Update();

    // AI 로직 처리
    ProcessAI();

    // 상태 완료 체크들
    CheckAppearCompletion();
    CheckDeathCompletion();

    // 대기열의 상태 변경 요청 처리
    while (!m_stateChangeQueue.empty())
    {
        MonsterStateType newState = m_stateChangeQueue.front();
        m_stateChangeQueue.pop();
        ExecuteStateChange(newState);
    }

    // 현재 상태 업데이트
    if (m_currentState)
    {
        m_currentState->Update();
    }
}

void MonsterStateMachine::OnDestroy()
{
    if (m_currentState)
    {
        m_currentState->Exit();
    }
    m_currentState = nullptr;
    m_states.clear();

    // 이벤트 구독 해제
    EVENT->UnsubscribeAll(EventType::MONSTER_STATE_CHANGE_REQUEST);
    EVENT->UnsubscribeAll(EventType::ANIMATION_STATE_CHANGED);

    Super::OnDestroy();
}

void MonsterStateMachine::RequestStateChange(MonsterStateType newState)
{
    // EventManager를 통해 상태 변경 요청
    auto eventData = make_shared<MonsterStateChangeEventData>(
        EventType::MONSTER_STATE_CHANGE_REQUEST,
        GetGameObject(),
        newState
    );
    EVENT->QueueEvent(eventData);
}

void MonsterStateMachine::ExecuteStateChange(MonsterStateType newState)
{
    if (!CanChangeState(newState))
        return;

    MonsterStateType oldState = GetCurrentState();

    // 현재 상태 종료
    if (m_currentState)
    {
        m_currentState->Exit();
    }

    // 새 상태 시작
    m_currentState = m_states[newState];
    if (m_currentState)
    {
        // 상태 진입 후 안전하게 타겟 설정
        if (m_target && (newState == MonsterStateType::Trace || newState == MonsterStateType::Attack))
        {
            m_currentState->SetTarget(m_target);
        }

        m_currentState->Enter();

    }

    // 상태 변경 완료 이벤트 발생
    auto completedEventData = make_shared<StateEventData>(
        EventType::MONSTER_STATE_CHANGED,
        GetGameObject(),
        static_cast<int>(oldState),
        static_cast<int>(newState)
    );
    EVENT->TriggerEvent(completedEventData);

    if (m_enableDebugLog)
    {
        cout << "MonsterState changed: " << static_cast<int>(oldState)
            << " -> " << static_cast<int>(newState) << endl;
    }
}

void MonsterStateMachine::HandleDeath()
{
    if (m_monsterInterface->GetHP() <= 0)
    {
        RequestStateChange(MonsterStateType::Death);
    }
}

void MonsterStateMachine::ProcessAI()
{
    // 1. 체력 체크 (최우선)
    if (m_monsterInterface && m_monsterInterface->GetHP() <= 0)
    {
        // 아직 Death 상태가 아니라면 Death로 전환
        if (!IsInState(MonsterStateType::Death) && !IsInState(MonsterStateType::Dying))
        {
            cout << "몬스터 사망 감지 - Death 상태로 전환" << endl;
            RequestStateChange(MonsterStateType::Death);

            if (m_animationStateMachine)
                m_animationStateMachine->RequestStateChange(AnimationStateType::Death);

            return; // 사망 처리 후 다른 AI 로직은 실행하지 않음
        }
    }

    // 2. 죽지 않은 경우에만 일반 AI 로직 실행
    if (IsInState(MonsterStateType::Death) || IsInState(MonsterStateType::Dying))
        return;

    // 3. 타겟 유효성 검사 및 거리 계산
    float distanceToTarget = 0.0f;
    bool hasValidTarget = false;

    if (m_target && m_target->GetActive() && m_target->GetType() == OBJECTTYPE::PLAYER)
    {
        distanceToTarget = Vec3::Distance(
            GetGameObject()->GetTransform()->GetPosition(),
            m_target->GetTransform()->GetPosition()
        );
        hasValidTarget = true;
    }

    // 4. 새로운 상태 전환 로직 (피격 기반)
    MonsterStateType currentState = GetCurrentState();

    switch (currentState)
    {
    case MonsterStateType::Appear:
        // Appear 상태는 애니메이션 완료로 처리
        break;

    case MonsterStateType::Wait:
        // Wait 상태에서는 피격 시에만 상태 전환 (Damaged에서 처리)
        // 별도 탐지 로직 없음
        break;

    case MonsterStateType::Trace:
        if (!hasValidTarget)
        {
            cout << "추적 중 타겟 소실 - Wait 상태로 전환" << endl;
            SetTarget(nullptr);
            RequestStateChange(MonsterStateType::Wait);
            if (m_animationStateMachine)
                m_animationStateMachine->RequestStateChange(AnimationStateType::Wait);
        }
        else if (distanceToTarget <= m_attackRange)
        {
            // **이미 Attack 상태가 아닐 때만 전환 요청**
            if (!IsInState(MonsterStateType::Attack))
            {
                cout << "공격 사거리 진입 - Attack 상태로 전환 (거리: " << distanceToTarget << ")" << endl;
                RequestStateChange(MonsterStateType::Attack);
                if (m_animationStateMachine)
                    m_animationStateMachine->RequestStateChange(AnimationStateType::BaseAttack);
            }
        }
        break;

    case MonsterStateType::Attack:
        if (!hasValidTarget)
        {
            cout << "공격 중 타겟 소실 - Wait 상태로 전환" << endl;
            SetTarget(nullptr);
            RequestStateChange(MonsterStateType::Wait);
            if (m_animationStateMachine)
                m_animationStateMachine->RequestStateChange(AnimationStateType::Wait);
        }
        else if (distanceToTarget > m_attackRange )
        {
            // **이미 Trace 상태가 아닐 때만 전환 요청**
            if (!IsInState(MonsterStateType::Trace))
            {
                RequestStateChange(MonsterStateType::Trace);
                if (m_animationStateMachine)
                    m_animationStateMachine->RequestStateChange(AnimationStateType::Trace);
            }
        }
        
        // 공격 범위 내에 있으면 Attack 상태 유지 (연속 공격)
        break;
    }
}

void MonsterStateMachine::UpdateTargetDetection()
{
    // 이미 타겟이 있고 유효하다면 유지
    if (m_target && m_target->GetActive() && m_target->GetType() == OBJECTTYPE::PLAYER)
    {
        return; // 기존 타겟 유지
    }

    // 새로운 타겟 탐지
    auto gameObjects = CURSCENE->GetObjects();
    for (auto& obj : gameObjects)
    {
        if (obj->GetType() == OBJECTTYPE::PLAYER && obj->GetActive())
        {
            float distance = Vec3::Distance(
                GetGameObject()->GetTransform()->GetPosition(),
                obj->GetTransform()->GetPosition()
            );

            if (distance <= m_detectionRange)
            {
                SetTarget(obj);
                cout << "새로운 타겟 감지: " << obj->GetName().c_str() << endl;
                return;
            }
        }
    }

    // 유효한 타겟이 없으면 해제
    if (m_target)
    {
        SetTarget(nullptr);
        cout << "타겟 해제됨" << endl;
    }
}

void MonsterStateMachine::HandleStateChangeRequest(shared_ptr<EventData> eventData)
{
    auto stateChangeData = static_pointer_cast<MonsterStateChangeEventData>(eventData);

    // 자신의 GameObject인지 확인
    if (stateChangeData->m_target != GetGameObject())
        return;

    // 상태 변경 대기열에 추가
    m_stateChangeQueue.push(stateChangeData->m_newState);
}

void MonsterStateMachine::HandleAnimationStateChanged(shared_ptr<EventData> eventData)
{
    auto stateData = static_pointer_cast<StateEventData>(eventData);

    // 자신의 GameObject인지 확인
    if (stateData->m_owner != GetGameObject())
        return;

    // 애니메이션 상태 변경에 따른 몬스터 상태 자동 전환 로직
    AnimationStateType animState = static_cast<AnimationStateType>(stateData->m_toState);

    switch (animState)
    {
    case AnimationStateType::Wait:
        if (IsInState(MonsterStateType::Attack))
        {
            // 공격 완료 후 추적 또는 대기 상태로 복귀
            if (m_target)
            {
                RequestStateChange(MonsterStateType::Trace);
            }
            else
            {
                RequestStateChange(MonsterStateType::Wait);
            }
        }
        break;
    }
}

bool MonsterStateMachine::CanChangeState(MonsterStateType newState) const
{
    if (!m_currentState)
        return true;

    return m_currentState->CanTransitionTo(newState);
}

MonsterStateType MonsterStateMachine::GetCurrentState() const
{
    return m_currentState ? m_currentState->GetType() : MonsterStateType::Wait;
}

shared_ptr<MonsterState> MonsterStateMachine::GetCurrentStatePtr() const
{
    return m_currentState;
}

shared_ptr<MonsterState> MonsterStateMachine::GetState(MonsterStateType type) const
{
    auto it = m_states.find(type);
    if (it != m_states.end()) {
        return it->second;  // 찾은 경우 값 반환
    }
    return nullptr;  // 찾지 못한 경우 nullptr 반환
}


bool MonsterStateMachine::IsInState(MonsterStateType state) const
{
    return GetCurrentState() == state;
}

void MonsterStateMachine::RegisterState(MonsterStateType type, shared_ptr<MonsterState> state)
{
    m_states[type] = state;
}

void MonsterStateMachine::SetTarget(shared_ptr<GameObject> target)
{
    m_target = target;
}

shared_ptr<GameObject> MonsterStateMachine::GetTarget() const
{
    return m_target;
}

void MonsterStateMachine::SetMonsterInterface(shared_ptr<IMonster> monsterInterface)
{
    m_monsterInterface = monsterInterface;
}

// 새로운 완료 체크 함수들 추가
void MonsterStateMachine::CheckAppearCompletion()
{
    if (IsInState(MonsterStateType::Appear))
    {
        if (m_appearCompletionChecked)
            return;

        // Appear 상태에서 Wait로 전환 가능한지 체크
        if (m_currentState->CanTransitionTo(MonsterStateType::Wait))
        {
            cout << "Appear 완료 감지 - Wait 상태로 전환" << endl;

            m_appearCompletionChecked = true;

            RequestStateChange(MonsterStateType::Wait);

            if (m_animationStateMachine)
            {
                m_animationStateMachine->RequestStateChange(AnimationStateType::Wait);
            }
        }
    }
    else
    {
        m_appearCompletionChecked = false;
    }
}

void MonsterStateMachine::CheckDeathCompletion()
{
    if (IsInState(MonsterStateType::Death))
    {
        if (m_deathCompletionChecked)
            return;

        // Death 상태에서 Dying로 전환 가능한지 체크
        if (m_currentState->CanTransitionTo(MonsterStateType::Dying))
        {
            cout << "Death 완료 감지 - Dying 상태로 전환" << endl;

            m_deathCompletionChecked = true;

            RequestStateChange(MonsterStateType::Dying);

            if (m_animationStateMachine)
            {
                m_animationStateMachine->RequestStateChange(AnimationStateType::Dying);
            }
        }
    }
    else
    {
        m_deathCompletionChecked = false;
    }
}