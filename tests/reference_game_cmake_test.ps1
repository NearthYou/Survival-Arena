[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot,
      [Parameter(Mandatory)][string]$CMakeExecutable)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$testOutputs = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'out'))
# MSBuild deliberately disables incremental builds below the system Temp folder.
$temporary = Join-Path $testOutputs ('dxa-reference-cmake-test-' + [Guid]::NewGuid().ToString('N'))
$project = Join-Path $temporary 'fixture project'
$build = Join-Path $temporary 'build'
New-Item -ItemType Directory -Path (Join-Path $project 'scripts') -Force | Out-Null
function Save([string]$Path, [string]$Text) { [IO.File]::WriteAllText($Path,$Text,[Text.UTF8Encoding]::new($false)) }
try {
    Save (Join-Path $project 'scripts/build_reference_oracle.ps1') @'
param($SourceRoot,$SourceRevision,$OutputRoot,[switch]$CollisionDebugToggle,[switch]$PreparedCraftState,[switch]$StableCharacterSelection,[switch]$IndependentSkinSelection,[switch]$SynchronizedPicking,[switch]$SynchronizedPlayerAnimation,[switch]$SynchronizedAnimationEpisodes,[switch]$WalkablePathSmoothing,[switch]$SynchronizedCombatAnimation,[switch]$PersistentAudioSettings)
$ErrorActionPreference='Stop'
if(-not $CollisionDebugToggle) { throw 'Playable game must enable the requested collision debug toggle' }
if(-not $PreparedCraftState) { throw 'Playable game must prepare the upcoming craft state' }
if(-not $StableCharacterSelection) { throw 'Playable game must preserve character selection' }
if(-not $IndependentSkinSelection) { throw 'Playable game must keep skin selection separate from quick start' }
if(-not $SynchronizedPicking) { throw 'Playable game must synchronize picking camera matrices' }
if(-not $SynchronizedPlayerAnimation) { throw 'Playable game must preserve accepted action animations' }
if(-not $SynchronizedAnimationEpisodes) { throw 'Playable game must not replay completed action animations' }
if(-not $WalkablePathSmoothing) { throw 'Playable game must preserve walkable corner paths' }
if(-not $SynchronizedCombatAnimation) { throw 'Playable game must retain locomotion while pursuing an attack target' }
if(-not $PersistentAudioSettings) { throw 'Playable game must retain audio category settings for new and overlapping voices' }
if(Test-Path -LiteralPath $OutputRoot) { throw 'CMake precreated immutable output directory' }
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $OutputRoot 'oracle-build.json'),'fixture receipt')
'BUILDER_EXECUTED'
'@
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'scripts/reference_game_patches.ps1') -Destination (Join-Path $project 'scripts/reference_game_patches.ps1')
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'scripts/prepare_reference_game.ps1') -Destination (Join-Path $project 'scripts/prepare_reference_game.ps1')
    Save (Join-Path $project 'scripts/run_reference_game.ps1') @'
