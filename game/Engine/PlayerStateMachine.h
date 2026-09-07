#pragma once
#include "Component.h"

#include "Delegate.h"

enum class PlayerStateType
{
    Wait,
    Run,
    Skill_1,
    Skill_2,
    Skill_3,
    Skill_4,
    Craft,
    Die,
    BaseAttack,
    Counter,
};
class IPlayer;

// 상태 인터페이스 (State Pattern 기본틀)
class PlayerState
{
public:
    PlayerState(PlayerStateType type) : m_type(type) {}
    virtual ~PlayerState() = default;

    virtual void Enter() = 0;             // 상태 시작 시 호출
    virtual void Update() = 0;   // 매 프레임 호출
    virtual void Exit() = 0;              // 상태 종료 시 호출
    virtual bool CanTransitionTo(PlayerStateType newState) = 0;

    // 새로 추가할 가상 함수들
    virtual bool IsCharging() const { return false; }      // 차징 중인지 확인
    virtual bool IsReleasing() const { return false; }     // 릴리즈 중인지 확인
    virtual bool IsMovable() const { return true; }        // 이동 가능한지 확인

    PlayerStateType GetType() const { return m_type; }

    void SetRecipeIndex(int index) { m_recipeIndex = index; }
    
    void SetTarget(shared_ptr<GameObject> _target) { m_target = _target; }
    shared_ptr<GameObject> GetTarget() { return m_target; }

protected:
    PlayerStateType m_type;
    shared_ptr<GameObject> m_target;
    int m_recipeIndex = 0;
};


class PlayerStateMachine :
    public Component
{
    using Super = Component;
public:
    PlayerStateMachine(uint32 _characterIdx);
    ~PlayerStateMachine();

    // Component 주요 함수 오버라이드
    virtual void Start() override;
    virtual void Update() override;
    virtual void OnDestroy() override;

    // 상태 관리
    void RequestStateChange(PlayerStateType newState);
    bool CanChangeState(PlayerStateType newState);

    // 상태 조회
    PlayerStateType GetCurrentState() const;
    shared_ptr<PlayerState> GetCurrentStatePtr() const;
    shared_ptr<PlayerState> GetState(PlayerStateType type) const;
    bool IsInState(PlayerStateType state) const;

    // 상태 등록
    void RegisterState(PlayerStateType type, shared_ptr<PlayerState> state);

    // 입력 처리 (외부에서 호출)
    void ProcessInput();
    Ray CreateRayFromMouse(POINT mousePos, shared_ptr<Camera> camera);

    // 스킬 관련
    void SetPlayerInterface(shared_ptr<class IPlayer> playerInterface);
    void SetAttackTarget(shared_ptr<GameObject> target);
    shared_ptr<GameObject> GetPickedTargetAtMouse();
    bool CheckTargetForSkill(KEY_TYPE skillKey);
    bool IsCurrentStateMovable() const;
    

    void PrintCurState();

public:
    // 델리게이트
    Delegate::Delegate<int, shared_ptr<GameObject>> OnSkillUsed;
    Delegate::Delegate<bool&> OnTryCraftFirst;// 기존

    Delegate::Delegate<bool&> OnTryCraft;  // 제작 시도 요청
    Delegate::Delegate<bool&> OnCraftCompleted;  // 제작 완료 체크

    // 새로 추가할 스킬 완료 체크 Delegate
    Delegate::Delegate<bool&> OnQSkillCompleted;  // Q스킬 완료 체크
    Delegate::Delegate<bool&> OnWSkillCompleted;  // W스킬 완료 체크
    Delegate::Delegate<bool&> OnESkillCompleted;  // E스킬 완료 체크
    Delegate::Delegate<bool&> OnRSkillCompleted;  // E스킬 완료 체크
    Delegate::Delegate<bool&> OnCounterSkillCompleted;

private:
    // 상태 전환 실제 실행
    void ExecuteStateChange(PlayerStateType newState);

    // 이벤트 핸들러
    void HandleStateChangeRequest(shared_ptr<EventData> eventData);
    void HandleAnimationStateChanged(shared_ptr<EventData> eventData);

    // 입력 처리 내부 로직
    void HandleMovementInput();
    void CheckMovementCompletion();

    void HandleSkillInput();
    bool QueueSkillCast(int skillIndex, shared_ptr<GameObject> target);
    void CheckQSkillCompletion();
    void CheckWSkillCompletion();
    void CheckESkillCompletion();
    void CheckRSkillCompletion();
    void CheckCounterSkillCompletion();

    void HandleCraftInput();
    void CheckCraftCompletion();

    void HandleAttackInput();
    bool IsValidAttackTarget(shared_ptr<GameObject> target);

    void HandleRightClickInput();

private:
    unordered_map<PlayerStateType, shared_ptr<PlayerState>> m_states;
    shared_ptr<PlayerState> m_currentState;

    // 컴포넌트 참조
    shared_ptr<class AnimationStateMachine> m_animationStateMachine;
    shared_ptr<class NavMeshAgent> m_navMeshAgent;
    shared_ptr<class IPlayer> m_playerInterface;

    // 상태 전환 대기열
    queue<PlayerStateType> m_stateChangeQueue;
    int m_pendingSkillIndex = -1;
    shared_ptr<GameObject> m_pendingSkillTarget;
    // 입력 처리
    bool m_inputEnabled = true;
    // 디버그
    bool m_enableDebugLog = false;

    uint32 m_characterIndex = 0;

private:
    shared_ptr<GameObject> m_attackTarget; // 공격 대상 저장
    bool m_isMovingToAttack = false; // 공격을 위해 이동 중인지
    float m_baseAttackDelay = (38.f / 25.f) / 2.f;
    float m_baseAttackDelayDuration = 0.f;


private:
    bool m_qSkillCompletionChecked = false;  // 추가
    bool m_wSkillCompletionChecked = false;  // 추가
    bool m_eSkillCompletionChecked = false;  // 추가
    bool m_rSkillCompletionChecked = false;  // 추가

    bool m_counterSkillCompletionChecked = false;

};

