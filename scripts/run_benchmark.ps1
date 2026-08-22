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

    [ValidateNotNullOrEmpty()]
    [string]$OutputRoot = 'docs/benchmarks/forward-baseline',

    [AllowEmptyString()]
    [string]$ExpectedAdapter = 'NVIDIA GeForce RTX 3050 Ti Laptop GPU',

    [switch]$Visible
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$originalLocation = Get-Location
$startedAt = [DateTimeOffset]::UtcNow

Set-Location $repositoryRoot
try {
    $status = @(& git status --porcelain=v1 --untracked-files=all)
    if ($LASTEXITCODE -ne 0) {
        throw 'Git 작업 트리 상태를 확인하지 못했습니다.'
    }
    if ($status.Count -ne 0) {
        throw '기준선은 깨끗한 commit에서만 실행할 수 있습니다.'
    }

    $commitSha = @(& git rev-parse HEAD) -join ''
    $commitSha = $commitSha.Trim()
    $shortSha = @(& git rev-parse --short=8 HEAD) -join ''
    $shortSha = $shortSha.Trim()
    $branch = @(& git branch --show-current) -join ''
    $branch = $branch.Trim()
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
    if ($summary.commit_sha -ne $commitSha -or
        [uint32]$summary.seed -ne $Seed -or
        [uint32]$summary.resolution.width -ne $Width -or
        [uint32]$summary.resolution.height -ne $Height -or
        [uint32]$summary.sample_count -ne $MeasuredFrames) {
        throw 'Benchmark 요약이 실행 인자와 일치하지 않습니다.'
    }
    if (-not [string]::IsNullOrWhiteSpace($ExpectedAdapter) -and
        $summary.adapter -ne $ExpectedAdapter) {
        throw "예상 GPU와 실제 GPU가 다릅니다. 예상: $ExpectedAdapter, 실제: $($summary.adapter)"
    }
    if ([uint32]$summary.gpu_missing_samples -ne 0) {
        throw "GPU timestamp가 누락됐습니다: $($summary.gpu_missing_samples)개"
    }

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
        schema_version = 1
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
        video_controllers = $videoControllers
        nvidia_smi = $nvidiaSmi
        benchmark = [ordered]@{
            width = $Width
            height = $Height
            seed = $Seed
            warmup_frames = $WarmupFrames
            measured_frames = $MeasuredFrames
        }
    }
    $environmentPath = Join-Path $outputDirectory 'environment.json'
    $environmentJson = $environment | ConvertTo-Json -Depth 6
    [IO.File]::WriteAllText(
        $environmentPath,
        $environmentJson,
        [Text.UTF8Encoding]::new($false))

    Write-Output "Benchmark 완료: $outputDirectory"
    Write-Output "CPU frame P95: $($summary.metrics.cpu_frame_ms.p95) ms"
    Write-Output "GPU forward P95: $($summary.metrics.gpu_forward_ms.p95) ms"
    Write-Output "Draw calls: $($summary.metrics.draw_calls.p50)"
    Write-Output "Working set P95: $($summary.metrics.working_set_bytes.p95) bytes"
}
finally {
    Set-Location $originalLocation
}
