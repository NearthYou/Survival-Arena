[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot,
      [Parameter(Mandatory)][string]$CMakeExecutable,
      [ValidateSet('Admission','Progression','Runtime','Collision')][string]$Mode='Progression')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($RepositoryRoot)
$out=Join-Path $root ('out/skill-ranks-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $out | Out-Null
try {
    if ($Mode -eq 'Admission') {
        Copy-Item -LiteralPath (Join-Path $root 'tests/game_skill_admission_fixture.cpp') -Destination (Join-Path $out 'main.cpp')
        $source=[Text.Encoding]::GetEncoding(949).GetString([IO.File]::ReadAllBytes((Join-Path $root 'game/Engine/PlayerStateMachine.cpp')))
        $parts=@()
        foreach ($name in @('SkillStateForIndex','PlayerStateMachine::QueueSkillCast','PlayerStateMachine::HandleSkillInput','PlayerStateMachine::ExecuteStateChange')) {
            $match=[regex]::Matches($source, '(?ms)^(?:static )?(?:PlayerStateType|bool|void) '+[regex]::Escape($name)+'\([^\r\n]*\)\s*\{.*?^\}')
            if ($match.Count -eq 1) { $parts += $match[0].Value }
            elseif ($name -in @('PlayerStateMachine::HandleSkillInput','PlayerStateMachine::ExecuteStateChange')) { throw "Missing actual method: $name" }
        }
        [IO.File]::WriteAllText((Join-Path $out 'OriginalAdmissionMethods.inc'),($parts -join "`n"),[Text.UTF8Encoding]::new($false))
    } elseif ($Mode -eq 'Runtime') {
        Copy-Item -LiteralPath (Join-Path $root 'tests/game_skill_runtime_fixture.cpp') -Destination (Join-Path $out 'main.cpp')
        $source=[Text.Encoding]::GetEncoding(949).GetString([IO.File]::ReadAllBytes((Join-Path $root 'game/Client/BaseSkill.cpp')))
        $ctor=[regex]::Match($source,'(?ms)^BaseSkill::BaseSkill\([^\r\n]*\).*?^\}')
        if (-not $ctor.Success) { throw 'Actual BaseSkill constructor missing' }
        $parts=@($ctor.Value,'BaseSkill::~BaseSkill() {}')
        foreach ($name in @('Update','PlaySkill','SkillEnd','UpdateSkillCoolDown','SkillLevelUp','ConfigureProgression','GetMaxCooldown','GetStaminaCost','GetDamageMultiplier','CanExecuteSkill','ExecuteSkill')) {
            $match=[regex]::Matches($source,'(?ms)^(?:void|float|int|bool) BaseSkill::'+$name+'\([^\r\n]*\)(?: const)?\s*\{.*?^\}')
            if ($match.Count -ne 1) { throw "Actual BaseSkill method missing: $name" }
            $parts += $match[0].Value
        }
        [IO.File]::WriteAllText((Join-Path $out 'OriginalSkillRuntimeMethods.inc'),($parts -join "`n"),[Text.UTF8Encoding]::new($false))
    } elseif ($Mode -eq 'Collision') {
        Copy-Item -LiteralPath (Join-Path $root 'tests/game_skill_collision_fixture.cpp') -Destination (Join-Path $out 'main.cpp')
        $source=[Text.Encoding]::GetEncoding(949).GetString([IO.File]::ReadAllBytes((Join-Path $root 'game/Client/BiancaESkillCircle.cpp')))
        $source=[regex]::Replace($source,'(?m)^#include[^\r\n]*','')
        [IO.File]::WriteAllText((Join-Path $out 'OriginalSkillCollisionMethods.inc'),$source,[Text.UTF8Encoding]::new($false))
        $cone=[Text.Encoding]::GetEncoding(949).GetString([IO.File]::ReadAllBytes((Join-Path $root 'game/Client/BiancaQCone.cpp')))
        $collision=[regex]::Match($cone,'(?ms)^void BiancaQCone::OnCollisionEnter\([^\r\n]*\)\s*\{.*?^\}')
        if (-not $collision.Success) { throw 'Actual Q collision method missing' }
        [IO.File]::WriteAllText((Join-Path $out 'OriginalConeCollisionMethod.inc'),$collision.Value,[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText((Join-Path $out 'GameObject.h'),"#pragma once`n#include <memory>`n#include <unordered_set>`nusing namespace std;`nstruct GameObject { virtual ~GameObject() = default; virtual void Start() {} virtual void Update() {} virtual void LateUpdate() {} };`n")
    } else {
        Copy-Item -LiteralPath (Join-Path $root 'tests/game_skill_progression_fixture.cpp') -Destination (Join-Path $out 'main.cpp')
    }
    $include=(Join-Path $root 'game/Client').Replace('\','/')
    $engine=(Join-Path $root 'game/Engine').Replace('\','/')
    $boundary=$out.Replace('\','/')
    [IO.File]::WriteAllText((Join-Path $out 'CMakeLists.txt'),"cmake_minimum_required(VERSION 3.25)`nproject(SkillRanks LANGUAGES CXX)`nadd_executable(skill_ranks main.cpp)`ntarget_compile_features(skill_ranks PRIVATE cxx_std_17)`ntarget_compile_options(skill_ranks PRIVATE /utf-8)`ntarget_include_directories(skill_ranks PRIVATE `"$boundary`" `"$include`" `"$engine`")`n")
    & $CMakeExecutable -S $out -B (Join-Path $out 'build') -G 'Visual Studio 17 2022' -A x64 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Skill fixture configuration failed' }
    $built=& $CMakeExecutable --build (Join-Path $out 'build') --config Debug 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Skill fixture compilation failed: $built" }
    & (Join-Path $out 'build/Debug/skill_ranks.exe')
    if ($LASTEXITCODE -ne 0) { throw "$Mode skill behavior failed" }
    "PASS: $Mode skill behavior"
} finally {
    $allowed=(Join-Path $root 'out')+[IO.Path]::DirectorySeparatorChar
    if (-not $out.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $out) -notmatch '^skill-ranks-[0-9a-f]{8}$') { throw 'Skill fixture cleanup boundary mismatch' }
    Remove-Item -LiteralPath $out -Recurse -Force
}
