[CmdletBinding()]
param(
    [ValidateRange(1, 16384)]
    [uint32]$Width = 1920,

    [ValidateRange(1, 16384)]
    [uint32]$Height = 1080,

    [uint32]$Seed = 20260823,

    [uint32]$WarmupFrames = 120,

    [ValidateRange(1, [uint32]::MaxValue)]
    [uint32]$MeasuredFrames = 600,

    [ValidateSet('forward', 'hybrid-deferred')]
    [string]$RenderPath = 'forward',

    [ValidateNotNullOrEmpty()]
    [string]$OutputRoot = 'docs/benchmarks/forward-baseline',

    [AllowEmptyString()]
    [string]$ExpectedAdapter = 'NVIDIA GeForce RTX 3050 Ti Laptop GPU',

    [switch]$Visible
)

$ErrorActionPreference = 'Stop'
$outputRootWasProvided = $PSBoundParameters.ContainsKey('OutputRoot')
if (-not $outputRootWasProvided -and $RenderPath -eq 'hybrid-deferred') {
    $OutputRoot = 'docs/benchmarks/hybrid-deferred'
}
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$originalLocation = Get-Location
$startedAt = [DateTimeOffset]::UtcNow
. (Join-Path $PSScriptRoot 'benchmark_common.ps1')

