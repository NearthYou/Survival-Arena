[CmdletBinding()]
param([Parameter(Mandatory)][string]$SourceRoot,
      [Parameter(Mandatory)][string]$SourceRevision,
      [Parameter(Mandatory)][string]$OutputRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not (Test-Path -LiteralPath (Join-Path $OutputRoot 'oracle-build.json') -PathType Leaf)) {
    & (Join-Path $PSScriptRoot 'build_reference_oracle.ps1') -SourceRoot $SourceRoot `
        -SourceRevision $SourceRevision -OutputRoot $OutputRoot -CollisionDebugToggle -PreparedCraftState -StableCharacterSelection -IndependentSkinSelection -SynchronizedPicking -SynchronizedPlayerAnimation -SynchronizedAnimationEpisodes -WalkablePathSmoothing -SynchronizedCombatAnimation -PersistentAudioSettings
}
# A clean or separate CMake build loses only its stamp, not this immutable game.
$validated = & (Join-Path $PSScriptRoot 'run_reference_game.ps1') -RuntimeRoot $OutputRoot -ValidateOnly
if ($validated.source_commit -cne $SourceRevision -or @($validated.runtime_adjustments).Count -ne 10 -or
    $validated.runtime_adjustments -cnotcontains 'collision-debug-toggle-v1' -or
    $validated.runtime_adjustments -cnotcontains 'craft-state-preparation-v1' -or
    $validated.runtime_adjustments -cnotcontains 'character-selection-index-v1' -or
    $validated.runtime_adjustments -cnotcontains 'skin-selection-isolation-v1' -or
    $validated.runtime_adjustments -cnotcontains 'picking-camera-sync-v1' -or
    $validated.runtime_adjustments -cnotcontains 'player-animation-sync-v1' -or
    $validated.runtime_adjustments -cnotcontains 'player-animation-episode-sync-v1' -or
    $validated.runtime_adjustments -cnotcontains 'walkable-path-smoothing-v1' -or
    $validated.runtime_adjustments -cnotcontains 'combat-approach-animation-v1' -or
    $validated.runtime_adjustments -cnotcontains 'audio-category-settings-v1') {
    throw 'Existing game does not match the requested revision and runtime adjustments'
}
$validated
