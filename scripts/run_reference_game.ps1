[CmdletBinding()]
param([Parameter(Mandatory)][string]$RuntimeRoot,
      [switch]$ValidateOnly,
      [switch]$Show)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'reference_game_patches.ps1')
$runtime = (Resolve-Path -LiteralPath $RuntimeRoot).Path.TrimEnd('\','/')
$receipt = Get-Content -LiteralPath (Join-Path $runtime 'oracle-build.json') -Raw -Encoding utf8 | ConvertFrom-Json -Depth 20
$adjustments = @()
if ($receipt.PSObject.Properties.Name -contains 'runtime_adjustments') { $adjustments = @($receipt.runtime_adjustments) }
if (@($adjustments | Select-Object -Unique).Count -ne $adjustments.Count -or
    @($adjustments | Where-Object { $_ -cnotin @('collision-debug-toggle-v1','craft-state-preparation-v1','character-selection-index-v1','skin-selection-isolation-v1','picking-camera-sync-v1','player-animation-sync-v1','player-animation-episode-sync-v1','walkable-path-smoothing-v1','combat-approach-animation-v1','audio-category-settings-v1') }).Count -gt 0) {
    throw 'Unsupported runtime adjustment'
}
$hasCollisionToggle = $adjustments -ccontains 'collision-debug-toggle-v1'
$hasCraftPreparation = $adjustments -ccontains 'craft-state-preparation-v1'
$hasStableSelection = $adjustments -ccontains 'character-selection-index-v1'
$hasIndependentSkinSelection = $adjustments -ccontains 'skin-selection-isolation-v1'
$hasPickingSync = $adjustments -ccontains 'picking-camera-sync-v1'
$hasAnimationSync = $adjustments -ccontains 'player-animation-sync-v1'
$hasAnimationEpisodes = $adjustments -ccontains 'player-animation-episode-sync-v1'
$hasWalkablePath = $adjustments -ccontains 'walkable-path-smoothing-v1'
$hasCombatAnimation = $adjustments -ccontains 'combat-approach-animation-v1'
$hasAudioSettings = $adjustments -ccontains 'audio-category-settings-v1'
if ($hasCombatAnimation -and -not $hasAnimationSync) { throw 'Combat animation requires player animation synchronization' }
if ($hasAnimationEpisodes -and -not $hasAnimationSync) { throw 'Animation episodes require player animation synchronization' }
if ($hasIndependentSkinSelection -and -not $hasStableSelection) { throw 'Independent skin selection requires stable character selection' }
if (($receipt.PSObject.Properties.Name -contains 'instrumentation' -and $receipt.instrumentation) -or
    $receipt.source_logic_modified -cne ($hasCollisionToggle -or $hasCraftPreparation -or $hasStableSelection -or $hasPickingSync -or $hasAnimationSync -or $hasWalkablePath -or $hasCombatAnimation -or $hasAudioSettings)) { throw 'Instrumented oracle is not the playable game' }
if ($receipt.source_commit -cne '01b820a3ebcfd473a898dec1f5bc67c4ac77261e') {
    throw 'Unexpected source game revision'
}
function Assert-SealedFile([string]$Relative, [string]$Hash) {
    if ([string]::IsNullOrWhiteSpace($Relative) -or [IO.Path]::IsPathRooted($Relative) -or
        $Relative.Contains(':') -or $Relative.Replace('\','/').Split('/') -contains '..') {
        throw "Uncontained runtime path: $Relative"
    }
    $path = [IO.Path]::GetFullPath((Join-Path $runtime $Relative))
    if (-not $path.StartsWith($runtime + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
        throw "Uncontained runtime path: $Relative"
    }
    $entry = Get-Item -LiteralPath $path
    if ($entry.PSIsContainer -or ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Runtime code must be a regular file: $Relative"
    }
    if ($Hash -notmatch '^[0-9a-f]{64}$' -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $Hash) {
        throw "File hash mismatch: $Relative"
    }
}
if (@($receipt.source_files).Count -eq 0 -or @($receipt.runtime_files).Count -eq 0) {
    throw 'Playable runtime requires complete source and binary receipts'
}
$patchedScenes = 0
$patchedSelections = 0
$patchedPicking = 0
$patchedAnimation = 0
$patchedAnimationHeader = 0
$patchedNavigation = 0
$patchedAudio = 0
$patchedAudioHeader = 0
foreach ($file in $receipt.source_files) {
    $isAdjustedLumiaScene = ($hasCollisionToggle -or $hasCraftPreparation) -and $file.path.Replace('\','/') -ceq 'Client/LumiaIsland.cpp'
    $isSelectionScene = $hasStableSelection -and $file.path.Replace('\','/') -ceq 'Client/CharacterSelectScene.cpp'
    $isPickingManager = $hasPickingSync -and $file.path.Replace('\','/') -ceq 'Engine/SceneObjectManager.cpp'
    $isPlayerAnimator = $hasAnimationSync -and $file.path.Replace('\','/') -ceq 'Engine/AnimationStateMachine.cpp'
    $isAnimationHeader = $hasAnimationEpisodes -and $file.path.Replace('\','/') -ceq 'Engine/AnimationStateMachine.h'
    $isNavigation = $hasWalkablePath -and $file.path.Replace('\','/') -ceq 'Engine/NavMesh.cpp'
    $isAudio = $hasAudioSettings -and $file.path.Replace('\','/') -ceq 'Engine/SoundManager.cpp'
    $isAudioHeader = $hasAudioSettings -and $file.path.Replace('\','/') -ceq 'Engine/SoundManager.h'
    if ($isAdjustedLumiaScene -or $isSelectionScene -or $isPickingManager -or $isPlayerAnimator -or $isAnimationHeader -or $isNavigation -or $isAudio -or $isAudioHeader) {
        $snapshot = 'source-snapshot/' + $file.path
        Assert-SealedFile $snapshot $file.sha256
        $encoding = [Text.Encoding]::GetEncoding(28591)
        $original = $encoding.GetString([IO.File]::ReadAllBytes((Join-Path $runtime $snapshot)))
        if ($isAdjustedLumiaScene) {
            ++$patchedScenes
            $expected = $original
            if ($hasCollisionToggle) { $expected = Add-ReferenceCollisionDebugToggle $expected }
            if ($hasCraftPreparation) { $expected = Add-ReferenceCraftStatePreparation $expected }
            $label = if ($hasCraftPreparation) { 'Craft preparation' } else { 'Collision debug' }
        } elseif ($isSelectionScene) {
            ++$patchedSelections
            $expected = Add-ReferenceStableCharacterSelection $original
            if ($hasIndependentSkinSelection) { $expected = Add-ReferenceIndependentSkinSelection $expected }
            $label = 'Character selection'
        } elseif ($isPickingManager) {
            ++$patchedPicking
            $expected = Add-ReferencePickingMatrixSync $original
            $label = 'Picking matrix'
        } elseif ($isPlayerAnimator) {
            ++$patchedAnimation
            $expected = Add-ReferencePlayerAnimationSync $original
            if ($hasCombatAnimation) { $expected = Add-ReferenceCombatAnimationSync $expected }
            if ($hasAnimationEpisodes) { $expected = Add-ReferenceAnimationEpisodeSync $expected }
            $label = 'Player animation'
        } elseif ($isAnimationHeader) {
            ++$patchedAnimationHeader
            $expected = Add-ReferenceAnimationEpisodeHeader $original
            $label = 'Animation episode header'
        } elseif ($isNavigation) {
            ++$patchedNavigation
            $expected = Add-ReferenceWalkablePathSmoothing $original
            $label = 'Walkable path'
        } elseif ($isAudio) {
            ++$patchedAudio
            $expected = Add-ReferenceAudioSettings $original
            $label = 'Audio settings'
        } else {
            ++$patchedAudioHeader
            $expected = Add-ReferenceAudioSettingsHeader $original
            $label = 'Audio settings'
        }
        $expectedHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($encoding.GetBytes($expected))).ToLowerInvariant()
        if ($expectedHash -cne $file.compiled_sha256) { throw "$label adjustment differs from the approved patch" }
    } elseif ($file.sha256 -cne $file.compiled_sha256) { throw "Source logic differs: $($file.path)" }
    Assert-SealedFile $file.path $file.compiled_sha256
}
if ($hasCollisionToggle -and $patchedScenes -ne 1) { throw 'Collision debug adjustment requires exactly one scene receipt' }
if ($hasCraftPreparation -and $patchedScenes -ne 1) { throw 'Craft preparation adjustment requires exactly one scene receipt' }
if ($hasStableSelection -and $patchedSelections -ne 1) { throw 'Character selection adjustment requires exactly one scene receipt' }
if ($hasPickingSync -and $patchedPicking -ne 1) { throw 'Picking matrix adjustment requires exactly one manager receipt' }
if ($hasAnimationSync -and $patchedAnimation -ne 1) { throw 'Player animation adjustment requires exactly one state-machine receipt' }
if ($hasAnimationEpisodes -and $patchedAnimationHeader -ne 1) { throw 'Animation episodes require exactly one header receipt' }
if ($hasWalkablePath -and $patchedNavigation -ne 1) { throw 'Walkable path adjustment requires exactly one navigation receipt' }
if ($hasAudioSettings -and ($patchedAudio -ne 1 -or $patchedAudioHeader -ne 1)) { throw 'Audio settings adjustment requires implementation and header receipts' }
foreach ($file in $receipt.runtime_files) { Assert-SealedFile $file.path $file.sha256 }
Assert-SealedFile 'Binaries/Client.exe' $receipt.executable_sha256
if (-not (Test-Path -LiteralPath (Join-Path $runtime 'Resources') -PathType Container)) {
    throw 'Source game Resources directory is missing'
}
$executable = Join-Path $runtime 'Binaries/Client.exe'
if ($ValidateOnly) {
    [pscustomobject]@{executable=$executable;source_commit=$receipt.source_commit;runtime_adjustments=$adjustments;validated=$true}
    return
}
$process = Start-Process -FilePath $executable -WorkingDirectory (Join-Path $runtime 'Binaries') `
    -WindowStyle $(if ($Show) {'Normal'} else {'Hidden'}) -PassThru
[pscustomobject]@{process_id=$process.Id;executable=$executable;source_commit=$receipt.source_commit}