Set-Location $repositoryRoot
try {
    $initialSnapshot = Get-DxaGitSnapshot -RepositoryRoot $repositoryRoot
    Assert-DxaGitSnapshot `
        -Snapshot $initialSnapshot `
        -ExpectedCommitSha $initialSnapshot.commit_sha
    $commitSha = $initialSnapshot.commit_sha
    $shortSha = $initialSnapshot.short_sha
    $branch = $initialSnapshot.branch
    if ([string]::IsNullOrWhiteSpace($commitSha) -or [string]::IsNullOrWhiteSpace($shortSha)) {
        throw '기준선에 기록할 commit SHA를 읽지 못했습니다.'
    }

    & (Join-Path $PSScriptRoot 'bootstrap.ps1') -Preset windows-msvc-release
    if ($LASTEXITCODE -ne 0) {
        throw 'Release configure가 실패했습니다.'
    }
    & (Join-Path $PSScriptRoot 'build.ps1') -Preset windows-msvc-release
    if ($LASTEXITCODE -ne 0) {
        throw 'Release build가 실패했습니다.'
    }

    $preRunSnapshot = Get-DxaGitSnapshot -RepositoryRoot $repositoryRoot
    Assert-DxaGitSnapshot -Snapshot $preRunSnapshot -ExpectedCommitSha $commitSha

    $runId = '{0}-{1}-seed{2}' -f (Get-Date -Format 'yyyyMMdd-HHmmss'), $shortSha, $Seed
    $resolvedOutputRoot = if ([IO.Path]::IsPathRooted($OutputRoot)) {
        [IO.Path]::GetFullPath($OutputRoot)
    }
    else {
        [IO.Path]::GetFullPath((Join-Path $repositoryRoot $OutputRoot))
    }
    $outputDirectory = Join-Path $resolvedOutputRoot $runId
    if (Test-Path -LiteralPath $outputDirectory) {
        throw "실행 디렉터리가 이미 존재합니다: $outputDirectory"
    }

    $client = Join-Path $repositoryRoot 'out/build/windows-msvc-vs-release/apps/client/Release/dxa_client.exe'
    if (-not (Test-Path -LiteralPath $client -PathType Leaf)) {
        throw "Release client를 찾지 못했습니다: $client"
    }

    $clientArguments = @(
        '--no-vsync',
        '--benchmark-output', $outputDirectory,
        '--benchmark-warmup', $WarmupFrames,
        '--benchmark-frames', $MeasuredFrames,
        '--benchmark-seed', $Seed,
        '--commit-sha', $commitSha,
        '--render-path', $RenderPath,
        '--width', $Width,
        '--height', $Height
    )
    if (-not $Visible) {
        $clientArguments = @('--hidden') + $clientArguments
    }

    & $client @clientArguments
    $clientExitCode = $LASTEXITCODE
    $finishedAt = [DateTimeOffset]::UtcNow
    if ($clientExitCode -ne 0) {
        throw "Benchmark client가 실패했습니다. 종료 코드: $clientExitCode"
    }

    $framesPath = Join-Path $outputDirectory 'frames.csv'
    $summaryPath = Join-Path $outputDirectory 'summary.json'
    foreach ($requiredPath in @($framesPath, $summaryPath)) {
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "Benchmark 원본이 생성되지 않았습니다: $requiredPath"
        }
    }

    $summary = Get-Content -Raw -Encoding utf8 -LiteralPath $summaryPath | ConvertFrom-Json
    $validationErrors = @(Get-DxaBenchmarkValidationErrors `
        -Summary $summary `
        -CommitSha $commitSha `
        -Seed $Seed `
        -Width $Width `
        -Height $Height `
        -WarmupFrames $WarmupFrames `
        -MeasuredFrames $MeasuredFrames `
        -ExpectedAdapter $ExpectedAdapter `
        -RenderPath $RenderPath)

    $operatingSystem = Get-CimInstance Win32_OperatingSystem
    $processor = Get-CimInstance Win32_Processor | Select-Object -First 1
    $videoControllers = @(Get-CimInstance Win32_VideoController | ForEach-Object {
        [ordered]@{
            name = $_.Name
            driver_version = $_.DriverVersion
            adapter_ram_bytes = [uint64]$_.AdapterRAM
        }
    })
    $visualStudio = @(& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products * -format json) -join ''
    $visualStudio = $visualStudio | ConvertFrom-Json | Select-Object -First 1
    $nvidiaSmiCommand = Get-Command nvidia-smi -ErrorAction SilentlyContinue
    $nvidiaSmi = if ($null -ne $nvidiaSmiCommand) {
        @(& $nvidiaSmiCommand.Source --query-gpu=name,driver_version,memory.total,pstate --format=csv,noheader) -join "`n"
    }
    else {
        $null
    }

    $environment = [ordered]@{
        schema_version = 2
        run_id = $runId
        started_at = $startedAt.ToString('o')
        finished_at = $finishedAt.ToString('o')
        elapsed_seconds = [Math]::Round(($finishedAt - $startedAt).TotalSeconds, 3)
        git = [ordered]@{
            commit_sha = $commitSha
            branch = $branch
            clean_before_run = $true
        }
        build = [ordered]@{
            preset = 'windows-msvc-release'
            configuration = 'Release'
            visual_studio = $visualStudio.catalog.productDisplayVersion
        }
        operating_system = [ordered]@{
            caption = $operatingSystem.Caption
            version = $operatingSystem.Version
            build_number = $operatingSystem.BuildNumber
        }
        processor = [ordered]@{
            name = $processor.Name
            physical_cores = [uint32]$processor.NumberOfCores
            logical_processors = [uint32]$processor.NumberOfLogicalProcessors
        }
        selected_adapter = $summary.adapter
        render_path = $RenderPath
        video_controllers = $videoControllers
        nvidia_smi = $nvidiaSmi
        validation = [ordered]@{
            status = if ($validationErrors.Count -eq 0) { 'passed' } else { 'failed' }
            errors = $validationErrors
        }
        benchmark = [ordered]@{
            width = $Width
            height = $Height
            seed = $Seed
            warmup_frames = $WarmupFrames
            measured_frames = $MeasuredFrames
            render_path = $RenderPath
        }
    }
    $environmentPath = Join-Path $outputDirectory 'environment.json'
    $environmentJson = $environment | ConvertTo-Json -Depth 6
    [IO.File]::WriteAllText(
        $environmentPath,
        $environmentJson,
        [Text.UTF8Encoding]::new($false))

    if ($validationErrors.Count -ne 0) {
        throw ($validationErrors -join ' ')
    }

    Write-Output "Benchmark 완료: $outputDirectory"
    Write-Output "CPU frame P95: $($summary.metrics.cpu_frame_ms.p95) ms"
    if ($RenderPath -eq 'hybrid-deferred') {
        Write-Output "GPU total P95: $($summary.metrics.gpu_total_ms.p95) ms"
        Write-Output "GPU shadow P95: $($summary.metrics.gpu_shadow_ms.p95) ms"
        Write-Output "GPU G-Buffer P95: $($summary.metrics.gpu_gbuffer_ms.p95) ms"
        Write-Output "GPU lighting P95: $($summary.metrics.gpu_lighting_ms.p95) ms"
        Write-Output "GPU transparent P95: $($summary.metrics.gpu_transparent_ms.p95) ms"
    }
    else {
        Write-Output "GPU forward P95: $($summary.metrics.gpu_forward_ms.p95) ms"
    }
    Write-Output "Draw calls: $($summary.metrics.draw_calls.p50)"
    Write-Output "Working set P95: $($summary.metrics.working_set_bytes.p95) bytes"
}
finally {
    Set-Location $originalLocation
}
