[CmdletBinding()]
param(
    [uint32]$Seed = 20260823,

    [ValidateNotNullOrEmpty()]
    [string]$OutputRoot = 'docs/benchmarks/offline-match'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$originalLocation = Get-Location
$startedAt = [DateTimeOffset]::UtcNow
. (Join-Path $PSScriptRoot 'benchmark_common.ps1')
. (Join-Path $PSScriptRoot 'offline_match_benchmark_common.ps1')

Set-Location $repositoryRoot
try {
    $initialSnapshot = Get-DxaGitSnapshot -RepositoryRoot $repositoryRoot
    Assert-DxaGitSnapshot `
        -Snapshot $initialSnapshot `
        -ExpectedCommitSha $initialSnapshot.commit_sha
    $commitSha = $initialSnapshot.commit_sha
    $shortSha = $initialSnapshot.short_sha
    if ([string]::IsNullOrWhiteSpace($commitSha) -or
        [string]::IsNullOrWhiteSpace($shortSha)) {
        throw 'Offline match benchmark commit SHA를 읽지 못했습니다.'
    }

    & (Join-Path $PSScriptRoot 'bootstrap.ps1') -Preset windows-msvc-release
    if ($LASTEXITCODE -ne 0) {
        throw 'Offline match benchmark Release configure가 실패했습니다.'
    }
    & (Join-Path $PSScriptRoot 'build.ps1') -Preset windows-msvc-release
    if ($LASTEXITCODE -ne 0) {
        throw 'Offline match benchmark Release build가 실패했습니다.'
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
    Assert-DxaOfflineMatchOutputDirectoryAvailable -OutputDirectory $outputDirectory

    $benchmark = Join-Path $repositoryRoot 'out/build/windows-msvc-vs-release/apps/offline_match_benchmark/Release/dxa_offline_match_benchmark.exe'
    if (-not (Test-Path -LiteralPath $benchmark -PathType Leaf)) {
        throw "Release offline match benchmark를 찾지 못했습니다: $benchmark"
    }

    & $benchmark `
        --output $outputDirectory `
        --commit-sha $commitSha `
        --seed $Seed
    $benchmarkExitCode = $LASTEXITCODE
    $finishedAt = [DateTimeOffset]::UtcNow
    if ($benchmarkExitCode -ne 0) {
        throw "Offline match benchmark가 실패했습니다. 종료 코드: $benchmarkExitCode"
    }

    $resultPath = Join-Path $outputDirectory 'result.json'
    $ticksPath = Join-Path $outputDirectory 'ticks.csv'
    foreach ($requiredPath in @($resultPath, $ticksPath)) {
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "Offline match benchmark 원본이 생성되지 않았습니다: $requiredPath"
        }
    }

    $result = Get-Content -Raw -Encoding utf8 -LiteralPath $resultPath | ConvertFrom-Json
    $validationErrors = @(Get-DxaOfflineMatchBenchmarkValidationErrors `
        -Result $result `
        -CommitSha $commitSha `
        -Seed $Seed)
    $validationErrors += @(Get-DxaOfflineMatchTickValidationErrors `
        -TicksPath $ticksPath `
        -ExpectedFinishedTick ([uint32]$result.finished_tick))

    $operatingSystem = Get-CimInstance Win32_OperatingSystem
    $processor = Get-CimInstance Win32_Processor | Select-Object -First 1
    $environment = [ordered]@{
        schema_version = 1
        run_id = $runId
        started_at = $startedAt.ToString('o')
        finished_at = $finishedAt.ToString('o')
        elapsed_seconds = [Math]::Round(($finishedAt - $startedAt).TotalSeconds, 3)
        git = [ordered]@{
            commit_sha = $commitSha
            branch = $initialSnapshot.branch
            clean_before_run = $true
        }
        build = [ordered]@{
            preset = 'windows-msvc-release'
            configuration = 'Release'
            compiler_id = $result.compiler.id
            compiler_version = $result.compiler.version
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
        validation = [ordered]@{
            status = if ($validationErrors.Count -eq 0) { 'passed' } else { 'failed' }
            errors = $validationErrors
        }
    }
    [IO.File]::WriteAllText(
        (Join-Path $outputDirectory 'environment.json'),
        ($environment | ConvertTo-Json -Depth 6),
        [Text.UTF8Encoding]::new($false))

    if ($validationErrors.Count -ne 0) {
        throw ($validationErrors -join ' ')
    }

    Write-Output "Offline match benchmark 완료: $outputDirectory"
    Write-Output "Finished tick: $($result.finished_tick)"
    Write-Output "Winner: $($result.winner)"
    Write-Output "Tick P95: $($result.tick_ms.p95) ms"
}
finally {
    Set-Location $originalLocation
}
