[CmdletBinding()]
param(
    [uint32]$Seed = 20260823,

    [ValidateNotNullOrEmpty()]
    [string]$OutputRoot = 'docs/benchmarks/spatial-navigation'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$originalLocation = Get-Location
$startedAt = [DateTimeOffset]::UtcNow
. (Join-Path $PSScriptRoot 'benchmark_common.ps1')
. (Join-Path $PSScriptRoot 'simulation_benchmark_common.ps1')

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
        throw 'Simulation benchmark commit SHA를 읽지 못했습니다.'
    }

    & (Join-Path $PSScriptRoot 'bootstrap.ps1') -Preset windows-msvc-release
    if ($LASTEXITCODE -ne 0) {
        throw 'Simulation benchmark Release configure가 실패했습니다.'
    }
    & (Join-Path $PSScriptRoot 'build.ps1') -Preset windows-msvc-release
    if ($LASTEXITCODE -ne 0) {
        throw 'Simulation benchmark Release build가 실패했습니다.'
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
    Assert-DxaSimulationOutputDirectoryAvailable -OutputDirectory $outputDirectory

    $benchmark = Join-Path $repositoryRoot 'out/build/windows-msvc-vs-release/apps/simulation_benchmark/Release/dxa_simulation_benchmark.exe'
    if (-not (Test-Path -LiteralPath $benchmark -PathType Leaf)) {
        throw "Release simulation benchmark를 찾지 못했습니다: $benchmark"
    }

    & $benchmark `
        --output $outputDirectory `
        --commit-sha $commitSha `
        --seed $Seed
    $benchmarkExitCode = $LASTEXITCODE
    $finishedAt = [DateTimeOffset]::UtcNow
    if ($benchmarkExitCode -ne 0) {
        throw "Simulation benchmark가 실패했습니다. 종료 코드: $benchmarkExitCode"
    }

    $resultPath = Join-Path $outputDirectory 'result.json'
    $samplesPath = Join-Path $outputDirectory 'samples.csv'
    foreach ($requiredPath in @($resultPath, $samplesPath)) {
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "Simulation benchmark 원본이 생성되지 않았습니다: $requiredPath"
        }
    }

    $result = Get-Content -Raw -Encoding utf8 -LiteralPath $resultPath | ConvertFrom-Json
    $validationErrors = @(Get-DxaSimulationBenchmarkValidationErrors `
        -Result $result `
        -CommitSha $commitSha `
        -Seed $Seed)

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

    Write-Output "Simulation benchmark 완료: $outputDirectory"
    Write-Output "Nav linear median: $($result.cases.nav_linear.median_ms) ms"
    Write-Output "Nav grid median: $($result.cases.nav_grid.median_ms) ms"
    Write-Output "Spatial linear AABB median: $($result.cases.spatial_linear_aabb.median_ms) ms"
    Write-Output "Spatial quadtree AABB median: $($result.cases.spatial_quadtree_aabb.median_ms) ms"
    Write-Output "Mismatch count: $($result.mismatch_count)"
}
finally {
    Set-Location $originalLocation
}
