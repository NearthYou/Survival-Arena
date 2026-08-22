[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$RepositoryRoot
)

$ErrorActionPreference = 'Stop'
. (Join-Path $RepositoryRoot 'scripts/benchmark_common.ps1')

$temporaryRoot = Join-Path (
    [IO.Path]::GetTempPath()) (
    'dxa-benchmark-runner-' + [Guid]::NewGuid().ToString('N'))
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
        'changed',
        [Text.UTF8Encoding]::new($false))
    $changed = Get-DxaGitSnapshot -RepositoryRoot $resolvedTemporaryRoot
    $dirtyRejected = $false
    try {
        Assert-DxaGitSnapshot -Snapshot $changed -ExpectedCommitSha $initial.commit_sha
    }
    catch {
        $dirtyRejected = $true
    }
    if (-not $dirtyRejected) {
        throw 'Build 이후 dirty tree가 거부되지 않았습니다.'
    }

    & git -C $resolvedTemporaryRoot add tracked.txt
    & git -C $resolvedTemporaryRoot commit --quiet -m 'move head'
    if ($LASTEXITCODE -ne 0) {
        throw '임시 Git HEAD 변경이 실패했습니다.'
    }
    $movedHead = Get-DxaGitSnapshot -RepositoryRoot $resolvedTemporaryRoot
    $headMoveRejected = $false
    try {
        Assert-DxaGitSnapshot -Snapshot $movedHead -ExpectedCommitSha $initial.commit_sha
    }
    catch {
        $headMoveRejected = $true
    }
    if (-not $headMoveRejected) {
        throw 'Build 이후 HEAD 변경이 거부되지 않았습니다.'
    }

    $summary = [pscustomobject]@{
        commit_sha = $initial.commit_sha
        seed = 7
        resolution = [pscustomobject]@{ width = 320; height = 180 }
        sample_count = 2
        adapter = 'Expected GPU'
        gpu_missing_samples = 1
    }
    $validationErrors = @(Get-DxaBenchmarkValidationErrors `
        -Summary $summary `
        -CommitSha $initial.commit_sha `
        -Seed 7 `
        -Width 320 `
        -Height 180 `
        -MeasuredFrames 2 `
        -ExpectedAdapter 'Expected GPU')
    if ($validationErrors.Count -ne 1 -or
        $validationErrors[0] -ne 'GPU timestamp가 누락됐습니다: 1개') {
        throw 'GPU 누락 검증 결과가 예상과 다릅니다.'
    }
}
finally {
    if (Test-Path -LiteralPath $resolvedTemporaryRoot) {
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
