[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$runner = Join-Path $RepositoryRoot 'scripts/run_reference_game.ps1'
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('dxa-reference-game-test-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $temporary 'Binaries'),(Join-Path $temporary 'Resources'),(Join-Path $temporary 'Shaders') -Force | Out-Null
function Save-Receipt {
    [IO.File]::WriteAllText((Join-Path $temporary 'oracle-build.json'), ($receipt | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
}
function Expect-Failure([string]$Expected) {
    $message = ''
    try { & $runner -RuntimeRoot $temporary -ValidateOnly | Out-Null } catch { $message = $_.Exception.Message }
    if (-not $message.Contains($Expected)) { throw "Expected '$Expected'; received '$message'" }
}
try {
    $executable = Join-Path $temporary 'Binaries/Client.exe'
    $shader = Join-Path $temporary 'Shaders/test.fx'
    [IO.File]::WriteAllText($executable, 'fixture only, never execute')
    [IO.File]::WriteAllText($shader, 'source shader')
    $exeHash = (Get-FileHash $executable -Algorithm SHA256).Hash.ToLowerInvariant()
    $shaderHash = (Get-FileHash $shader -Algorithm SHA256).Hash.ToLowerInvariant()
    $receipt = [ordered]@{
        source_commit='01b820a3ebcfd473a898dec1f5bc67c4ac77261e'
        source_logic_modified=$false
        runtime_adjustments=@()
        instrumentation=$null
        executable_sha256=$exeHash
        source_files=@([ordered]@{path='Shaders/test.fx';sha256=$shaderHash;compiled_sha256=$shaderHash})
        runtime_files=@([ordered]@{path='Binaries/Client.exe';sha256=$exeHash})
    }
    Save-Receipt
    & $runner -RuntimeRoot $temporary -ValidateOnly | Out-Null
    $receipt.source_logic_modified = $true
    Save-Receipt
    Expect-Failure 'Instrumented oracle is not the playable game'
    $receipt.source_logic_modified = $false
    $receipt.source_commit = '5d9aa0d9b421b32bf8004703d286ba5dbfd8bac5'
    Save-Receipt
    Expect-Failure 'Unexpected source game revision'
    $receipt.source_commit = '01b820a3ebcfd473a898dec1f5bc67c4ac77261e'
    $receipt.source_files[0].compiled_sha256 = '0' * 64
    Save-Receipt
    Expect-Failure 'Source logic differs'
    $receipt.source_files[0].compiled_sha256 = $shaderHash
    Save-Receipt
    [IO.File]::WriteAllText($shader, 'changed shader')
    Expect-Failure 'File hash mismatch'
    [IO.File]::WriteAllText($shader, 'source shader')
    [IO.File]::WriteAllText($executable, 'changed executable')
    Expect-Failure 'File hash mismatch'
    [IO.File]::WriteAllText($executable, 'fixture only, never execute')
    $receipt.runtime_files[0].path = '../outside.exe'
    Save-Receipt
    Expect-Failure 'Uncontained runtime path'
    $receipt.runtime_files[0].path = 'Binaries/Client.exe'

    . (Join-Path $RepositoryRoot 'scripts/reference_game_patches.ps1')
    $original = "void LumiaIsland::LateUpdate()`r`n{`r`n    Super::LateUpdate();`r`n}`r`n"
    $original += "void LumiaIsland::PrepareCraft()`n{`n" + (@(
        '    m_player->GetPlayerStateMachine()->GetCurrentStatePtr()->SetRecipeIndex(0);',
        '    m_player->GetAnimationStateMachine()->GetCurrentStatePtr()->SetExpectedDuration(3.f);',
        '    m_player->GetPlayerStateMachine()->GetCurrentStatePtr()->SetRecipeIndex(0);',
        '    m_player->GetAnimationStateMachine()->GetCurrentStatePtr()->SetExpectedDuration(1.5f);'
    ) -join "`n") + "`n}`n"
    $compiled = Add-ReferenceCollisionDebugToggle $original
    $scene = Join-Path $temporary 'Client/LumiaIsland.cpp'
    $snapshot = Join-Path $temporary 'source-snapshot/Client/LumiaIsland.cpp'
    New-Item -ItemType Directory -Path (Split-Path $scene),(Split-Path $snapshot) -Force | Out-Null
    [IO.File]::WriteAllText($scene, $compiled)
    [IO.File]::WriteAllText($snapshot, $original)
    $sceneHash = (Get-FileHash $scene -Algorithm SHA256).Hash.ToLowerInvariant()
    $originalHash = (Get-FileHash $snapshot -Algorithm SHA256).Hash.ToLowerInvariant()
    $receipt.source_files += [ordered]@{path='Client/LumiaIsland.cpp';sha256=$originalHash;compiled_sha256=$sceneHash}
    $receipt.runtime_adjustments = @('collision-debug-toggle-v1')
    $receipt.source_logic_modified = $true
    Save-Receipt
    & $runner -RuntimeRoot $temporary -ValidateOnly | Out-Null
    $receipt.instrumentation = 'probe hooks'
    Save-Receipt
    Expect-Failure 'Instrumented oracle is not the playable game'
    $receipt.instrumentation = $null
    $receipt.runtime_adjustments = @('unknown-patch')
    Save-Receipt
    Expect-Failure 'Unsupported runtime adjustment'
    $receipt.runtime_adjustments = @('collision-debug-toggle-v1')
    [IO.File]::WriteAllText($scene, $compiled + "`nvoid AlterGameplay() {}")
    $receipt.source_files[1].compiled_sha256 = (Get-FileHash $scene -Algorithm SHA256).Hash.ToLowerInvariant()
    Save-Receipt
    Expect-Failure 'Collision debug adjustment differs'
    [IO.File]::WriteAllText($scene, $compiled)
    $receipt.source_files[1].compiled_sha256 = $sceneHash
    Save-Receipt
    [IO.File]::WriteAllText($snapshot, 'modified original')
    Expect-Failure 'File hash mismatch'
    [IO.File]::WriteAllText($snapshot, $original)
    $selectionOriginal = "void CharacterSelectScene::OnCharacterSelectButtonClicked(int charindex)`n{`n    m_selectCharIdx = charindex - 1;`n}`nvoid CharacterSelectScene::StartLumiaIsland()`n{`n    LumiaIslandScene->SetSelectedCharacter(m_selectCharIdx);`n}`n"
    $selectionOriginal += "void CharacterSelectScene::UpdateSkinList(shared_ptr<Button> button, int charIndex)`n{`n    button->OnClick += [this, button, i]() {`n        UpdateFullImage(button, i);`n        OnCharacterSelectButtonClicked(i);`n    };`n}`n"
    $selectionCompiled = Add-ReferenceStableCharacterSelection $selectionOriginal
    $selectionFile = Join-Path $temporary 'Client/CharacterSelectScene.cpp'
    $selectionSnapshot = Join-Path $temporary 'source-snapshot/Client/CharacterSelectScene.cpp'
    [IO.File]::WriteAllText($selectionFile, $selectionCompiled)
    [IO.File]::WriteAllText($selectionSnapshot, $selectionOriginal)
    $selectionHash = (Get-FileHash $selectionFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $receipt.source_files += [ordered]@{path='Client/CharacterSelectScene.cpp';sha256=(Get-FileHash $selectionSnapshot -Algorithm SHA256).Hash.ToLowerInvariant();compiled_sha256=$selectionHash}
    $receipt.runtime_adjustments = @('collision-debug-toggle-v1','character-selection-index-v1')
    Save-Receipt
    & $runner -RuntimeRoot $temporary -ValidateOnly | Out-Null
    [IO.File]::WriteAllText($selectionFile, $selectionCompiled + "`nvoid WrongCharacter() {}")
    $receipt.source_files[2].compiled_sha256 = (Get-FileHash $selectionFile -Algorithm SHA256).Hash.ToLowerInvariant()
    Save-Receipt
    Expect-Failure 'Character selection adjustment differs'
    [IO.File]::WriteAllText($selectionFile, $selectionCompiled)
    $receipt.source_files[2].compiled_sha256 = $selectionHash
    $isolatedSelection = Add-ReferenceIndependentSkinSelection $selectionCompiled
    [IO.File]::WriteAllText($selectionFile, $isolatedSelection)
    $isolatedHash = (Get-FileHash $selectionFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $receipt.source_files[2].compiled_sha256 = $isolatedHash
    $receipt.runtime_adjustments += 'skin-selection-isolation-v1'
    Save-Receipt
    & $runner -RuntimeRoot $temporary -ValidateOnly | Out-Null
    $receipt.runtime_adjustments = @('collision-debug-toggle-v1','skin-selection-isolation-v1')
    Save-Receipt
    Expect-Failure 'Independent skin selection requires stable character selection'
    $receipt.runtime_adjustments = @('collision-debug-toggle-v1','character-selection-index-v1','skin-selection-isolation-v1')
    [IO.File]::WriteAllText($selectionFile, $selectionCompiled)
    $receipt.source_files[2].compiled_sha256 = $selectionHash
    Save-Receipt
    Expect-Failure 'Character selection adjustment differs'
    [IO.File]::WriteAllText($selectionFile, $isolatedSelection)
    $receipt.source_files[2].compiled_sha256 = $isolatedHash
    $selectionReceipt = $receipt.source_files[2]
    $receipt.source_files = @($receipt.source_files | Where-Object { $_.path -cne 'Client/CharacterSelectScene.cpp' })
    Save-Receipt
    Expect-Failure 'Character selection adjustment requires exactly one scene receipt'
    $receipt.source_files += $selectionReceipt
    $pickingOriginal = "void SceneObjectManager::UpdateQuadTree()`n{`n    CacheScreenBounds();`n}`n"
    $pickingCompiled = Add-ReferencePickingMatrixSync $pickingOriginal
    $pickingFile = Join-Path $temporary 'Engine/SceneObjectManager.cpp'
    $pickingSnapshot = Join-Path $temporary 'source-snapshot/Engine/SceneObjectManager.cpp'
    New-Item -ItemType Directory -Path (Split-Path $pickingFile),(Split-Path $pickingSnapshot) -Force | Out-Null
    [IO.File]::WriteAllText($pickingFile, $pickingCompiled)
    [IO.File]::WriteAllText($pickingSnapshot, $pickingOriginal)
    $pickingHash = (Get-FileHash $pickingFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $receipt.source_files += [ordered]@{path='Engine/SceneObjectManager.cpp';sha256=(Get-FileHash $pickingSnapshot -Algorithm SHA256).Hash.ToLowerInvariant();compiled_sha256=$pickingHash}
    $receipt.runtime_adjustments += 'picking-camera-sync-v1'
    Save-Receipt
    & $runner -RuntimeRoot $temporary -ValidateOnly | Out-Null
    [IO.File]::WriteAllText($pickingFile, $pickingCompiled + "`nvoid WrongPicking() {}")
    $receipt.source_files[3].compiled_sha256 = (Get-FileHash $pickingFile -Algorithm SHA256).Hash.ToLowerInvariant()
    Save-Receipt
    Expect-Failure 'Picking matrix adjustment differs'
    [IO.File]::WriteAllText($pickingFile, $pickingCompiled)
    $receipt.source_files[3].compiled_sha256 = $pickingHash
    $animationOriginal = "#include `"PlayerStateMachine.h`"`nvoid AnimationStateMachine::HandleAutoTransitions()`n{`n}`nbool AnimationStateMachine::CanChangeState(AnimationStateType newState)`n{`n    return false;`n}`n"
    $animationCompiled = Add-ReferencePlayerAnimationSync $animationOriginal
    $animationFile = Join-Path $temporary 'Engine/AnimationStateMachine.cpp'
    $animationSnapshot = Join-Path $temporary 'source-snapshot/Engine/AnimationStateMachine.cpp'
    [IO.File]::WriteAllText($animationFile, $animationCompiled)
    [IO.File]::WriteAllText($animationSnapshot, $animationOriginal)
    $animationHash = (Get-FileHash $animationFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $receipt.source_files += [ordered]@{path='Engine/AnimationStateMachine.cpp';sha256=(Get-FileHash $animationSnapshot -Algorithm SHA256).Hash.ToLowerInvariant();compiled_sha256=$animationHash}
    $receipt.runtime_adjustments += 'player-animation-sync-v1'
    Save-Receipt
    & $runner -RuntimeRoot $temporary -ValidateOnly | Out-Null
    [IO.File]::WriteAllText($animationFile, $animationCompiled + "`nvoid WrongAnimation() {}")
    $receipt.source_files[4].compiled_sha256 = (Get-FileHash $animationFile -Algorithm SHA256).Hash.ToLowerInvariant()
    Save-Receipt
    Expect-Failure 'Player animation adjustment differs'
    [IO.File]::WriteAllText($animationFile, $animationCompiled)
    $receipt.source_files[4].compiled_sha256 = $animationHash
    $navOriginal = "bool NavMesh::IsLineOnNavMesh(const Vec3& start, const Vec3& end, float stepSize)`n{`n    return true;`n}`nbool NavMesh::HasLineOfSight(const Vec3& start, const Vec3& end)`n{`n    return GetDistance(start, end) < 0.1f || IsLineOnNavMesh(start, end);`n}`n"
    $navCompiled = Add-ReferenceWalkablePathSmoothing $navOriginal
    $navFile = Join-Path $temporary 'Engine/NavMesh.cpp'
    $navSnapshot = Join-Path $temporary 'source-snapshot/Engine/NavMesh.cpp'
    [IO.File]::WriteAllText($navFile, $navCompiled)
    [IO.File]::WriteAllText($navSnapshot, $navOriginal)
    $navHash = (Get-FileHash $navFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $receipt.source_files += [ordered]@{path='Engine/NavMesh.cpp';sha256=(Get-FileHash $navSnapshot -Algorithm SHA256).Hash.ToLowerInvariant();compiled_sha256=$navHash}
    $receipt.runtime_adjustments += 'walkable-path-smoothing-v1'
    Save-Receipt
    & $runner -RuntimeRoot $temporary -ValidateOnly | Out-Null
    $combatCompiled = Add-ReferenceCombatAnimationSync $animationCompiled
    [IO.File]::WriteAllText($animationFile, $combatCompiled)
    $combatHash = (Get-FileHash $animationFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $receipt.source_files[4].compiled_sha256 = $combatHash
    $receipt.runtime_adjustments += 'combat-approach-animation-v1'
    Save-Receipt
    & $runner -RuntimeRoot $temporary -ValidateOnly | Out-Null
    $allAdjustments = $receipt.runtime_adjustments
    $receipt.runtime_adjustments = @($allAdjustments | Where-Object { $_ -cne 'player-animation-sync-v1' })
    Save-Receipt
    Expect-Failure 'Combat animation requires player animation synchronization'
    $receipt.runtime_adjustments = $allAdjustments
    [IO.File]::WriteAllText($animationFile, $combatCompiled + "`nvoid WrongCombatMotion() {}")
    $receipt.source_files[4].compiled_sha256 = (Get-FileHash $animationFile -Algorithm SHA256).Hash.ToLowerInvariant()
    Save-Receipt
    Expect-Failure 'Player animation adjustment differs'
    [IO.File]::WriteAllText($animationFile, $combatCompiled)
    $receipt.source_files[4].compiled_sha256 = $combatHash
    $audioOriginal = @'
LoadSoundFile();
m_System->playSound(iter->second, 0, false, &m_Channels[_eID]);
m_System->playSound(iter->second, 0, false, &m_Channels[0]);
void SoundManager::SetBGMVolume(float _volume)
{
}
void SoundManager::SetSFXVolume(float _volume)
{
}
void SoundManager::StopAll()
{
}
'@
    $audioHeaderOriginal = 'class SoundManager { FMOD::System* m_System; };'
    $audioCompiled = Add-ReferenceAudioSettings $audioOriginal
    $audioHeaderCompiled = Add-ReferenceAudioSettingsHeader $audioHeaderOriginal
    foreach ($pair in @(@('cpp',$audioOriginal,$audioCompiled),@('h',$audioHeaderOriginal,$audioHeaderCompiled))) {
        $audioFile = Join-Path $temporary "Engine/SoundManager.$($pair[0])"
        $audioSnapshot = Join-Path $temporary "source-snapshot/Engine/SoundManager.$($pair[0])"
        [IO.File]::WriteAllText($audioFile,$pair[2])
        [IO.File]::WriteAllText($audioSnapshot,$pair[1])
        $receipt.source_files += [ordered]@{path="Engine/SoundManager.$($pair[0])";sha256=(Get-FileHash $audioSnapshot -Algorithm SHA256).Hash.ToLowerInvariant();compiled_sha256=(Get-FileHash $audioFile -Algorithm SHA256).Hash.ToLowerInvariant()}
    }
    $receipt.runtime_adjustments += 'audio-category-settings-v1'
    Save-Receipt
    & $runner -RuntimeRoot $temporary -ValidateOnly | Out-Null
    $audioRecords = @($receipt.source_files | Where-Object { $_.path -like 'Engine/SoundManager.*' })
    foreach ($record in $audioRecords) {
        $audioFile = Join-Path $temporary $record.path
        $validText = Get-Content -LiteralPath $audioFile -Raw
        $validHash = $record.compiled_sha256
        [IO.File]::AppendAllText($audioFile,"`nvoid OverrideAudio() {}")
        $record.compiled_sha256 = (Get-FileHash $audioFile -Algorithm SHA256).Hash.ToLowerInvariant()
        Save-Receipt
        Expect-Failure 'Audio settings adjustment differs'
        [IO.File]::WriteAllText($audioFile,$validText)
        $record.compiled_sha256 = $validHash
    }
    $episodeCompiled = Add-ReferenceAnimationEpisodeSync $combatCompiled
    [IO.File]::WriteAllText($animationFile, $episodeCompiled)
    $receipt.source_files[4].compiled_sha256 = (Get-FileHash $animationFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $episodeHeaderOriginal = 'class AnimationStateMachine { AnimationStateType m_initialStateType; };'
    $episodeHeader = Add-ReferenceAnimationEpisodeHeader $episodeHeaderOriginal
    $episodeHeaderFile = Join-Path $temporary 'Engine/AnimationStateMachine.h'
    $episodeHeaderSnapshot = Join-Path $temporary 'source-snapshot/Engine/AnimationStateMachine.h'
    [IO.File]::WriteAllText($episodeHeaderFile, $episodeHeader)
    [IO.File]::WriteAllText($episodeHeaderSnapshot, $episodeHeaderOriginal)
    $receipt.source_files += [ordered]@{path='Engine/AnimationStateMachine.h';sha256=(Get-FileHash $episodeHeaderSnapshot).Hash.ToLowerInvariant();compiled_sha256=(Get-FileHash $episodeHeaderFile).Hash.ToLowerInvariant()}
    $receipt.runtime_adjustments += 'player-animation-episode-sync-v1'
    Save-Receipt
    & $runner -RuntimeRoot $temporary -ValidateOnly | Out-Null
    $episodeAdjustments = $receipt.runtime_adjustments
    $receipt.runtime_adjustments = @($episodeAdjustments | Where-Object { $_ -cnotin @('player-animation-sync-v1','combat-approach-animation-v1') })
    Save-Receipt
    Expect-Failure 'Animation episodes require player animation synchronization'
    $receipt.runtime_adjustments = $episodeAdjustments
    $episodeRecords = @($receipt.source_files | Where-Object { $_.path -like 'Engine/AnimationStateMachine.*' })
    foreach ($record in $episodeRecords) {
        $episodeFile = Join-Path $temporary $record.path
        $validEpisode = Get-Content -LiteralPath $episodeFile -Raw
        $validEpisodeHash = $record.compiled_sha256
        [IO.File]::AppendAllText($episodeFile,"`nvoid ReplayCompletedAction() {}")
        $record.compiled_sha256 = (Get-FileHash $episodeFile).Hash.ToLowerInvariant()
        Save-Receipt
        $label = if ($record.path.EndsWith('.h')) { 'Animation episode header' } else { 'Player animation' }
        Expect-Failure "$label adjustment differs"
        [IO.File]::WriteAllText($episodeFile,$validEpisode)
        $record.compiled_sha256 = $validEpisodeHash
    }
    $episodeSources = $receipt.source_files
    $receipt.source_files = @($episodeSources | Where-Object { $_.path -cne 'Engine/AnimationStateMachine.h' })
    Save-Receipt
    Expect-Failure 'Animation episodes require exactly one header receipt'
    $receipt.source_files = $episodeSources
    $completeSources = $receipt.source_files
    $receipt.source_files = @($completeSources | Where-Object { $_.path -cne 'Engine/SoundManager.h' })
    Save-Receipt
    Expect-Failure 'Audio settings adjustment requires implementation and header receipts'
    $receipt.source_files = $completeSources
    $preparedScene = Add-ReferenceCraftStatePreparation $compiled
    [IO.File]::WriteAllText($scene, $preparedScene)
    $receipt.source_files[1].compiled_sha256 = (Get-FileHash $scene).Hash.ToLowerInvariant()
    $receipt.runtime_adjustments += 'craft-state-preparation-v1'
    Save-Receipt
    & $runner -RuntimeRoot $temporary -ValidateOnly | Out-Null
    [IO.File]::AppendAllText($scene,"`nvoid WrongCraftTarget() {}")
    $receipt.source_files[1].compiled_sha256 = (Get-FileHash $scene).Hash.ToLowerInvariant()
    Save-Receipt
    Expect-Failure 'Craft preparation adjustment differs'
    $allAdjustments = $receipt.runtime_adjustments
    $receipt.runtime_adjustments = @($allAdjustments | Where-Object { $_ -cne 'collision-debug-toggle-v1' })
    [IO.File]::WriteAllText($scene, (Add-ReferenceCraftStatePreparation $original))
    $receipt.source_files[1].compiled_sha256 = (Get-FileHash $scene).Hash.ToLowerInvariant()
    Save-Receipt
    & $runner -RuntimeRoot $temporary -ValidateOnly | Out-Null
    $allSourceFiles = $receipt.source_files
    $receipt.source_files = @($allSourceFiles | Where-Object { $_.path -cne 'Client/LumiaIsland.cpp' })
    Save-Receipt
    Expect-Failure 'Craft preparation adjustment requires exactly one scene receipt'
    $receipt.source_files = $allSourceFiles
    $receipt.runtime_adjustments = $allAdjustments
    [IO.File]::WriteAllText($scene, $preparedScene)
    $receipt.source_files[1].compiled_sha256 = (Get-FileHash $scene).Hash.ToLowerInvariant()
    $receipt.instrumentation = 'traversal probe'
    Save-Receipt
    Expect-Failure 'Instrumented oracle is not the playable game'
    $receipt.instrumentation = $null
    [IO.File]::WriteAllText($navFile, $navCompiled + "`nvoid ShortcutThroughWall() {}")
    $receipt.source_files[5].compiled_sha256 = (Get-FileHash $navFile -Algorithm SHA256).Hash.ToLowerInvariant()
    Save-Receipt
    Expect-Failure 'Walkable path adjustment differs'
    $receipt.source_files = @($receipt.source_files | Where-Object { $_.path -cne 'Engine/NavMesh.cpp' })
    Save-Receipt
    Expect-Failure 'Walkable path adjustment requires exactly one navigation receipt'
    Write-Output 'PASS: playable runtime revision, unmodified source, sealed executable/shaders, path boundary, and no probe launch'
} finally {
    $resolved = [IO.Path]::GetFullPath($temporary)
    $prefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or
        -not (Split-Path -Leaf $resolved).StartsWith('dxa-reference-game-test-')) { throw 'Temporary cleanup boundary mismatch' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
