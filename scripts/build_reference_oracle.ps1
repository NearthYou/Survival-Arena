[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SourceRoot,
    [Parameter(Mandatory)][string]$OutputRoot,
    [string]$SourceRevision,
    [switch]$CombatProbe,
    [switch]$CraftProbe,
    [switch]$NaturalLootProbe,
    [switch]$NickySkillProbe,
    [switch]$BiancaSkillProbe,
    [switch]$TraversalProbe,
    [switch]$AudioProbe,
    [switch]$SelectionProbe,
    [switch]$CollisionDebugToggle,
    [switch]$PreparedCraftState,
    [switch]$StableCharacterSelection,
    [switch]$IndependentSkinSelection,
    [switch]$SynchronizedPicking,
    [switch]$SynchronizedPlayerAnimation,
    [switch]$SynchronizedAnimationEpisodes,
    [switch]$WalkablePathSmoothing,
    [switch]$SynchronizedCombatAnimation,
    [switch]$PersistentAudioSettings,
    [ValidateSet('Bianca','Nicky')][string]$ProbeCharacter = 'Bianca',
    [string]$MSBuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'reference_game_patches.ps1')
if (([int][bool]$CombatProbe + [int][bool]$CraftProbe + [int][bool]$NaturalLootProbe + [int][bool]$NickySkillProbe + [int][bool]$BiancaSkillProbe + [int][bool]$TraversalProbe + [int][bool]$AudioProbe + [int][bool]$SelectionProbe) -gt 1) { throw 'Select one integration probe scenario' }
if ($NickySkillProbe -and $ProbeCharacter -cne 'Nicky') { throw 'Nicky skill probe requires the Nicky character' }
if ($BiancaSkillProbe -and $ProbeCharacter -cne 'Bianca') { throw 'Bianca skill probe requires the Bianca character' }
if ($SynchronizedCombatAnimation -and -not $SynchronizedPlayerAnimation) { throw 'Combat animation requires player animation synchronization' }
if ($SynchronizedAnimationEpisodes -and -not $SynchronizedPlayerAnimation) { throw 'Animation episodes require player animation synchronization' }
if ($IndependentSkinSelection -and -not $StableCharacterSelection) { throw 'Independent skin selection requires stable character selection' }
$skillInputProbe = $NickySkillProbe -or $BiancaSkillProbe -or $TraversalProbe -or $AudioProbe -or $SelectionProbe -or $NaturalLootProbe
$runtimeProbe = $CombatProbe -or $CraftProbe -or $skillInputProbe
$probeHeader = if ($NaturalLootProbe) { 'ReferenceLootCraftProbe.hpp' } elseif ($SelectionProbe) { 'ReferenceSelectionProbe.hpp' } elseif ($AudioProbe) { 'ReferenceAudioProbe.hpp' } elseif ($TraversalProbe) { 'ReferenceTraversalProbe.hpp' } elseif ($BiancaSkillProbe) { 'ReferenceBiancaSkillProbe.hpp' } elseif ($NickySkillProbe) { 'ReferenceNickySkillProbe.hpp' } elseif ($CraftProbe) { 'ReferenceCraftProbe.hpp' } else { 'ReferenceCombatProbe.hpp' }
$probeFunction = if ($NaturalLootProbe) { 'ReferenceLootCraftProbe' } elseif ($SelectionProbe) { 'ReferenceSelectionGameProbe' } elseif ($AudioProbe) { 'ReferenceAudioProbe' } elseif ($TraversalProbe) { 'ReferenceTraversalProbe' } elseif ($BiancaSkillProbe) { 'ReferenceBiancaSkillProbe' } elseif ($NickySkillProbe) { 'ReferenceNickySkillProbe' } elseif ($CraftProbe) { 'ReferenceCraftProbe' } else { 'ReferenceCombatProbe' }
if (($CollisionDebugToggle -or $PreparedCraftState -or $StableCharacterSelection -or $SynchronizedPicking -or $SynchronizedPlayerAnimation -or $WalkablePathSmoothing -or $PersistentAudioSettings) -and -not $SourceRevision) { throw 'Runtime adjustments require a source revision snapshot' }
$source = (Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\')
$output = [IO.Path]::GetFullPath($OutputRoot).TrimEnd('\')
$repository = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path.TrimEnd('\')
foreach ($protected in @($source, $repository)) {
    if ($output.Equals($protected, [StringComparison]::OrdinalIgnoreCase) -or
        $output.StartsWith($protected + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $protected.StartsWith($output + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Oracle output must be external to the source and worktree.'
    }
}
if (Test-Path -LiteralPath $output) { throw "Oracle output already exists: $output" }
foreach ($name in @('Client', 'Engine', 'Libraries', 'Shaders', 'Binaries', 'Resources')) {
    if (-not (Test-Path -LiteralPath (Join-Path $source $name) -PathType Container)) {
        throw "Missing source directory: $name"
    }
}
if (-not (Test-Path -LiteralPath $MSBuild -PathType Leaf)) { throw 'MSBuild is unavailable.' }
$sourceCommit = $null
if ($SourceRevision) {
    $sourceCommit = & git -C $source rev-parse --verify --end-of-options ($SourceRevision + '^{commit}') 2>$null
    if ($LASTEXITCODE -ne 0 -or $sourceCommit -notmatch '^[0-9a-f]{40}$') {
        throw "Cannot resolve source revision: $SourceRevision"
    }
    & git -C $source diff --quiet $sourceCommit -- Resources
    if ($LASTEXITCODE -ne 0) { throw 'Resources differ from selected revision; extract that revision before building.' }
}

function Add-CombatProbe([string]$Text) {
    $include = '#include "Wolf.h"'
    if (($Text.Split($include,[StringSplitOptions]::None)).Count -ne 2) { throw 'Unexpected Wolf include site' }
    $Text = $Text.Replace($include, $include + "`r`n" + ('#include "' + $probeHeader + '"'))
    $matches = [regex]::Matches($Text, 'void Wolf::Update\(\)\s*\{')
    if ($matches.Count -ne 1) { throw 'Unexpected Wolf update site' }
    return $Text.Insert($matches[0].Index + $matches[0].Length,
        "`r`n    $probeFunction`(static_pointer_cast<Wolf>(shared_from_this()));")
}
function Add-HiddenProbeWindow([string]$Text) {
    $site = 'InitInstance(SW_SHOWNORMAL)'
    if ($Text.Split($site, [StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected probe window site' }
    return $Text.Replace($site, 'InitInstance(SW_HIDE)')
}
function Add-SilentProbeAudio([string]$Text) {
    $site = 'm_System->init(32, FMOD_INIT_NORMAL, nullptr);'
    if ($Text.Split($site, [StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected probe audio site' }
    $setup = if ($AudioProbe) {
        "m_System->setOutput(FMOD_OUTPUTTYPE_NOSOUND_NRT);`r`n    m_System->setSoftwareFormat(48000, FMOD_SPEAKERMODE_STEREO, 0);`r`n    m_System->setDSPBufferSize(512, 2);"
    } else { 'm_System->setOutput(FMOD_OUTPUTTYPE_NOSOUND);' }
    return $Text.Replace($site, $setup + "`r`n`t" + $site)
}
function Add-AudioCaptureAttachment([string]$Text) {
    $include = '#include "SoundManager.h"'
    $site = 'LoadSoundFile();'
    if ($Text.Split($include,[StringSplitOptions]::None).Count -ne 2 -or
        $Text.Split($site,[StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected audio capture attachment site' }
    $Text = $Text.Replace($include, $include + "`r`n" + '#include "ReferenceAudioCapture.hpp"')
    return $Text.Replace($site, "ReferenceAttachAudioCapture(m_System, &m_loadingComplete);`r`n    " + $site)
}
function Add-AudioCapturePump([string]$Text) {
    $include = '#include "Game.h"'
    if ($Text.Split($include,[StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected audio pump include site' }
    $Text = $Text.Replace($include,$include + "`r`n" + '#include "ReferenceAudioCapture.hpp"')
    $sites = [regex]::Matches($Text,'void Game::Update\(\)\s*\{')
    if ($sites.Count -ne 1) { throw 'Unexpected audio pump update site' }
    return $Text.Insert($sites[0].Index + $sites[0].Length,"`r`n    ReferencePumpAudio();`r`n")
}
function Add-IsolatedProbeInput([string]$Text) {
    $include = '#include "InputManager.h"'
    if ($Text.Split($include, [StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected probe input include site' }
    $Text = $Text.Replace($include, $include + "`r`n" + '#include "ReferenceProbeInput.hpp"')
    $sites = [regex]::Matches($Text, 'void InputManager::Update\(\)\s*\{')
    if ($sites.Count -ne 1) { throw 'Unexpected probe input update site' }
    return $Text.Insert($sites[0].Index + $sites[0].Length,
        "`r`n    ReferenceApplyProbeInput(m_states, m_mousePos);`r`n    return;`r`n")
}
function Add-CombatSceneProbe([string]$Text) {
    $pattern = 'return make_shared<(StartScene|LumiaIsland)>\(\);'
    if ([regex]::Matches($Text, $pattern).Count -ne 1) { throw 'Unexpected initial scene site' }
    return [regex]::Replace($Text, $pattern, 'return make_shared<StartScene>();')
}
function Add-SceneFlowProbe([string]$Text, [string]$Scene) {
    $include = '#include "' + $Scene + '.h"'
    if ($Text.Split($include,[StringSplitOptions]::None).Count -ne 2) { throw "Unexpected $Scene include site" }
    $Text = $Text.Replace($include, $include + "`r`n" + '#include "ReferenceFrameCapture.hpp"')
    if ($AudioProbe -and $Scene -ceq 'StartScene') {
        $Text = $Text.Replace($include, $include + "`r`n" + '#include "ReferenceAudioProbe.hpp"')
    }
    if ($SelectionProbe -and $Scene -ceq 'CharacterSelectScene') {
        $Text = $Text.Replace($include, $include + "`r`n" + '#include "ReferenceSelectionProbe.hpp"')
    }
    $matches = [regex]::Matches($Text, ('void ' + $Scene + '::Update\(\)\s*\{'))
    if ($matches.Count -ne 1) { throw "Unexpected $Scene update site" }
    if ($SelectionProbe -and $Scene -ceq 'CharacterSelectScene') {
        $characterIndex = if ($ProbeCharacter -ceq 'Bianca') { 1 } else { 2 }
        $body = 'ReferenceSelectionUiProbe(m_characterList->GetScrollView(), m_selectedCharacterSkinScrollView->GetScrollView(), m_charSelectPanel->GetButton(L"GameStartButton"), m_selectCharIdx, m_selectElapsedTime, ' + $characterIndex + ');'
        return $Text.Insert($matches[0].Index + $matches[0].Length,"`r`n    " + $body + "`r`n")
    }
    if ($AudioProbe -and $Scene -ceq 'StartScene') {
        $body = @'
    static bool audioPanelOpened = false;
    if (!audioPanelOpened) { EnableSoundPanel(); audioPanelOpened = true; }
    ReferenceLobbyAudioProbe(m_BGMSlider, m_SFXSlider, [this]() { OnButtonHover(); }, [this]() { OnStartButtonClicked(); });
'@
        return $Text.Insert($matches[0].Index + $matches[0].Length,"`r`n" + $body + "`r`n")
    }
    $action = if ($Scene -ceq 'StartScene') {
        'ReferenceQueueFrame(L"oracle-start.bmp"); OnStartButtonClicked();'
    } else {
        $characterIndex = if ($ProbeCharacter -ceq 'Bianca') { 1 } else { 2 }
        'ReferenceQueueFrame(L"oracle-selection.bmp"); OnCharacterSelectButtonClicked(' + $characterIndex + ');'
    }
    return $Text.Insert($matches[0].Index + $matches[0].Length,
        "`r`n    static int oracleFrames = 0;`r`n    if (++oracleFrames == 30) { $action }`r`n")
}
function Add-CaptureFlush([string]$Text) {
    $include = '#include "Graphics.h"'
    if ($Text.Split($include,[StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected Graphics include site' }
    $Text = $Text.Replace($include, $include + "`r`n" + '#include "ReferenceFrameCapture.hpp"')
    $matches = [regex]::Matches($Text, 'void Graphics::RenderEnd\(\)\s*\{')
    if ($matches.Count -ne 1) { throw 'Unexpected Graphics render-end site' }
    return $Text.Insert($matches[0].Index + $matches[0].Length, "`r`n    ReferenceFlushFrame();")
}
$byteEncoding = [Text.Encoding]::GetEncoding(28591) # Byte-preserving insertion into original CP949 source.

# A private source copy keeps the reference project's pre-build header export,
# compiler outputs and runtime relative paths away from the read-only original.
New-Item -ItemType Directory -Path $output | Out-Null
$codeSource = $source
if ($sourceCommit) {
    $archive = Join-Path $output 'source-snapshot.zip'
    & git -C $source archive --format=zip "--output=$archive" $sourceCommit Client Engine Shaders Libraries GameCoding.sln
    if ($LASTEXITCODE -ne 0) { throw 'Cannot archive selected source revision.' }
    $codeSource = Join-Path $output 'source-snapshot'
    Expand-Archive -LiteralPath $archive -DestinationPath $codeSource
}
foreach ($name in @('Client', 'Engine', 'Shaders', 'Libraries')) {
    Copy-Item -LiteralPath (Join-Path $codeSource $name) -Destination (Join-Path $output $name) -Recurse
}
foreach ($name in @('Binaries')) {
    Copy-Item -LiteralPath (Join-Path $source $name) -Destination (Join-Path $output $name) -Recurse
}
Copy-Item -LiteralPath (Join-Path $codeSource 'GameCoding.sln') -Destination $output
New-Item -ItemType Junction -Path (Join-Path $output 'Resources') `
    -Target (Join-Path $source 'Resources') | Out-Null
foreach ($file in @(Get-ChildItem -LiteralPath (Join-Path $output 'Libraries/Lib/FMOD') -Filter '*.dll')) {
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $output 'Binaries')
}
if ($CollisionDebugToggle -or $PreparedCraftState) {
    $scenePath = Join-Path $output 'Client/LumiaIsland.cpp'
    $sceneText = $byteEncoding.GetString([IO.File]::ReadAllBytes($scenePath))
    if ($CollisionDebugToggle) { $sceneText = Add-ReferenceCollisionDebugToggle $sceneText }
    if ($PreparedCraftState) { $sceneText = Add-ReferenceCraftStatePreparation $sceneText }
    [IO.File]::WriteAllBytes($scenePath, $byteEncoding.GetBytes($sceneText))
}
if ($StableCharacterSelection) {
    $scenePath = Join-Path $output 'Client/CharacterSelectScene.cpp'
    $sceneText = $byteEncoding.GetString([IO.File]::ReadAllBytes($scenePath))
    $sceneText = Add-ReferenceStableCharacterSelection $sceneText
    if ($IndependentSkinSelection) { $sceneText = Add-ReferenceIndependentSkinSelection $sceneText }
    [IO.File]::WriteAllBytes($scenePath, $byteEncoding.GetBytes($sceneText))
}
if ($SynchronizedPicking) {
    $managerPath = Join-Path $output 'Engine/SceneObjectManager.cpp'
    $managerText = $byteEncoding.GetString([IO.File]::ReadAllBytes($managerPath))
    [IO.File]::WriteAllBytes($managerPath, $byteEncoding.GetBytes((Add-ReferencePickingMatrixSync $managerText)))
}
if ($SynchronizedPlayerAnimation) {
    $animationPath = Join-Path $output 'Engine/AnimationStateMachine.cpp'
    $animationText = $byteEncoding.GetString([IO.File]::ReadAllBytes($animationPath))
    $animationText = Add-ReferencePlayerAnimationSync $animationText
    if ($SynchronizedCombatAnimation) { $animationText = Add-ReferenceCombatAnimationSync $animationText }
    if ($SynchronizedAnimationEpisodes) { $animationText = Add-ReferenceAnimationEpisodeSync $animationText }
    [IO.File]::WriteAllBytes($animationPath, $byteEncoding.GetBytes($animationText))
}
if ($SynchronizedAnimationEpisodes) {
    $animationHeaderPath = Join-Path $output 'Engine/AnimationStateMachine.h'
    $animationHeader = $byteEncoding.GetString([IO.File]::ReadAllBytes($animationHeaderPath))
    [IO.File]::WriteAllBytes($animationHeaderPath, $byteEncoding.GetBytes((Add-ReferenceAnimationEpisodeHeader $animationHeader)))
}
if ($WalkablePathSmoothing) {
    $navPath = Join-Path $output 'Engine/NavMesh.cpp'
    $navText = $byteEncoding.GetString([IO.File]::ReadAllBytes($navPath))
    [IO.File]::WriteAllBytes($navPath, $byteEncoding.GetBytes((Add-ReferenceWalkablePathSmoothing $navText)))
}
if ($PersistentAudioSettings) {
    foreach ($extension in @('cpp','h')) {
        $audioPath = Join-Path $output "Engine/SoundManager.$extension"
        $audioText = $byteEncoding.GetString([IO.File]::ReadAllBytes($audioPath))
        $audioText = if ($extension -ceq 'cpp') { Add-ReferenceAudioSettings $audioText } else { Add-ReferenceAudioSettingsHeader $audioText }
        [IO.File]::WriteAllBytes($audioPath,$byteEncoding.GetBytes($audioText))
    }
}
if ($runtimeProbe) {
    foreach ($header in @($probeHeader,'ReferenceFrameCapture.hpp')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot "reference_oracle/$header") `
            -Destination (Join-Path $output "Client/$header")
    }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'reference_oracle/ReferenceFrameCapture.hpp') `
        -Destination (Join-Path $output 'Engine/ReferenceFrameCapture.hpp')
    if ($AudioProbe) {
        foreach ($folder in @('Client','Engine')) {
            Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'reference_oracle/ReferenceAudioCapture.hpp') -Destination (Join-Path $output "$folder/ReferenceAudioCapture.hpp")
        }
    }
    if ($skillInputProbe) {
        foreach ($folder in @('Client','Engine')) {
            Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'reference_oracle/ReferenceProbeInput.hpp') `
                -Destination (Join-Path $output "$folder/ReferenceProbeInput.hpp")
        }
        $inputPath = Join-Path $output 'Engine/InputManager.cpp'
        $inputText = $byteEncoding.GetString([IO.File]::ReadAllBytes($inputPath))
        [IO.File]::WriteAllBytes($inputPath, $byteEncoding.GetBytes((Add-IsolatedProbeInput $inputText)))
    }
    $gamePath = Join-Path $output 'Engine/Game.cpp'
    $gameText = $byteEncoding.GetString([IO.File]::ReadAllBytes($gamePath))
    $gameText = Add-HiddenProbeWindow $gameText
    if ($AudioProbe) { $gameText = Add-AudioCapturePump $gameText }
    [IO.File]::WriteAllBytes($gamePath, $byteEncoding.GetBytes($gameText))
    $soundPath = Join-Path $output 'Engine/SoundManager.cpp'
    $soundText = $byteEncoding.GetString([IO.File]::ReadAllBytes($soundPath))
    $soundText = Add-SilentProbeAudio $soundText
    if ($AudioProbe) { $soundText = Add-AudioCaptureAttachment $soundText }
    [IO.File]::WriteAllBytes($soundPath, $byteEncoding.GetBytes($soundText))
    $graphicsPath = Join-Path $output 'Engine/Graphics.cpp'
    $graphicsText = $byteEncoding.GetString([IO.File]::ReadAllBytes($graphicsPath))
    [IO.File]::WriteAllBytes($graphicsPath, $byteEncoding.GetBytes((Add-CaptureFlush $graphicsText)))
    $wolfPath = Join-Path $output 'Client/Wolf.cpp'
    $wolfText = $byteEncoding.GetString([IO.File]::ReadAllBytes($wolfPath))
    [IO.File]::WriteAllBytes($wolfPath, $byteEncoding.GetBytes((Add-CombatProbe $wolfText)))
    $mainPath = Join-Path $output 'Client/Main.cpp'
    $mainText = $byteEncoding.GetString([IO.File]::ReadAllBytes($mainPath))
    [IO.File]::WriteAllBytes($mainPath, $byteEncoding.GetBytes((Add-CombatSceneProbe $mainText)))
    foreach ($scene in @('StartScene','CharacterSelectScene')) {
        $scenePath = Join-Path $output "Client/$scene.cpp"
        $sceneText = $byteEncoding.GetString([IO.File]::ReadAllBytes($scenePath))
        [IO.File]::WriteAllBytes($scenePath, $byteEncoding.GetBytes((Add-SceneFlowProbe $sceneText $scene)))
    }
}

foreach ($project in @('Engine', 'Client')) {
    $projectPath = Join-Path $output "$project/$project.vcxproj"
    $intermediate = Join-Path $output "Intermediate/$project/"
    & $MSBuild $projectPath /t:Build /m:1 /nodeReuse:false /v:minimal `
        /p:Configuration=Debug /p:Platform=x64 "/p:SolutionDir=$output\" `
        "/p:IntDir=$intermediate" /p:LanguageStandard=stdcpp17 `
        "/flp:logfile=$output\$project-build.log;verbosity=normal"
    if ($LASTEXITCODE -ne 0) { throw "$project oracle build failed; see $output\$project-build.log" }
}
$sourceFiles = @(foreach ($folder in @('Client', 'Engine', 'Shaders')) {
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $codeSource $folder) -File -Recurse) {
        $relative = [IO.Path]::GetRelativePath($codeSource, $file.FullName)
        $sourceHash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        $copyHash = (Get-FileHash -LiteralPath (Join-Path $output $relative) -Algorithm SHA256).Hash
        $expectedHash = $sourceHash
        $expectedText = $null
        if (($CollisionDebugToggle -or $PreparedCraftState) -and $relative.Replace('\','/') -ceq 'Client/LumiaIsland.cpp') {
            $expectedText = $byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName))
            if ($CollisionDebugToggle) { $expectedText = Add-ReferenceCollisionDebugToggle $expectedText }
            if ($PreparedCraftState) { $expectedText = Add-ReferenceCraftStatePreparation $expectedText }
        }
        if ($StableCharacterSelection -and $relative.Replace('\','/') -ceq 'Client/CharacterSelectScene.cpp') {
            $expectedText = Add-ReferenceStableCharacterSelection ($byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)))
            if ($IndependentSkinSelection) { $expectedText = Add-ReferenceIndependentSkinSelection $expectedText }
        }
        if ($SynchronizedPicking -and $relative.Replace('\','/') -ceq 'Engine/SceneObjectManager.cpp') {
            $expectedText = Add-ReferencePickingMatrixSync ($byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)))
        }
        if ($SynchronizedPlayerAnimation -and $relative.Replace('\','/') -ceq 'Engine/AnimationStateMachine.cpp') {
            $expectedText = Add-ReferencePlayerAnimationSync ($byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)))
            if ($SynchronizedCombatAnimation) { $expectedText = Add-ReferenceCombatAnimationSync $expectedText }
            if ($SynchronizedAnimationEpisodes) { $expectedText = Add-ReferenceAnimationEpisodeSync $expectedText }
        }
        if ($SynchronizedAnimationEpisodes -and $relative.Replace('\','/') -ceq 'Engine/AnimationStateMachine.h') {
            $expectedText = Add-ReferenceAnimationEpisodeHeader ($byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)))
        }
        if ($WalkablePathSmoothing -and $relative.Replace('\','/') -ceq 'Engine/NavMesh.cpp') {
            $expectedText = Add-ReferenceWalkablePathSmoothing ($byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)))
        }
        if ($PersistentAudioSettings -and $relative.Replace('\','/') -ceq 'Engine/SoundManager.cpp') {
            $expectedText = Add-ReferenceAudioSettings ($byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)))
        }
        if ($PersistentAudioSettings -and $relative.Replace('\','/') -ceq 'Engine/SoundManager.h') {
            $expectedText = Add-ReferenceAudioSettingsHeader ($byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)))
        }
        if ($runtimeProbe -and $relative.Replace('\','/') -ceq 'Client/Wolf.cpp') {
            $expectedText = Add-CombatProbe ($byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)))
        }
        if ($runtimeProbe -and $relative.Replace('\','/') -ceq 'Client/Main.cpp') {
            $expectedText = Add-CombatSceneProbe ($byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)))
        }
        foreach ($scene in @('StartScene','CharacterSelectScene')) {
            if ($runtimeProbe -and $relative.Replace('\','/') -ceq "Client/$scene.cpp") {
                if ($null -eq $expectedText) { $expectedText = $byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)) }
                $expectedText = Add-SceneFlowProbe $expectedText $scene
            }
        }
        if ($runtimeProbe -and $relative.Replace('\','/') -ceq 'Engine/Graphics.cpp') {
            $expectedText = Add-CaptureFlush ($byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)))
        }
        if ($runtimeProbe -and $relative.Replace('\','/') -ceq 'Engine/Game.cpp') {
            $expectedText = Add-HiddenProbeWindow ($byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)))
            if ($AudioProbe) { $expectedText = Add-AudioCapturePump $expectedText }
        }
        if ($runtimeProbe -and $relative.Replace('\','/') -ceq 'Engine/SoundManager.cpp') {
            if ($null -eq $expectedText) { $expectedText = $byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)) }
            $expectedText = Add-SilentProbeAudio $expectedText
            if ($AudioProbe) { $expectedText = Add-AudioCaptureAttachment $expectedText }
        }
        if ($skillInputProbe -and $relative.Replace('\','/') -ceq 'Engine/InputManager.cpp') {
            $expectedText = Add-IsolatedProbeInput ($byteEncoding.GetString([IO.File]::ReadAllBytes($file.FullName)))
        }
        if ($null -ne $expectedText) {
            $expectedHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($byteEncoding.GetBytes($expectedText)))
        }
        if ($expectedHash -cne $copyHash) { throw "Oracle source copy differs: $relative" }
        [ordered]@{ path=$relative; sha256=$sourceHash.ToLowerInvariant(); compiled_sha256=$copyHash.ToLowerInvariant() }
    }
})
$executable = Join-Path $output 'Binaries/Client.exe'
$receipt = [ordered]@{
    source_commit = $sourceCommit
    source_files = $sourceFiles
    configuration = 'Debug|x64'
    executable_sha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()
    runtime_files = @(Get-ChildItem -LiteralPath (Join-Path $output 'Binaries') -File | Where-Object {
        $_.Extension -in @('.exe','.dll')
    } | Sort-Object Name | ForEach-Object {
        [ordered]@{path=('Binaries/' + $_.Name);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
    })
    working_directory = 'Binaries'
    source_logic_modified = [bool]($runtimeProbe -or $CollisionDebugToggle -or $PreparedCraftState -or $StableCharacterSelection -or $SynchronizedPicking -or $SynchronizedPlayerAnimation -or $WalkablePathSmoothing -or $PersistentAudioSettings)
    runtime_adjustments = @(
        if ($CollisionDebugToggle) { 'collision-debug-toggle-v1' }
        if ($PreparedCraftState) { 'craft-state-preparation-v1' }
        if ($StableCharacterSelection) { 'character-selection-index-v1' }
        if ($IndependentSkinSelection) { 'skin-selection-isolation-v1' }
        if ($SynchronizedPicking) { 'picking-camera-sync-v1' }
        if ($SynchronizedPlayerAnimation) { 'player-animation-sync-v1' }
        if ($SynchronizedAnimationEpisodes) { 'player-animation-episode-sync-v1' }
        if ($WalkablePathSmoothing) { 'walkable-path-smoothing-v1' }
        if ($SynchronizedCombatAnimation) { 'combat-approach-animation-v1' }
        if ($PersistentAudioSettings) { 'audio-category-settings-v1' }
    )
    instrumentation = $(if ($SelectionProbe) {'Hidden silent integration test: invoke registered character and skin buttons, observe twelve seconds without quick start, then invoke the explicit start button and verify the actual gameplay actor; no desktop input or stat grants'} elseif ($AudioProbe) {'Hidden offline PCM integration test: original UI slider delegates mute and restore hover sounds, then retain mute through original selection/countdown into gameplay; no desktop input or audio endpoint'} elseif ($TraversalProbe) {'Hidden silent integration test: camera-projected in-process right-clicks navigate the original NavMesh, fight six wolves, earn progression and fight Alpha; record positions, live health, boss phases and GPU frames; no desktop input, teleports or stat grants'} elseif ($BiancaSkillProbe) {'Hidden silent integration test: in-process input learns Q, earns a kill and skill points, exercises W coffin/defense, E charge/dash/hit and both living-health R phases; no desktop input or stat grants'} elseif ($NickySkillProbe) {'Hidden silent integration test: in-process key states and camera-projected cursor coordinates feed original input consumers; learn skills through earned points, exercise R rush, W incoming-hit counter, E punch and Q charging movement/release; no desktop input or stat grants'} elseif ($CraftProbe) {'Hidden silent integration test: seed catalog sports shoes and steel ball, reject missing ingredients, invoke original craft delegates/states, observe item conversion and animation, equip result; no desktop input'} elseif ($CombatProbe) {'Hidden silent window; invoke original start and character-select button handlers, retain original countdown/loading, request one attack after 3 seconds, record HP and GPU frames; no desktop input'} else {$null})
    probe_character = $(if ($runtimeProbe) {$ProbeCharacter} else {$null})
    probe_scenario = $(if ($NaturalLootProbe) {'loot-craft'} elseif ($SelectionProbe) {'selection-ui'} elseif ($AudioProbe) {'audio-ui'} elseif ($TraversalProbe) {'traversal-boss'} elseif ($BiancaSkillProbe) {'bianca-skills'} elseif ($NickySkillProbe) {'nicky-skills'} elseif ($CraftProbe) {'crafting'} elseif ($CombatProbe) {'combat'} else {$null})
    probe_input_mode = $(if ($AudioProbe -or $SelectionProbe) {'in-process-ui'} elseif ($skillInputProbe) {'in-process'} else {$null})
    input_probe_sha256 = $(if ($skillInputProbe) {(Get-FileHash -LiteralPath (Join-Path $output 'Engine/ReferenceProbeInput.hpp') -Algorithm SHA256).Hash.ToLowerInvariant()} else {$null})
    probe_window_mode = $(if ($runtimeProbe) {'hidden'} else {$null})
    probe_audio_mode = $(if ($AudioProbe) {'offline-pcm'} elseif ($runtimeProbe) {'nosound'} else {$null})
    audio_capture_sha256 = $(if ($AudioProbe) {(Get-FileHash -LiteralPath (Join-Path $output 'Engine/ReferenceAudioCapture.hpp') -Algorithm SHA256).Hash.ToLowerInvariant()} else {$null})
    instrumentation_sha256 = $(if ($runtimeProbe) {(Get-FileHash -LiteralPath (Join-Path $output "Client/$probeHeader") -Algorithm SHA256).Hash.ToLowerInvariant()} else {$null})
    capture_sha256 = $(if ($runtimeProbe) {(Get-FileHash -LiteralPath (Join-Path $output 'Client/ReferenceFrameCapture.hpp') -Algorithm SHA256).Hash.ToLowerInvariant()} else {$null})
    visual_comparison_approved = $false
}
if ($NaturalLootProbe) {
    $receipt.instrumentation = 'Hidden silent integration test: source wolf kills and corpse loot, UI mouse clicks, Z crafting and UI equipment; in-process input only, no items, stats, damage or movement injected'
}
[IO.File]::WriteAllText((Join-Path $output 'oracle-build.json'),
    ($receipt | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
Get-FileHash -LiteralPath $executable -Algorithm SHA256
