[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$builder = Join-Path $RepositoryRoot 'scripts/build_reference_oracle.ps1'
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('dxa-oracle-test-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
function Save([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}
function Invoke-FixtureGit([string[]]$Arguments) {
    $result = & git.exe -C $source @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Fixture git failed: $result" }
    return $result
}
function Expect-Failure([hashtable]$Arguments, [string]$Expected) {
    $message = ''
    try { & $builder @Arguments | Out-Null } catch { $message = $_.Exception.Message }
    if (-not $message.Contains($Expected)) { throw "Expected '$Expected'; received '$message'" }
}
try {
    $source = Join-Path $temporary 'source'
    foreach ($folder in @('Client','Engine','Shaders','Binaries','Libraries/Lib/FMOD','Resources')) {
        New-Item -ItemType Directory -Path (Join-Path $source $folder) -Force | Out-Null
    }
    $wolf = '#include "Wolf.h"' + "`r`nvoid Wolf::Update()`r`n{`r`n    OriginalAttack();`r`n}`r`n"
    $main = 'return make_shared<StartScene>();'
    $lumia = "void LumiaIsland::LateUpdate()`r`n{`r`n    Super::LateUpdate();`r`n}`r`n"
    $lumia += "void LumiaIsland::PrepareCraft()`n{`n" + (@(
        '    m_player->GetPlayerStateMachine()->GetCurrentStatePtr()->SetRecipeIndex(0);',
        '    m_player->GetAnimationStateMachine()->GetCurrentStatePtr()->SetExpectedDuration(3.f);',
        '    m_player->GetPlayerStateMachine()->GetCurrentStatePtr()->SetRecipeIndex(0);',
        '    m_player->GetAnimationStateMachine()->GetCurrentStatePtr()->SetExpectedDuration(1.5f);'
    ) -join "`n") + "`n}`n"
    Save (Join-Path $source 'Client/Wolf.cpp') $wolf
    Save (Join-Path $source 'Client/Main.cpp') $main
    Save (Join-Path $source 'Client/LumiaIsland.cpp') $lumia
    Save (Join-Path $source 'Client/StartScene.cpp') "#include `"StartScene.h`"`r`nvoid StartScene::Update()`r`n{`r`n}`r`n"
    $selection = "#include `"CharacterSelectScene.h`"`r`nvoid CharacterSelectScene::Update()`r`n{`r`n}`r`nvoid CharacterSelectScene::OnCharacterSelectButtonClicked(int charindex)`r`n{`r`n    m_selectCharIdx = charindex - 1;`r`n}`r`nvoid CharacterSelectScene::StartLumiaIsland()`r`n{`r`n    LumiaIslandScene->SetSelectedCharacter(m_selectCharIdx);`r`n}`r`n"
    $selection += "void CharacterSelectScene::UpdateSkinList(shared_ptr<Button> button, int charIndex)`n{`n    button->OnClick += [this, button, i]() {`n        UpdateFullImage(button, i);`n        OnCharacterSelectButtonClicked(i);`n    };`n}`n"
    Save (Join-Path $source 'Client/CharacterSelectScene.cpp') $selection
    Save (Join-Path $source 'Client/Client.vcxproj') 'original client project'
    Save (Join-Path $source 'Engine/Engine.vcxproj') 'original engine project'
    Save (Join-Path $source 'Engine/Graphics.cpp') "#include `"Graphics.h`"`r`nvoid Graphics::RenderEnd()`r`n{`r`n    Present();`r`n}`r`n"
    Save (Join-Path $source 'Engine/Game.cpp') "#include `"Game.h`"`nif (!InitInstance(SW_SHOWNORMAL)) return false;`nvoid Game::Update()`n{`n}`n"
    Save (Join-Path $source 'Engine/SoundManager.cpp') @'
#include "SoundManager.h"
HRESULT SoundManager::Init()
{
    m_System->init(32, FMOD_INIT_NORMAL, nullptr);
    LoadSoundFile();
    return S_OK;
}
void SoundManager::PlaySound(const wstring& _keyname, const int _eID, const float _volume)
{
    m_System->playSound(iter->second, 0, false, &m_Channels[_eID]);
}
void SoundManager::PlayBGM(const wstring& _keyname, const float _volume)
{
    m_System->playSound(iter->second, 0, false, &m_Channels[0]);
}
void SoundManager::SetBGMVolume(float _volume)
{
    m_BGMvolume = _volume;
}
void SoundManager::SetSFXVolume(float _volume)
{
    m_SFXvolume = _volume;
}
void SoundManager::StopAll()
{
}
'@
    Save (Join-Path $source 'Engine/SoundManager.h') 'class SoundManager { FMOD::System* m_System; };'
    Save (Join-Path $source 'Engine/InputManager.cpp') "#include `"InputManager.h`"`nvoid InputManager::Update()`n{`n    ReadDesktopInput();`n}`n"
    Save (Join-Path $source 'Engine/SceneObjectManager.cpp') "void SceneObjectManager::UpdateQuadTree()`n{`n    CacheScreenBounds();`n}`n"
    Save (Join-Path $source 'Engine/AnimationStateMachine.cpp') "#include `"PlayerStateMachine.h`"`nvoid AnimationStateMachine::HandleAutoTransitions()`n{`n}`nbool AnimationStateMachine::CanChangeState(AnimationStateType newState)`n{`n    return false;`n}`n"
    Save (Join-Path $source 'Engine/AnimationStateMachine.h') 'class AnimationStateMachine { AnimationStateType m_initialStateType; };'
    Save (Join-Path $source 'Engine/NavMesh.cpp') "bool NavMesh::IsLineOnNavMesh(const Vec3& start, const Vec3& end, float stepSize)`n{`n    return true;`n}`nbool NavMesh::HasLineOfSight(const Vec3& start, const Vec3& end)`n{`n    return GetDistance(start, end) < 0.1f || IsLineOnNavMesh(start, end);`n}`n"
    Save (Join-Path $source 'Shaders/test.fx') 'original shader'
    Save (Join-Path $source 'GameCoding.sln') 'original solution'
    Save (Join-Path $source 'Resources/model.mesh') 'original resource'
    Save (Join-Path $source 'Binaries/Client.exe') 'fixture executable'
    Save (Join-Path $source 'Libraries/Lib/FMOD/fmod.dll') 'fixture dependency'
    Invoke-FixtureGit @('init','--quiet') | Out-Null
    Invoke-FixtureGit @('config','core.autocrlf','false') | Out-Null
    Invoke-FixtureGit @('add','.') | Out-Null
    Invoke-FixtureGit @('-c','user.name=Oracle fixture','-c','user.email=fixture@example.invalid','commit','--quiet','-m','video revision') | Out-Null
    $revision = [string](Invoke-FixtureGit @('rev-parse','HEAD'))
    Save (Join-Path $source 'Client/Wolf.cpp') 'later behavior tree'
    Save (Join-Path $source 'Libraries/Lib/FMOD/fmod.dll') 'later dependency'
    Invoke-FixtureGit @('add','Client/Wolf.cpp','Libraries/Lib/FMOD/fmod.dll') | Out-Null
    Invoke-FixtureGit @('-c','user.name=Oracle fixture','-c','user.email=fixture@example.invalid','commit','--quiet','-m','later revision') | Out-Null
    $head = [string](Invoke-FixtureGit @('rev-parse','HEAD'))
    $buildStub = Join-Path $temporary 'msbuild-fixture.ps1'
    Save $buildStub '$global:LASTEXITCODE = 0'
    $output = Join-Path $temporary 'oracle'
    $arguments = @{SourceRoot=$source; OutputRoot=$output; SourceRevision=$revision; MSBuild=$buildStub}
    & $builder @arguments | Out-Null
    if ((Get-Content (Join-Path $output 'Client/Wolf.cpp') -Raw) -cne $wolf) { throw 'Compiled HEAD instead of selected revision' }
    if ((Get-Content (Join-Path $output 'Client/Main.cpp') -Raw) -cne $main) { throw 'Uninstrumented build changed initial scene' }
    if ((Get-Content (Join-Path $output 'Binaries/fmod.dll') -Raw) -cne 'fixture dependency') { throw 'Mixed historical code with current libraries' }
    $receipt = Get-Content (Join-Path $output 'oracle-build.json') -Raw | ConvertFrom-Json
    if ($receipt.source_commit -cne $revision -or $receipt.source_logic_modified -or @($receipt.runtime_adjustments).Count -ne 0) { throw 'Incorrect revision receipt' }
    if ([string](Invoke-FixtureGit @('rev-parse','HEAD')) -cne $head -or (Invoke-FixtureGit @('status','--porcelain'))) { throw 'Builder changed source checkout' }
    Expect-Failure $arguments 'already exists'

    $arguments.OutputRoot = Join-Path $temporary 'collision-debug'
    $arguments.CollisionDebugToggle = $true
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    $changed = @($receipt.source_files | Where-Object { $_.sha256 -cne $_.compiled_sha256 })
    if (-not $receipt.source_logic_modified -or @($receipt.runtime_adjustments).Count -ne 1 -or
        $receipt.runtime_adjustments[0] -cne 'collision-debug-toggle-v1' -or $receipt.instrumentation) {
        throw 'Collision debug adjustment was not distinguished from the original and probe builds'
    }
    if ($changed.Count -ne 1 -or $changed[0].path.Replace('\','/') -cne 'Client/LumiaIsland.cpp') {
        throw 'Collision visibility patch changed unrelated source files'
    }
    if ((Get-Content (Join-Path $arguments.OutputRoot 'source-snapshot/Client/LumiaIsland.cpp') -Raw) -cne $lumia) {
        throw 'Collision visibility patch overwrote the original snapshot'
    }
    $arguments.PreparedCraftState = $true
    $arguments.OutputRoot = Join-Path $temporary 'prepared-craft'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    $changed = @($receipt.source_files | Where-Object { $_.sha256 -cne $_.compiled_sha256 })
    if ($receipt.runtime_adjustments -cnotcontains 'craft-state-preparation-v1' -or
        $receipt.runtime_adjustments -cnotcontains 'collision-debug-toggle-v1' -or
        $changed.Count -ne 1 -or $changed[0].path.Replace('\','/') -cne 'Client/LumiaIsland.cpp') {
        throw 'Craft preparation must compose with visibility in the same scene file'
    }
    $arguments.Remove('CollisionDebugToggle')
    $arguments.OutputRoot = Join-Path $temporary 'prepared-craft-only'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    if (@($receipt.runtime_adjustments).Count -ne 1 -or $receipt.runtime_adjustments[0] -cne 'craft-state-preparation-v1') {
        throw 'Craft preparation incorrectly depends on an unrelated visibility adjustment'
    }
    $missingCraftRevision = $arguments.Clone()
    $missingCraftRevision.Remove('SourceRevision')
    $missingCraftRevision.OutputRoot = Join-Path $temporary 'craft-without-revision'
    Expect-Failure $missingCraftRevision 'Runtime adjustments require a source revision snapshot'
    $arguments.Remove('PreparedCraftState')
    $arguments.StableCharacterSelection = $true
    $arguments.OutputRoot = Join-Path $temporary 'stable-selection'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    $changed = @($receipt.source_files | Where-Object { $_.sha256 -cne $_.compiled_sha256 })
    if (-not $receipt.source_logic_modified -or $receipt.runtime_adjustments[0] -cne 'character-selection-index-v1' -or
        $changed.Count -ne 1 -or $changed[0].path.Replace('\','/') -cne 'Client/CharacterSelectScene.cpp') {
        throw 'Stable character selection was not built and recorded as a distinct adjustment'
    }
    $arguments.IndependentSkinSelection = $true
    $arguments.OutputRoot = Join-Path $temporary 'independent-skin'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    $changed = @($receipt.source_files | Where-Object { $_.sha256 -cne $_.compiled_sha256 })
    $skinSource = Get-Content (Join-Path $arguments.OutputRoot 'Client/CharacterSelectScene.cpp') -Raw
    if ($receipt.runtime_adjustments -cnotcontains 'skin-selection-isolation-v1' -or
        $changed.Count -ne 1 -or $changed[0].path.Replace('\','/') -cne 'Client/CharacterSelectScene.cpp' -or
        $skinSource.Contains('OnCharacterSelectButtonClicked(i);') -or -not $skinSource.Contains('UpdateFullImage(button, i);')) {
        throw 'Skin selection must preserve the preview without confirming character/start'
    }
    $arguments.Remove('StableCharacterSelection')
    $arguments.OutputRoot = Join-Path $temporary 'skin-without-base'
    Expect-Failure $arguments 'Independent skin selection requires stable character selection'
    if (Test-Path -LiteralPath $arguments.OutputRoot) { throw 'Missing selection dependency created output' }
    $arguments.Remove('IndependentSkinSelection')

    $arguments.OutputRoot = Join-Path $temporary 'invalid'
    $arguments.SourceRevision = 'not-a-revision'
    Expect-Failure $arguments 'Cannot resolve source revision'
    if (Test-Path $arguments.OutputRoot) { throw 'Invalid revision created output' }

    $arguments.SourceRevision = $revision
    $arguments.OutputRoot = Join-Path $temporary 'probe'
    $arguments.CombatProbe = $true
    & $builder @arguments | Out-Null
    if (-not (Get-Content (Join-Path $arguments.OutputRoot 'Client/Wolf.cpp') -Raw).Contains('ReferenceCombatProbe(')) { throw 'Probe hook missing' }
    if ((Get-Content (Join-Path $arguments.OutputRoot 'Client/Main.cpp') -Raw) -cne $main) { throw 'Probe bypassed original start scene' }
    if (-not (Get-Content (Join-Path $arguments.OutputRoot 'Client/StartScene.cpp') -Raw).Contains('OnStartButtonClicked();')) { throw 'Probe skipped start button handler' }
    if (-not (Get-Content (Join-Path $arguments.OutputRoot 'Client/CharacterSelectScene.cpp') -Raw).Contains('OnCharacterSelectButtonClicked(1);')) { throw 'Probe skipped character selection and resource registration' }
    if (-not (Get-Content (Join-Path $arguments.OutputRoot 'Engine/Graphics.cpp') -Raw).Contains('ReferenceFlushFrame();')) { throw 'Capture must run after scene rendering, before Present' }
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    if (-not $receipt.source_logic_modified -or $receipt.source_commit -cne $revision) { throw 'Probe receipt hides instrumentation' }
    $arguments.OutputRoot = Join-Path $temporary 'patched-probe'
    $arguments.StableCharacterSelection = $true
    $arguments.CollisionDebugToggle = $true
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    if (@($receipt.runtime_adjustments).Count -ne 2 -or -not $receipt.instrumentation) {
        throw 'Combined gameplay adjustments and instrumentation were not recorded separately'
    }
    $arguments.Remove('CombatProbe')
    $arguments.CraftProbe = $true
    $arguments.OutputRoot = Join-Path $temporary 'craft-probe'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    if ($receipt.probe_scenario -cne 'crafting' -or $receipt.probe_window_mode -cne 'hidden' -or $receipt.probe_audio_mode -cne 'nosound' -or
        -not $receipt.source_logic_modified -or -not $receipt.instrumentation) {
        throw 'Craft integration probe did not record its scenario and hidden window'
    }
    if (-not (Get-Content (Join-Path $arguments.OutputRoot 'Client/Wolf.cpp') -Raw).Contains('ReferenceCraftProbe(') -or
        -not (Get-Content (Join-Path $arguments.OutputRoot 'Engine/Game.cpp') -Raw).Contains('InitInstance(SW_HIDE)')) {
        throw 'Craft integration probe was not connected or would display its test window'
    }
    $arguments.Remove('CraftProbe')
    $arguments.NickySkillProbe = $true
    $arguments.ProbeCharacter = 'Nicky'
    $arguments.OutputRoot = Join-Path $temporary 'nicky-skill-probe'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    if ($receipt.probe_scenario -cne 'nicky-skills' -or $receipt.probe_input_mode -cne 'in-process' -or
        -not $receipt.input_probe_sha256) { throw 'Skill probe did not record its isolated input path' }
    if (-not (Get-Content (Join-Path $arguments.OutputRoot 'Engine/InputManager.cpp') -Raw).Contains('ReferenceApplyProbeInput(m_states, m_mousePos);')) {
        throw 'Skill probe did not connect the source input consumers'
    }
    $arguments.SynchronizedPicking = $true
    $arguments.OutputRoot = Join-Path $temporary 'synchronized-picking'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    if ($receipt.runtime_adjustments -cnotcontains 'picking-camera-sync-v1') { throw 'Picking correction was not recorded' }
    $arguments.SynchronizedPlayerAnimation = $true
    $arguments.OutputRoot = Join-Path $temporary 'player-animation'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    if ($receipt.runtime_adjustments -cnotcontains 'player-animation-sync-v1') { throw 'Animation correction was not recorded' }
    $arguments.Remove('NickySkillProbe')
    $arguments.BiancaSkillProbe = $true
    $arguments.ProbeCharacter = 'Bianca'
    $arguments.OutputRoot = Join-Path $temporary 'bianca-skill-probe'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    if ($receipt.probe_scenario -cne 'bianca-skills' -or $receipt.probe_input_mode -cne 'in-process') { throw 'Bianca skill probe was not connected' }

    $arguments.TraversalProbe = $true
    $arguments.OutputRoot = Join-Path $temporary 'mixed-probes'
    Expect-Failure $arguments 'Select one integration probe scenario'
    if (Test-Path -LiteralPath $arguments.OutputRoot) { throw 'Conflicting probes created output' }
    $arguments.Remove('BiancaSkillProbe')
    $arguments.OutputRoot = Join-Path $temporary 'traversal-probe'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    if ($receipt.probe_scenario -cne 'traversal-boss' -or $receipt.probe_input_mode -cne 'in-process' -or
        $receipt.probe_window_mode -cne 'hidden' -or $receipt.probe_audio_mode -cne 'nosound' -or
        -not $receipt.instrumentation_sha256 -or -not $receipt.input_probe_sha256) {
        throw 'Traversal probe did not retain the isolated integration boundary'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $arguments.OutputRoot 'Client/ReferenceTraversalProbe.hpp'))) {
        throw 'Traversal observer was not deployed to its private build'
    }
    $arguments.WalkablePathSmoothing = $true
    $arguments.OutputRoot = Join-Path $temporary 'walkable-path'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    $navFile = @($receipt.source_files | Where-Object { $_.path.Replace('\','/') -ceq 'Engine/NavMesh.cpp' })
    if ($receipt.runtime_adjustments -cnotcontains 'walkable-path-smoothing-v1' -or $navFile.Count -ne 1 -or
        $navFile[0].sha256 -ceq $navFile[0].compiled_sha256) { throw 'Walkable path correction was not applied and recorded' }
    $arguments.SynchronizedCombatAnimation = $true
    $arguments.OutputRoot = Join-Path $temporary 'combat-animation'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    if ($receipt.runtime_adjustments -cnotcontains 'combat-approach-animation-v1' -or
        $receipt.runtime_adjustments -cnotcontains 'player-animation-sync-v1') { throw 'Combat locomotion correction was not composed with the base animation patch' }
    $missingBase = $arguments.Clone()
    $missingBase.SynchronizedPlayerAnimation = $false
    $missingBase.OutputRoot = Join-Path $temporary 'combat-without-base'
    Expect-Failure $missingBase 'Combat animation requires player animation synchronization'
    if (Test-Path -LiteralPath $missingBase.OutputRoot) { throw 'Missing combat patch dependency created output' }
    $arguments.Remove('TraversalProbe')
    $arguments.SynchronizedAnimationEpisodes = $true
    $arguments.OutputRoot = Join-Path $temporary 'animation-episodes'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    $episodeFiles = @($receipt.source_files | Where-Object { $_.path.Replace('\','/') -in @('Engine/AnimationStateMachine.cpp','Engine/AnimationStateMachine.h') })
    if ($receipt.runtime_adjustments -cnotcontains 'player-animation-episode-sync-v1' -or $episodeFiles.Count -ne 2 -or
        @($episodeFiles | Where-Object { $_.sha256 -ceq $_.compiled_sha256 }).Count -ne 0) {
        throw 'Animation episode state must be built and recorded in both implementation and header'
    }
    $missingEpisodeBase = $arguments.Clone()
    $missingEpisodeBase.SynchronizedPlayerAnimation = $false
    $missingEpisodeBase.SynchronizedCombatAnimation = $false
    $missingEpisodeBase.OutputRoot = Join-Path $temporary 'episode-without-base'
    Expect-Failure $missingEpisodeBase 'Animation episodes require player animation synchronization'
    if (Test-Path $missingEpisodeBase.OutputRoot) { throw 'Missing animation episode dependency created output' }
    $arguments.AudioProbe = $true
    $arguments.OutputRoot = Join-Path $temporary 'audio-probe'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    if ($receipt.probe_scenario -cne 'audio-ui' -or $receipt.probe_audio_mode -cne 'offline-pcm' -or
        $receipt.probe_input_mode -cne 'in-process-ui' -or -not $receipt.audio_capture_sha256) { throw 'Audio UI probe did not record its offline capture boundary' }
    $arguments.PersistentAudioSettings = $true
    $arguments.OutputRoot = Join-Path $temporary 'audio-settings'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    $audioFiles = @($receipt.source_files | Where-Object { $_.path.Replace('\','/') -in @('Engine/SoundManager.cpp','Engine/SoundManager.h') })
    if ($receipt.runtime_adjustments -cnotcontains 'audio-category-settings-v1' -or $audioFiles.Count -ne 2 -or
        @($audioFiles | Where-Object { $_.sha256 -ceq $_.compiled_sha256 }).Count -ne 0) { throw 'Audio settings changes were not composed with PCM instrumentation' }

    $arguments.Remove('AudioProbe')
    $arguments.SelectionProbe = $true
    $arguments.IndependentSkinSelection = $true
    $arguments.OutputRoot = Join-Path $temporary 'selection-probe'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    if ($receipt.probe_scenario -cne 'selection-ui' -or $receipt.probe_input_mode -cne 'in-process-ui' -or
        $receipt.probe_window_mode -cne 'hidden' -or $receipt.probe_audio_mode -cne 'nosound' -or
        -not $receipt.instrumentation -or -not $receipt.input_probe_sha256) {
        throw 'Selection probe must declare its isolated UI event path'
    }
    $selectionProbe = Get-Content (Join-Path $arguments.OutputRoot 'Client/CharacterSelectScene.cpp') -Raw
    if (-not $selectionProbe.Contains('ReferenceSelectionUiProbe(') -or $selectionProbe.Contains('OnCharacterSelectButtonClicked(1);')) {
        throw 'Selection probe bypassed the registered character and skin buttons'
    }
    $incompatibleProbe = $arguments.Clone()
    $incompatibleProbe.AudioProbe = $true
    $incompatibleProbe.OutputRoot = Join-Path $temporary 'selection-audio-conflict'
    Expect-Failure $incompatibleProbe 'Select one integration probe scenario'

    $arguments.Remove('SelectionProbe')
    $arguments.NaturalLootProbe = $true
    $arguments.OutputRoot = Join-Path $temporary 'natural-loot-probe'
    & $builder @arguments | Out-Null
    $receipt = Get-Content (Join-Path $arguments.OutputRoot 'oracle-build.json') -Raw | ConvertFrom-Json
    if ($receipt.probe_scenario -cne 'loot-craft' -or $receipt.probe_input_mode -cne 'in-process' -or
        $receipt.probe_window_mode -cne 'hidden' -or $receipt.probe_audio_mode -cne 'nosound' -or
        -not $receipt.instrumentation -or -not $receipt.input_probe_sha256) {
        throw 'Natural loot probe must keep original input consumers isolated from the desktop'
    }
    if (-not (Test-Path (Join-Path $arguments.OutputRoot 'Client/ReferenceLootCraftProbe.hpp'))) {
        throw 'Natural loot observer was not deployed to the private test copy'
    }
    $mixedLootProbe = $arguments.Clone()
    $mixedLootProbe.CraftProbe = $true
    $mixedLootProbe.OutputRoot = Join-Path $temporary 'mixed-loot-probe'
    Expect-Failure $mixedLootProbe 'Select one integration probe scenario'

    Save (Join-Path $source 'Resources/model.mesh') 'different resource'
    Invoke-FixtureGit @('add','Resources/model.mesh') | Out-Null
    Invoke-FixtureGit @('-c','user.name=Oracle fixture','-c','user.email=fixture@example.invalid','commit','--quiet','-m','resource drift') | Out-Null
    $arguments.OutputRoot = Join-Path $temporary 'resource-drift'
    Expect-Failure $arguments 'Resources differ from selected revision'
    if (Test-Path $arguments.OutputRoot) { throw 'Resource mismatch created output' }
    Write-Output 'PASS: revision selection, immutable inputs/outputs, invalid revisions, explicit probe instrumentation, and resource drift'
} finally {
    $resolved = [IO.Path]::GetFullPath($temporary)
    $prefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or
        -not (Split-Path -Leaf $resolved).StartsWith('dxa-oracle-test-')) { throw 'Temporary cleanup boundary mismatch' }
    foreach ($name in @('oracle','probe','collision-debug','prepared-craft','prepared-craft-only','stable-selection','independent-skin','patched-probe','craft-probe','nicky-skill-probe','synchronized-picking','player-animation','bianca-skill-probe','traversal-probe','walkable-path','combat-animation','animation-episodes','audio-probe','audio-settings','selection-probe','natural-loot-probe')) {
        $junction = Join-Path $resolved "$name/Resources"
        if (Test-Path -LiteralPath $junction) { Remove-Item -LiteralPath $junction -Force }
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
