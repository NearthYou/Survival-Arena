#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <utility>
using namespace std;
enum class KEY_TYPE { LCTRL, Q, W, E, R };
enum class PlayerStateType { Wait, Run, Skill_1, Skill_2, Skill_3, Skill_4 };
enum class AnimationStateType { Wait, Run, Skill_1, Skill_2, Skill_3, Skill_4 };
enum class SkillTargetType { None, Single };
struct Vec3 { float x=0; static float Distance(Vec3 a,Vec3 b) { return abs(a.x-b.x); } };
struct Transform { Vec3 position; Vec3 GetPosition() const { return position; } };
struct GameObject { bool active=true; Transform transform; bool GetActive() const { return active; } Transform* GetTransform() { return &transform; } };
struct Input { array<bool,5> down{}; bool GetButtonDown(KEY_TYPE key) { return down[static_cast<int>(key)]; } bool GetButton(KEY_TYPE) { return false; } } input;
#define INPUT (&input)
struct Meta { SkillTargetType targetType=SkillTargetType::None; float range=20; };
struct SkillConfig { static Meta GetSkillMetaData(unsigned,int i) { return {i==3?SkillTargetType::Single:SkillTargetType::None,20}; } };
struct PlayerInterface { int stamina=100; bool CanUseSkill(int) { return stamina>=30; } float GetCurSkillCooldown(int) { return 0; } };
struct PlayerState { PlayerStateType type; bool allow=true; explicit PlayerState(PlayerStateType value):type(value) {} void Enter() {} void Exit() {} bool CanTransitionTo(PlayerStateType) { return allow; } };
enum class EventType { PLAYER_STATE_CHANGED };
struct StateEventData { template<class... Args> StateEventData(Args...) {} };
struct EventManager { template<class T> void TriggerEvent(T) {} } events;
#define EVENT (&events)
struct Navigation { int stops=0; void Stop() { ++stops; } };
struct Animations { void RequestStateChange(AnimationStateType) {} };
struct PlayerStateMachine {
    map<PlayerStateType,shared_ptr<PlayerState>> m_states;
    shared_ptr<PlayerState> m_currentState;
    shared_ptr<PlayerInterface> m_playerInterface=make_shared<PlayerInterface>();
    shared_ptr<Navigation> m_navMeshAgent=make_shared<Navigation>();
    shared_ptr<Animations> m_animationStateMachine=make_shared<Animations>();
    shared_ptr<GameObject> object=make_shared<GameObject>(), picked=make_shared<GameObject>();
    int m_pendingSkillIndex=-1, casts=0;
    shared_ptr<GameObject> m_pendingSkillTarget;
    unsigned m_characterIndex=1;
    bool m_enableDebugLog=false;
    queue<PlayerStateType> pending;
    function<void(int,shared_ptr<GameObject>)> OnSkillUsed;
    PlayerStateMachine() {
        for(auto type:{PlayerStateType::Wait,PlayerStateType::Run,PlayerStateType::Skill_1,PlayerStateType::Skill_2,PlayerStateType::Skill_3,PlayerStateType::Skill_4}) m_states[type]=make_shared<PlayerState>(type);
        m_currentState=m_states[PlayerStateType::Wait];
        OnSkillUsed=[this](int,shared_ptr<GameObject>){++casts; m_playerInterface->stamina-=30;};
    }
    bool CanChangeState(PlayerStateType type) { return m_currentState->CanTransitionTo(type); }
    PlayerStateType GetCurrentState() const { return m_currentState->type; }
    bool IsInState(PlayerStateType type) const { return GetCurrentState()==type; }
    shared_ptr<GameObject> GetGameObject() { return object; }
    bool IsValidAttackTarget(shared_ptr<GameObject> target) { return target!=nullptr; }
    bool CheckTargetForSkill(KEY_TYPE) { return picked && picked->GetActive(); }
    shared_ptr<GameObject> GetPickedTargetAtMouse() { return picked; }
    void RequestStateChange(PlayerStateType state) { pending.push(state); }
    void ProcessPending() { while(!pending.empty()) {auto state=pending.front();pending.pop();ExecuteStateChange(state);} }
    void HandleSkillInput();
    bool QueueSkillCast(int index,shared_ptr<GameObject> target);
    void ExecuteStateChange(PlayerStateType state);
};
#include "OriginalAdmissionMethods.inc"
void require(bool ok,const char* message) { if(!ok) { cerr<<message<<endl; exit(1); } }
int main() {
    input.down[static_cast<int>(KEY_TYPE::Q)]=true;
    PlayerStateMachine denied;
    denied.m_currentState->allow=false;
    denied.HandleSkillInput(); denied.ProcessPending();
    require(denied.casts==0 && denied.m_playerInterface->stamina==100,"Rejected state change executed and charged a skill");
    PlayerStateMachine accepted;
    input.down[static_cast<int>(KEY_TYPE::W)]=true;
    accepted.HandleSkillInput();
    require(accepted.casts==0 && accepted.pending.size()==1,"A queued cast spent resources or queued multiple skills");
    accepted.ProcessPending();
    require(accepted.casts==1 && accepted.m_playerInterface->stamina==70,"Accepted cast did not charge exactly once");
    PlayerStateMachine depleted;
    depleted.HandleSkillInput(); depleted.m_playerInterface->stamina=0; depleted.ProcessPending();
    require(depleted.casts==0 && depleted.GetCurrentState()==PlayerStateType::Wait,"Resource validation was stale at state acceptance");
    input.down.fill(false); input.down[static_cast<int>(KEY_TYPE::R)]=true;
    PlayerStateMachine targetGone;
    targetGone.HandleSkillInput(); targetGone.picked->active=false; targetGone.ProcessPending();
    require(targetGone.casts==0 && targetGone.m_playerInterface->stamina==100,"An invalidated target consumed resources");
    PlayerStateMachine invalid;
    invalid.picked.reset(); invalid.HandleSkillInput(); invalid.ProcessPending();
    require(invalid.casts==0 && invalid.pending.empty(),"A missing R target was accepted");
}
