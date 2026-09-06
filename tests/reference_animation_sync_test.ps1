[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot,
      [Parameter(Mandatory)][string]$SourceFile,
      [Parameter(Mandatory)][string]$CMakeExecutable,
      [switch]$ApplyPatch,
      [switch]$ApplyCombatPatch,
      [switch]$ApplyEpisodePatch,
      [switch]$CheckCombatMovement,
      [switch]$CheckCompletedAction)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = [Text.Encoding]::GetEncoding(949).GetString([IO.File]::ReadAllBytes($SourceFile))
if ($ApplyPatch) {
    . (Join-Path $RepositoryRoot 'scripts/reference_game_patches.ps1')
    $source = Add-ReferencePlayerAnimationSync $source
}
if ($ApplyCombatPatch) {
    if (-not $ApplyPatch) { throw 'Combat animation patch requires base player synchronization' }
    $source = Add-ReferenceCombatAnimationSync $source
}
$episodeFields = ''
if ($ApplyEpisodePatch) {
    if (-not $ApplyPatch) { throw 'Animation episode patch requires base player synchronization' }
    $source = Add-ReferenceAnimationEpisodeSync $source
    $headerPath = [IO.Path]::ChangeExtension($SourceFile, '.h')
    $header = [Text.Encoding]::GetEncoding(949).GetString([IO.File]::ReadAllBytes($headerPath))
    $header = Add-ReferenceAnimationEpisodeHeader $header
    $episodeFields = ([regex]::Matches($header, '(?m)^[\t ]*(?:AnimationStateType|bool) m_reference\w+ = [^;]+;') | ForEach-Object Value) -join "`n"
}
$parts = @()
if ($ApplyPatch) {
    $helper = [regex]::Matches($source, '(?ms)^static bool ReferenceAcceptedPlayerAnimation\(.*?^\}')
    if ($helper.Count -ne 1) { throw 'Missing accepted-animation mapping' }
    $parts += $helper[0].Value
}
foreach ($name in @('Update','CanChangeState','HandleAutoTransitions')) {
    $method = [regex]::Matches($source, ('(?ms)^(?:void|bool) AnimationStateMachine::' + $name + '\([^)]*\)\s*\{.*?^\}'))
    if ($method.Count -ne 1) { throw "Unexpected animation method: $name" }
    $parts += $method[0].Value
}
$fixture = @'
#include <memory>
#include <queue>
#include <unordered_map>
#include <iostream>
#include <cstdlib>
using namespace std;
enum class PlayerStateType { Wait, Run, Skill_1, Skill_2, Skill_3, Skill_4, Craft, Die, BaseAttack, Counter };
enum class AnimationStateType { Wait, Run, Skill_1, Skill_2, Skill_3, Skill_4, Craft, BaseAttack, Counter };
struct PlayerStateMachine {
    PlayerStateType current = PlayerStateType::Wait;
    PlayerStateType GetCurrentState() { return current; }
};
struct NavMeshAgent {
    bool moving = false;
    bool IsMoving() { return moving; }
};
struct GameObject {
    shared_ptr<PlayerStateMachine> player = make_shared<PlayerStateMachine>();
    shared_ptr<NavMeshAgent> nav = make_shared<NavMeshAgent>();
    shared_ptr<PlayerStateMachine> GetPlayerStateMachine() { return player; }
    shared_ptr<NavMeshAgent> GetNavMeshAgent() { return nav; }
};
struct AnimationState {
    AnimationStateType type;
    bool completed = false;
    int entries = 0;
    AnimationStateType GetType() { return type; }
    bool CanTransitionTo(AnimationStateType next) {
        if (type == AnimationStateType::Counter || type == AnimationStateType::Skill_3)
            return completed && (next == AnimationStateType::Wait || next == AnimationStateType::Run);
        return next != type;
    }
    void Update(shared_ptr<int>) {}
};
struct Component { void Update() {} };
struct AnimationStateMachine : Component {
    using Super = Component;
    // REFERENCE_EPISODE_FIELDS
    unordered_map<AnimationStateType, shared_ptr<AnimationState>> m_states;
    shared_ptr<AnimationState> m_currentState;
    shared_ptr<int> m_animator = make_shared<int>(1);
    shared_ptr<GameObject> object = make_shared<GameObject>();
    queue<AnimationStateType> m_stateChangeQueue;
    unordered_map<AnimationStateType, AnimationStateType> m_autoTransitions;
    AnimationStateMachine() {
        for (auto type : {AnimationStateType::Wait, AnimationStateType::Run, AnimationStateType::BaseAttack, AnimationStateType::Counter, AnimationStateType::Skill_3})
            m_states[type] = make_shared<AnimationState>(AnimationState{type});
        m_currentState = m_states[AnimationStateType::Counter];
    }
    shared_ptr<GameObject> GetGameObject() { return object; }
    AnimationStateType GetCurrentState() { return m_currentState->type; }
    bool IsCurrentAnimationCompleted() { return m_currentState->completed; }
    void PrintCurState() {}
    void CheckAnimationCompletion() {}
    void RequestStateChange(AnimationStateType next) { m_stateChangeQueue.push(next); }
    void ExecuteStateChange(AnimationStateType next) {
        if (!CanChangeState(next)) return;
        m_currentState = m_states.at(next);
        ++m_currentState->entries;
    }
    bool CanChangeState(AnimationStateType);
    void HandleAutoTransitions();
    void Update();
};
'@
$fixture = $fixture.Replace('// REFERENCE_EPISODE_FIELDS', $episodeFields)
$checks = @'
void require(bool value, const char* message) { if (!value) { cerr << message << endl; exit(1); } }
int main() {
    AnimationStateMachine combo;
    combo.object->player->current = PlayerStateType::Skill_3;
    combo.RequestStateChange(AnimationStateType::Skill_3);
    combo.Update();
    require(combo.GetCurrentState() == AnimationStateType::Skill_3, "Accepted E was discarded while Counter visual was still finishing");
    combo.RequestStateChange(AnimationStateType::Skill_3); combo.Update();
    require(combo.m_currentState->entries == 1, "Duplicate request restarted the active animation");

    AnimationStateMachine delayed;
    delayed.RequestStateChange(AnimationStateType::Skill_3); delayed.Update();
    require(delayed.GetCurrentState() == AnimationStateType::Counter, "Unaccepted action interrupted the counter");
    delayed.object->player->current = PlayerStateType::Skill_3;
    delayed.Update(); delayed.Update();
    require(delayed.GetCurrentState() == AnimationStateType::Skill_3, "Later gameplay acceptance did not recover a discarded animation request");

    AnimationStateMachine settle;
    settle.RequestStateChange(AnimationStateType::Wait); settle.Update();
    require(settle.GetCurrentState() == AnimationStateType::Counter, "Idle gameplay truncated the original counter tail");
    settle.m_currentState->completed = true;
    settle.Update(); settle.Update();
    require(settle.GetCurrentState() == AnimationStateType::Wait, "Completed counter never returned to idle after the early Wait request");

    AnimationStateMachine monster;
    monster.object->player.reset();
    monster.RequestStateChange(AnimationStateType::Skill_3); monster.Update();
    require(monster.GetCurrentState() == AnimationStateType::Counter, "Player animation sync changed a non-player transition");

#ifdef REFERENCE_CHECK_COMPLETED_ACTION
    AnimationStateMachine completed;
    completed.object->player->current = PlayerStateType::Skill_3;
    completed.RequestStateChange(AnimationStateType::Skill_3);
    completed.Update();
    require(completed.m_states[AnimationStateType::Skill_3]->entries == 1, "Accepted action did not begin once");
    completed.m_currentState->completed = true;
    completed.RequestStateChange(AnimationStateType::Wait);
    completed.Update();
    require(completed.GetCurrentState() == AnimationStateType::Wait, "Completed action did not leave its animation");
    // The player-completion event is still deferred. It is not a fresh cast.
    completed.Update();
    require(completed.GetCurrentState() == AnimationStateType::Wait, "Completed action replayed before deferred gameplay completion");
    require(completed.m_states[AnimationStateType::Skill_3]->entries == 1, "A stale gameplay state restarted the completed clip");
    completed.object->player->current = PlayerStateType::Wait;
    completed.Update(); completed.Update();
    require(completed.GetCurrentState() == AnimationStateType::Wait, "Stale action survived gameplay completion");
    completed.object->player->current = PlayerStateType::Skill_3;
    completed.m_states[AnimationStateType::Skill_3]->completed = false;
    completed.Update(); completed.Update();
    require(completed.GetCurrentState() == AnimationStateType::Skill_3, "A new cast of the same action was suppressed");
    require(completed.m_currentState->entries == 2, "A new accepted action did not start exactly once");
#endif

#ifdef REFERENCE_CHECK_COMBAT
    AnimationStateMachine chase;
    chase.object->player->current = PlayerStateType::BaseAttack;
    chase.object->nav->moving = true;
    chase.m_currentState = chase.m_states[AnimationStateType::Run];
    chase.Update(); chase.Update();
    require(chase.GetCurrentState() == AnimationStateType::Run, "Combat intent overwrote chase locomotion with an attack animation");
    chase.object->nav->moving = false;
    chase.Update(); chase.Update();
    require(chase.GetCurrentState() == AnimationStateType::BaseAttack, "Arrival did not restore the attack animation");
    chase.object->nav->moving = true;
    chase.Update(); chase.Update();
    require(chase.GetCurrentState() == AnimationStateType::Run, "Target pursuit did not restore locomotion");
    chase.object->player->current = PlayerStateType::Skill_3;
    chase.Update(); chase.Update();
    require(chase.GetCurrentState() == AnimationStateType::Skill_3, "Navigation state overrode an accepted skill");
    chase.object->player->current = PlayerStateType::BaseAttack;
    chase.object->nav.reset();
    chase.Update(); chase.Update();
    require(chase.GetCurrentState() == AnimationStateType::BaseAttack, "Missing optional navigation broke combat animation");
#endif
}
'@
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('ras-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    $defines = if ($CheckCombatMovement) { "#define REFERENCE_CHECK_COMBAT`n" } else { '' }
    if ($CheckCompletedAction) { $defines += "#define REFERENCE_CHECK_COMPLETED_ACTION`n" }
    [IO.File]::WriteAllText((Join-Path $temporary 'main.cpp'), $defines + $fixture + "`n" + ($parts -join "`n") + "`n" + $checks)
    [IO.File]::WriteAllText((Join-Path $temporary 'CMakeLists.txt'), "cmake_minimum_required(VERSION 3.25)`nproject(AnimationSync LANGUAGES CXX)`nadd_executable(animation_sync main.cpp)`ntarget_compile_options(animation_sync PRIVATE /utf-8)`n")
    $build = Join-Path $temporary 'build'
    & $CMakeExecutable -S $temporary -B $build -G 'Visual Studio 17 2022' -A x64 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Animation sync fixture configuration failed' }
    & $CMakeExecutable --build $build --config Debug --target animation_sync | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Animation sync fixture compilation failed' }
    & (Join-Path $build 'Debug/animation_sync.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Accepted gameplay animation synchronization failed' }
    Write-Output 'PASS: accepted combo animations, delayed acceptance, idle tail recovery, duplicate requests and non-player isolation'
    if ($CheckCombatMovement) { Write-Output 'PASS: combat pursuit keeps locomotion, arrival restores attack and skill animation remains authoritative' }
    if ($CheckCompletedAction) { Write-Output 'PASS: completed action does not replay while player completion is deferred; a new cast still starts' }
} finally {
    $resolved = [IO.Path]::GetFullPath($temporary)
    $prefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $resolved) -notmatch '^ras-[0-9a-f]{8}$') { throw 'Temporary cleanup boundary mismatch' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
