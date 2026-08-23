[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$RepositoryRoot
)

$ErrorActionPreference = 'Stop'
. (Join-Path $RepositoryRoot 'scripts/benchmark_common.ps1')
. (Join-Path $RepositoryRoot 'scripts/simulation_benchmark_common.ps1')

$temporaryRoot = Join-Path (
    [IO.Path]::GetTempPath()) (
    'dxa-simulation-benchmark-' + [Guid]::NewGuid().ToString('N'))
$resolvedTemporaryRoot = [IO.Path]::GetFullPath($temporaryRoot)
$resolvedSystemTemporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
if (-not $resolvedTemporaryRoot.StartsWith(
        $resolvedSystemTemporaryRoot,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw '임시 테스트 경로가 시스템 temp 밖입니다.'
}

New-Item -ItemType Directory -Path $resolvedTemporaryRoot | Out-Null
try {
    & git -C $resolvedTemporaryRoot init --quiet
    & git -C $resolvedTemporaryRoot config user.name 'DXA Test'
    & git -C $resolvedTemporaryRoot config user.email 'dxa-test@example.invalid'
    [IO.File]::WriteAllText(
        (Join-Path $resolvedTemporaryRoot 'tracked.txt'),
        'baseline',
        [Text.UTF8Encoding]::new($false))
    & git -C $resolvedTemporaryRoot add tracked.txt
    & git -C $resolvedTemporaryRoot commit --quiet -m 'test baseline'
    if ($LASTEXITCODE -ne 0) {
        throw '임시 Git 저장소 준비가 실패했습니다.'
    }

    $initial = Get-DxaGitSnapshot -RepositoryRoot $resolvedTemporaryRoot
    Assert-DxaGitSnapshot -Snapshot $initial -ExpectedCommitSha $initial.commit_sha

    [IO.File]::AppendAllText(
        (Join-Path $resolvedTemporaryRoot 'tracked.txt'),
        'dirty',
        [Text.UTF8Encoding]::new($false))
    $dirtyRejected = $false
    try {
        Assert-DxaGitSnapshot `
            -Snapshot (Get-DxaGitSnapshot -RepositoryRoot $resolvedTemporaryRoot) `
            -ExpectedCommitSha $initial.commit_sha
    }
    catch {
        $dirtyRejected = $true
    }
    if (-not $dirtyRejected) {
        throw 'Simulation benchmark dirty tree가 거부되지 않았습니다.'
    }

    & git -C $resolvedTemporaryRoot add tracked.txt
    & git -C $resolvedTemporaryRoot commit --quiet -m 'move head'
    $headRejected = $false
    try {
        Assert-DxaGitSnapshot `
            -Snapshot (Get-DxaGitSnapshot -RepositoryRoot $resolvedTemporaryRoot) `
            -ExpectedCommitSha $initial.commit_sha
    }
    catch {
        $headRejected = $true
    }
    if (-not $headRejected) {
        throw 'Simulation benchmark HEAD 변경이 거부되지 않았습니다.'
    }

    $existingOutput = Join-Path $resolvedTemporaryRoot 'existing-output'
    New-Item -ItemType Directory -Path $existingOutput | Out-Null
    $existingRejected = $false
    try {
        Assert-DxaSimulationOutputDirectoryAvailable -OutputDirectory $existingOutput
    }
    catch {
        $existingRejected = $true
    }
    if (-not $existingRejected) {
        throw '기존 simulation benchmark 출력 경로가 거부되지 않았습니다.'
    }

    $case = [pscustomobject]@{
        median_ms = 1.0
        candidates = 10
        bounds_tested = 10
        evaluations = 10
        checksum = 'abc123'
    }
    $result = [pscustomobject]@{
        schema_version = 1
        seed = 20260823
        commit_sha = $initial.commit_sha
        mismatch_count = 0
        sample_count = 5
        result_checksum = 'result123'
        workload = [pscustomobject]@{
            nav_queries = 100000
            aabb_queries = 20000
            pick_queries = 20000
            ai_decisions = 100000
        }
        cases = [pscustomobject]@{
            nav_linear = $case
            nav_grid = $case
            spatial_linear_aabb = $case
            spatial_quadtree_aabb = $case
            spatial_linear_pick = $case
            spatial_quadtree_pick = $case
            ai_fsm = $case
            ai_behavior_tree = $case
        }
    }
    $validErrors = @(Get-DxaSimulationBenchmarkValidationErrors `
        -Result $result `
        -CommitSha $initial.commit_sha `
        -Seed 20260823)
    if ($validErrors.Count -ne 0) {
        throw "유효한 simulation benchmark 결과가 거부됐습니다: $validErrors"
    }

    $result.mismatch_count = 1
    if (@(Get-DxaSimulationBenchmarkValidationErrors `
            -Result $result `
            -CommitSha $initial.commit_sha `
            -Seed 20260823).Count -eq 0) {
        throw 'Simulation benchmark mismatch가 거부되지 않았습니다.'
    }
    $result.mismatch_count = 0

    if (@(Get-DxaSimulationBenchmarkValidationErrors `
            -Result $result `
            -CommitSha 'wrong-sha' `
            -Seed 20260823).Count -eq 0) {
        throw 'Simulation benchmark commit SHA 불일치가 거부되지 않았습니다.'
    }

    $result.PSObject.Properties.Remove('result_checksum')
    if (@(Get-DxaSimulationBenchmarkValidationErrors `
            -Result $result `
            -CommitSha $initial.commit_sha `
            -Seed 20260823).Count -eq 0) {
        throw 'Simulation benchmark result checksum 누락이 거부되지 않았습니다.'
    }
}
finally {
    if (Test-Path -LiteralPath $resolvedTemporaryRoot) {
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
