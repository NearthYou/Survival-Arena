[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot,
      [Parameter(Mandatory)][string]$ReferenceRoot,
      [Parameter(Mandatory)][string]$CMakeExecutable,
      [string]$OutputRoot,
      [switch]$ApplyPatch,
      [string]$SdkRoot,
      [string]$AssetRoot)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$temporaryOutput = [string]::IsNullOrWhiteSpace($OutputRoot)
if ($temporaryOutput) { $OutputRoot = Join-Path ([IO.Path]::GetTempPath()) ('rau-' + [Guid]::NewGuid().ToString('N').Substring(0,8)) }
$output = [IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/')
foreach ($protected in @($RepositoryRoot,$ReferenceRoot)) {
    $protected = (Resolve-Path -LiteralPath $protected).Path.TrimEnd('\','/')
    if ($output.Equals($protected,[StringComparison]::OrdinalIgnoreCase) -or
        $output.StartsWith($protected + '\',[StringComparison]::OrdinalIgnoreCase) -or
        $protected.StartsWith($output + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Audio evidence must remain outside source and worktree' }
}
if (Test-Path -LiteralPath $output) { throw 'Audio evidence output already exists' }
$encoding = [Text.Encoding]::GetEncoding(949)
$codeRoot = if (Test-Path -LiteralPath (Join-Path $ReferenceRoot 'source-snapshot/Engine/SoundManager.cpp')) { Join-Path $ReferenceRoot 'source-snapshot' } else { $ReferenceRoot }
$source = $encoding.GetString([IO.File]::ReadAllBytes((Join-Path $codeRoot 'Engine/SoundManager.cpp')))
$header = $encoding.GetString([IO.File]::ReadAllBytes((Join-Path $codeRoot 'Engine/SoundManager.h')))
if ($ApplyPatch) {
    . (Join-Path $RepositoryRoot 'scripts/reference_game_patches.ps1')
    $source = Add-ReferenceAudioSettings $source
    $header = Add-ReferenceAudioSettingsHeader $header
}
$init = 'm_System->init(32, FMOD_INIT_NORMAL, nullptr);'
if ($source.Split($init,[StringSplitOptions]::None).Count -ne 2) { throw 'Unexpected audio initialization site' }
# Offline software mixing only. Never open or capture a desktop audio endpoint.
$source = $source.Replace($init, "m_System->setOutput(FMOD_OUTPUTTYPE_NOSOUND_NRT);`n    m_System->setSoftwareFormat(48000, FMOD_SPEAKERMODE_STEREO, 0);`n    m_System->setDSPBufferSize(512, 2);`n    " + $init)
$methods = foreach ($name in @('Init','PlaySound','PlayBGM','StopAll','SetBGMVolume','SetSFXVolume')) {
    $match = [regex]::Matches($source, "(?ms)^(HRESULT|void) SoundManager::$name\([^\r\n]*\)\s*\{.*?^\}")
    if ($match.Count -ne 1) { throw "Unexpected sound method: $name" }
    $match[0].Value
}
New-Item -ItemType Directory -Path $output | Out-Null
try {
[IO.File]::WriteAllText((Join-Path $output 'SoundManager.h'),$header,[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $output 'OriginalSoundMethods.inc'),($methods -join "`n"),[Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'tests/reference_audio_settings_fixture.cpp') -Destination (Join-Path $output 'main.cpp')
Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'scripts/reference_oracle/ReferenceAudioCapture.hpp') -Destination (Join-Path $output 'ReferenceAudioCapture.hpp')
if (-not $SdkRoot) { $SdkRoot = Join-Path $ReferenceRoot 'Libraries' }
if (-not $AssetRoot) { $AssetRoot = $ReferenceRoot }
$include = (Join-Path $SdkRoot 'Include').Replace('\','/')
$library = (Join-Path $SdkRoot 'Lib/FMOD/fmod_vc.lib').Replace('\','/')
$project = "cmake_minimum_required(VERSION 3.25)`nproject(ReferenceAudio LANGUAGES CXX)`nadd_executable(audio_settings main.cpp)`ntarget_compile_features(audio_settings PRIVATE cxx_std_17)`ntarget_compile_options(audio_settings PRIVATE /utf-8)`ntarget_include_directories(audio_settings PRIVATE `"$include`")`ntarget_link_libraries(audio_settings PRIVATE `"$library`")`n"
[IO.File]::WriteAllText((Join-Path $output 'CMakeLists.txt'),$project)
$build = Join-Path $output 'build'
& $CMakeExecutable -S $output -B $build -G 'Visual Studio 17 2022' -A x64 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Audio fixture configuration failed' }
$compiled = & $CMakeExecutable --build $build --config Debug --target audio_settings 2>&1
if ($LASTEXITCODE -ne 0) { throw "Audio fixture build failed: $($compiled -join [Environment]::NewLine)" }
$runtimeDll = Join-Path $SdkRoot 'Runtime/fmod.dll'
if (-not (Test-Path -LiteralPath $runtimeDll)) { $runtimeDll = Join-Path $SdkRoot 'Lib/FMOD/fmod.dll' }
Copy-Item -LiteralPath $runtimeDll -Destination (Join-Path $build 'Debug/fmod.dll')
Push-Location -LiteralPath $output
try {
    & (Join-Path $build 'Debug/audio_settings.exe') (Join-Path $AssetRoot 'Resources/Sounds/BSER_AreaBGM_CEMETERY.wav')
    if ($LASTEXITCODE -ne 0) { throw 'Actual FMOD audio settings regression failed; evidence preserved' }
} finally { Pop-Location }
} finally {
    if ($temporaryOutput) {
        $prefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
        if (-not $output.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or
            (Split-Path -Leaf $output) -notmatch '^rau-[0-9a-f]{8}$') { throw 'Audio fixture cleanup boundary mismatch' }
        Remove-Item -LiteralPath $output -Recurse -Force
    }
}