param($RuntimeRoot,[switch]$ValidateOnly,[switch]$Show)
if (-not $ValidateOnly) { throw 'Test must not launch a game window' }
if (-not (Test-Path -LiteralPath (Join-Path $RuntimeRoot 'oracle-build.json'))) { throw 'Missing build receipt' }
Write-Host 'VALIDATED'
[pscustomobject]@{source_commit='01b820a3ebcfd473a898dec1f5bc67c4ac77261e';runtime_adjustments=@('collision-debug-toggle-v1','craft-state-preparation-v1','character-selection-index-v1','skin-selection-isolation-v1','picking-camera-sync-v1','player-animation-sync-v1','player-animation-episode-sync-v1','walkable-path-smoothing-v1','combat-approach-animation-v1','audio-category-settings-v1');validated=$true}
'@
    $module = (Join-Path $RepositoryRoot 'cmake/ReferenceGame.cmake').Replace('\','/')
    Save (Join-Path $project 'CMakeLists.txt') "cmake_minimum_required(VERSION 3.25)`nproject(ReferenceGameContract NONE)`ninclude(`"$module`")`n"
    & $CMakeExecutable -S $project -B $build -G 'Visual Studio 17 2022' -A x64 `
        "-DDXA_REFERENCE_SOURCE_ROOT=$project" "-DDXA_REFERENCE_RUNTIME_ROOT=$temporary/runtime" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    $first = & $CMakeExecutable --build $build --config Debug --target dxa_reference_game 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Source game target failed: $first" }
    if (-not ($first -match 'BUILDER_EXECUTED')) { throw 'Target did not invoke builder' }
    $second = & $CMakeExecutable --build $build --config Debug --target dxa_reference_game 2>&1
    if ($LASTEXITCODE -ne 0 -or ($second -match 'BUILDER_EXECUTED') -or -not ($second -match 'VALIDATED')) {
        throw "Incremental target rebuilt immutable output or skipped validation: $second"
    }
    & $CMakeExecutable --build $build --config Debug --target clean | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Fixture clean failed' }
    $afterClean = & $CMakeExecutable --build $build --config Debug --target dxa_reference_game 2>&1
    if ($LASTEXITCODE -ne 0 -or ($afterClean -match 'BUILDER_EXECUTED') -or -not ($afterClean -match 'VALIDATED')) {
        throw "Clean rebuild must revalidate and reuse the existing immutable game: $afterClean"
    }
    $freshBuild = Join-Path $temporary 'fresh build'
    & $CMakeExecutable -S $project -B $freshBuild -G 'Visual Studio 17 2022' -A x64 `
        "-DDXA_REFERENCE_SOURCE_ROOT=$project" "-DDXA_REFERENCE_RUNTIME_ROOT=$temporary/runtime" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Fresh build configuration failed' }
    $fresh = & $CMakeExecutable --build $freshBuild --config Debug --target dxa_reference_game 2>&1
    if ($LASTEXITCODE -ne 0 -or ($fresh -match 'BUILDER_EXECUTED') -or -not ($fresh -match 'VALIDATED')) {
        throw "A new CMake build folder must reuse the validated immutable game: $fresh"
    }
    & $CMakeExecutable -S $project -B $build "-DDXA_REFERENCE_RUNTIME_ROOT=$temporary/runtime-next" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Runtime root reconfiguration failed' }
    $relocated = & $CMakeExecutable --build $build --config Debug --target dxa_reference_game 2>&1
    if ($LASTEXITCODE -ne 0 -or -not ($relocated -match 'BUILDER_EXECUTED')) {
        throw "Runtime root change reused stale build stamp: $relocated"
    }
    $alternativeSource = Join-Path $temporary 'alternative source'
    New-Item -ItemType Directory -Path $alternativeSource | Out-Null
    & $CMakeExecutable -S $project -B $build "-DDXA_REFERENCE_SOURCE_ROOT=$alternativeSource" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Source root reconfiguration failed' }
    $repointed = & $CMakeExecutable --build $build --config Debug --target dxa_reference_game 2>&1
    if ($LASTEXITCODE -ne 0 -or -not ($repointed -match 'BUILDER_EXECUTED')) {
        throw "Source root change reused previous source/resource binding: $repointed"
    }
    [IO.File]::AppendAllText((Join-Path $project 'scripts/reference_game_patches.ps1'), "`n# changed fixture patch`n")
    $patched = & $CMakeExecutable --build $build --config Debug --target dxa_reference_game 2>&1
    if ($LASTEXITCODE -ne 0 -or -not ($patched -match 'BUILDER_EXECUTED')) {
        throw "Runtime patch change reused an earlier immutable build: $patched"
    }
    [IO.File]::AppendAllText((Join-Path $project 'scripts/prepare_reference_game.ps1'), "`n# changed fixture preparation policy`n")
    $prepared = & $CMakeExecutable --build $build --config Debug --target dxa_reference_game 2>&1
    if ($LASTEXITCODE -ne 0 -or -not ($prepared -match 'BUILDER_EXECUTED')) {
        throw "Preparation policy change reused an earlier immutable build: $prepared"
    }
    Write-Output 'PASS: CMake builds immutable external runtime once and revalidates without launching it'
} finally {
    $resolved = [IO.Path]::GetFullPath($temporary)
    $prefix = $testOutputs.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or
        -not (Split-Path -Leaf $resolved).StartsWith('dxa-reference-cmake-test-')) { throw 'Temporary cleanup boundary mismatch' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
