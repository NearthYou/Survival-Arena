[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('bianca','nicky','loot','combat','traversal','selection','audio','diagnostics')][string]$Scenario,
    [ValidateSet('Bianca','Nicky')][string]$Character = 'Bianca',
    [ValidateSet('1366x768','1920x1080')][string]$Resolution = '1366x768',
    [switch]$TimeoutStart,
    [switch]$CheckCooldowns,
    [switch]$RecordClip,
    [switch]$RecordVideo,
    [ValidateSet('libx264','h264_nvenc')][string]$VideoEncoder = 'libx264',
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateRange(0,86400)][int]$SoakSeconds = 0,
    [Parameter(Mandatory)][string]$SdkRoot,
    [Parameter(Mandatory)][string]$AssetRoot,
    [Parameter(Mandatory)][string]$BuildRoot,
    [string]$Python = 'python',
    [string]$MSBuild = 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe',
    [switch]$Run
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repository = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$build = [IO.Path]::GetFullPath($BuildRoot)
$allowed = Join-Path $repository 'out'
if (-not $build.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Probe outputs must be under this worktree/out'
}
if (($Scenario -eq 'bianca' -and $Character -ne 'Bianca') -or ($Scenario -eq 'nicky' -and $Character -ne 'Nicky')) {
    throw 'Skill scenario and character do not match'
}
$resultFiles = @{ bianca='bianca-skills.json'; nicky='nicky-skills.json'; loot='loot-craft.json'; combat='combat-probe.csv'; traversal='traversal-boss.json'; selection='selection-probe.json'; audio='audio-probe.json'; diagnostics='diagnostics.json' }
$resultPath = Join-Path $build ('runtime/Binaries/' + $resultFiles[$Scenario])
if ($Run -and (Test-Path -LiteralPath $resultPath)) {
    throw 'Existing probe result: choose a fresh BuildRoot to preserve evidence and prevent stale success'
}
& $Python (Join-Path $PSScriptRoot 'game_packages.py') check-output --path $build
if ($LASTEXITCODE -ne 0) { throw 'Probe output path is not independent' }
New-Item -ItemType Directory -Path (Join-Path $build 'include') -Force | Out-Null
$number = if ($Character -eq 'Bianca') { 1 } else { 2 }
$config = "#pragma once`n#define DXA_GAME_PROBE 1`n#define DXA_GAME_PROBE_$($Scenario.ToUpperInvariant()) 1`n#define DXA_GAME_PROBE_CHARACTER $number`n"
if ($RecordVideo) {
    if ($RecordClip -or $SoakSeconds) { throw 'RecordVideo must run separately from GIF or performance capture' }
    $encoder = (Get-Command ffmpeg -ErrorAction Stop).Source.Replace('\','\\')
    $config += "#define DXA_GAME_RECORD_VIDEO 1`n#define DXA_GAME_VIDEO_ENCODER L`"$encoder`"`n#define DXA_GAME_VIDEO_CODEC L`"$VideoEncoder`"`n"
}
if ($RecordClip) {
    if ($Scenario -notin @('selection','nicky')) { throw 'Clip recording supports selection or Nicky skills' }
    $config += "#define DXA_GAME_RECORD_CLIP 1`n"
}
if ($SoakSeconds -gt 0) {
    if ($Scenario -ne 'traversal') { throw 'Soak continues the traversal scenario only' }
    $config += "#define DXA_GAME_SOAK_SECONDS $SoakSeconds`n"
}
if ($TimeoutStart) {
    if ($Scenario -ne 'selection') { throw 'Timeout start is a selection scenario only' }
    $config += "#define DXA_GAME_PROBE_TIMEOUT_START 1`n"
}
if ($CheckCooldowns) {
    if ($Scenario -ne 'nicky') { throw 'Cooldown replay verification is a Nicky scenario only' }
    $config += "#define DXA_GAME_CHECK_NICKY_COOLDOWNS 1`n"
}
$configPath = Join-Path $build 'include/GameProbeConfig.h'
if (-not (Test-Path -LiteralPath $configPath) -or [IO.File]::ReadAllText($configPath) -cne $config) {
    [IO.File]::WriteAllText($configPath, $config, [Text.UTF8Encoding]::new($false))
}
& $Python (Join-Path $PSScriptRoot 'stage_game_runtime.py') --repository $repository --build-root $build --sdk-root $SdkRoot --asset-root $AssetRoot --configuration $Configuration
if ($LASTEXITCODE -ne 0) { throw 'Probe package staging failed' }
foreach ($project in @('Engine','Client')) {
    $projectFile = Join-Path $repository "game/$project/$project.vcxproj"
    & $MSBuild $projectFile /t:Build /m:1 /nodeReuse:false /v:minimal "/p:Configuration=$Configuration" /p:Platform=x64 `
        "/p:DxaGameSdkRoot=$SdkRoot" "/p:DxaGameBuildRoot=$build" "/p:DxaGameProbe=$Scenario" `
        "/p:SolutionDir=$build/" "/flp:logfile=$build/$project-build.log;verbosity=normal" "/bl:$build/$project-build.binlog" *> (Join-Path $build "$project-console.log")
    if ($LASTEXITCODE -ne 0) { throw "$project probe build failed; see $build/$project-console.log" }
}
$profile = "$Scenario-$Character" + $(if ($CheckCooldowns) { '-cooldowns' } else { '' }) + $(if ($SoakSeconds) { '-soak' } else { '' }) + $(if ($RecordClip) { '-clip' } else { '' }) + $(if ($RecordVideo) { '-video' } else { '' })
& $Python (Join-Path $PSScriptRoot 'write_game_build_receipt.py') --repository $repository --build-root $build --sdk-root $SdkRoot --scenario $profile --configuration $Configuration
if ($LASTEXITCODE -ne 0) { throw 'Probe compilation/link provenance failed' }
if ($Run) {
    $binaries = Join-Path $build 'runtime/Binaries'
    $executable = Join-Path $binaries 'dxa_game_probe.exe'
    $arguments = @{ ArgumentList = @("--resolution=$Resolution") }
    if ($SoakSeconds) {
        $arguments.ArgumentList += '--measure-output="' + (Join-Path $build 'frames.csv') + '"'
        $arguments.ArgumentList += "--measure-seconds=$SoakSeconds"
    }
    $windowStyle = if ($SoakSeconds -or $RecordVideo -or $Scenario -eq 'diagnostics') { 'Normal' } else { 'Hidden' }
    $process = Start-Process @arguments -FilePath $executable -WorkingDirectory ([IO.Path]::GetTempPath()) -WindowStyle $windowStyle `
        -RedirectStandardOutput (Join-Path $build 'probe.stdout.log') -RedirectStandardError (Join-Path $build 'probe.stderr.log') -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(240 + $SoakSeconds)
    while (-not $process.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -gt $deadline) {
            Stop-Process -Id $process.Id
            throw 'Internal game probe exceeded its wall-clock deadline'
        }
    }
    if ($Scenario -eq 'combat') {
        $trace = @(Import-Csv -LiteralPath (Join-Path $binaries 'combat-probe.csv'))
        if ($process.ExitCode -ne 0 -or $trace.Count -lt 2 -or
            [int]$trace[-1].wolf_hp -ge [int]$trace[0].wolf_hp) { throw 'Combat trace does not show damage from the requested ordinary attack' }
        [pscustomobject]@{ passed=$true; scenario='combat'; initial_wolf_hp=$trace[0].wolf_hp; final_wolf_hp=$trace[-1].wolf_hp }
        return
    }
    if (-not (Test-Path -LiteralPath $resultPath)) { throw "Probe result missing: $resultPath; process exit $($process.ExitCode)" }
    $result = Get-Content -Raw -LiteralPath $resultPath | ConvertFrom-Json
    $captures = @(Get-Content -LiteralPath (Join-Path $binaries 'capture-status.csv') | ConvertFrom-Csv -Header file,result)
    if (-not $captures.Count) { throw 'Probe produced no captured frames' }
    $dimensions = $Resolution.Split('x')
    $captureEvidence = @(foreach ($capture in $captures) {
        if ($capture.result -ne '0' -or [IO.Path]::GetFileName($capture.file) -cne $capture.file) { throw 'Probe frame capture failed' }
        $path = Join-Path $binaries $capture.file
        $bytes = [IO.File]::ReadAllBytes($path)
        if ($bytes.Length -lt 54 -or $bytes[0] -ne 66 -or $bytes[1] -ne 77 -or
            [BitConverter]::ToInt32($bytes,18) -ne [int]$dimensions[0] -or
            [Math]::Abs([BitConverter]::ToInt32($bytes,22)) -ne [int]$dimensions[1]) { throw "Probe frame has the wrong dimensions: $path" }
        [ordered]@{file=$capture.file; sha256=(Get-FileHash -LiteralPath $path).Hash; width=[int]$dimensions[0]; height=[int]$dimensions[1]}
    })
    [ordered]@{ scenario=$Scenario; character=$Character; resolution=$Resolution; executable_sha256=(Get-FileHash -LiteralPath $executable).Hash; exit_code=$process.ExitCode; result=$result; captures=$captureEvidence } |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build 'probe-run.json') -Encoding utf8
    $result | ConvertTo-Json -Depth 8
    if ($process.ExitCode -ne 0 -or -not $result.passed) { throw "Internal game probe failed: $resultPath; process exit $($process.ExitCode)" }
    if ($RecordVideo) {
        $video = Get-Content -LiteralPath (Join-Path $binaries 'video-capture.json') -Raw | ConvertFrom-Json
        if (-not $video.passed -or $video.frames -lt 30 -or $video.width -ne [int]$dimensions[0] -or $video.height -ne [int]$dimensions[1]) {
            throw 'Requested gameplay video was not recorded completely'
        }
    }
    if ($RecordClip -and @($captureEvidence | Where-Object { $_.file -match '-clip-[0-9]{3}\.bmp$' }).Count -ne 64) {
        throw 'The requested gameplay clip is incomplete'
    }
    if ($SoakSeconds) {
        & $Python (Join-Path $PSScriptRoot 'analyze_game_measurement.py') --input (Join-Path $build 'frames.csv') `
            --output (Join-Path $build 'measurement.json') --minimum-seconds $SoakSeconds --warmup-seconds ([Math]::Min(120, $SoakSeconds / 4))
        if ($LASTEXITCODE -ne 0) { throw 'Soak measurement is incomplete or invalid' }
        $soak = Get-Content -LiteralPath (Join-Path $binaries 'soak-state.json') -Raw | ConvertFrom-Json
        if ($soak.player_hp -le 0 -or $soak.walked -lt 20 -or $soak.q_casts -lt 1 -or $soak.w_casts -lt 1 -or $soak.e_casts -lt 1) {
            throw 'Soak did not retain a living player with movement and accepted Q/W/E casts'
        }
        [ordered]@{ passed=$true; seconds=$SoakSeconds; process_exit=$process.ExitCode; state=$soak; executable_sha256=(Get-FileHash -LiteralPath $executable).Hash } |
            ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $build 'soak-run.json') -Encoding utf8
    }
}
