[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$RepositoryRoot
)

$ErrorActionPreference = 'Stop'
. (Join-Path $RepositoryRoot 'scripts/benchmark_common.ps1')
. (Join-Path $RepositoryRoot 'scripts/offline_match_benchmark_common.ps1')

$temporaryRoot = Join-Path (
    [IO.Path]::GetTempPath()) (
    'dxa-offline-match-benchmark-' + [Guid]::NewGuid().ToString('N'))
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
        throw 'Offline match benchmark dirty tree가 거부되지 않았습니다.'
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
        throw 'Offline match benchmark HEAD 변경이 거부되지 않았습니다.'
    }

    $existingOutput = Join-Path $resolvedTemporaryRoot 'existing-output'
    New-Item -ItemType Directory -Path $existingOutput | Out-Null
    $existingRejected = $false
    try {
        Assert-DxaOfflineMatchOutputDirectoryAvailable -OutputDirectory $existingOutput
    }
    catch {
        $existingRejected = $true
    }
    if (-not $existingRejected) {
        throw '기존 offline match benchmark 출력 경로가 거부되지 않았습니다.'
    }

    $result = [pscustomobject]@{
        schema_version = 1
        commit_sha = $initial.commit_sha
        seed = 20260823
        winner = 2
        end_reason = 'last_survivor'
        finished_tick = 16147
        event_checksum = '17222440337191440965'
        repeat_mismatch_count = 0
        tick_ms = [pscustomobject]@{ p50 = 0.02; p95 = 0.08; max = 0.5 }
        population = [pscustomobject]@{ contenders = 24; neutrals = 100 }
        compiler = [pscustomobject]@{ id = 'MSVC'; version = 'test' }
        cpu = [pscustomobject]@{ logical_processors = 8 }
    }
    $validErrors = @(Get-DxaOfflineMatchBenchmarkValidationErrors `
        -Result $result `
        -CommitSha $initial.commit_sha `
        -Seed 20260823)
    if ($validErrors.Count -ne 0) {
        throw "유효한 offline match benchmark 결과가 거부됐습니다: $validErrors"
    }

    $ticksPath = Join-Path $resolvedTemporaryRoot 'ticks.csv'
    @(
        [pscustomobject]@{ tick = 1; elapsed_ms = 0.01; alive_contenders = 24; alive_neutrals = 100; event_count = 0 }
        [pscustomobject]@{ tick = 2; elapsed_ms = 0.02; alive_contenders = 24; alive_neutrals = 100; event_count = 1 }
        [pscustomobject]@{ tick = 3; elapsed_ms = 0.03; alive_contenders = 23; alive_neutrals = 99; event_count = 2 }
    ) | Export-Csv -LiteralPath $ticksPath -NoTypeInformation -Encoding utf8
    $tickErrors = @(Get-DxaOfflineMatchTickValidationErrors `
        -TicksPath $ticksPath `
        -ExpectedFinishedTick 3)
    if ($tickErrors.Count -ne 0) {
        throw "유효한 offline match tick CSV가 거부됐습니다: $tickErrors"
    }

    @(
        [pscustomobject]@{ tick = 1; elapsed_ms = 0.01 }
        [pscustomobject]@{ tick = 2; elapsed_ms = 0.02 }
        [pscustomobject]@{ tick = 3; elapsed_ms = 0.03 }
    ) | Export-Csv -LiteralPath $ticksPath -NoTypeInformation -Encoding utf8
    if (@(Get-DxaOfflineMatchTickValidationErrors `
            -TicksPath $ticksPath `
            -ExpectedFinishedTick 3).Count -eq 0) {
        throw 'Offline match benchmark 필수 tick 열 누락이 거부되지 않았습니다.'
    }

    @(
        [pscustomobject]@{ tick = 1; elapsed_ms = 0.01; alive_contenders = 24; alive_neutrals = 100; event_count = $null }
        [pscustomobject]@{ tick = 2; elapsed_ms = 0.02; alive_contenders = 24; alive_neutrals = 100; event_count = 1 }
        [pscustomobject]@{ tick = 3; elapsed_ms = 0.03; alive_contenders = 23; alive_neutrals = 99; event_count = 2 }
    ) | Export-Csv -LiteralPath $ticksPath -NoTypeInformation -Encoding utf8
    if (@(Get-DxaOfflineMatchTickValidationErrors `
            -TicksPath $ticksPath `
            -ExpectedFinishedTick 3).Count -eq 0) {
        throw 'Offline match benchmark 빈 tick 값이 거부되지 않았습니다.'
    }

    if (@(Get-DxaOfflineMatchBenchmarkValidationErrors `
            -Result $result `
            -CommitSha 'wrong-sha' `
            -Seed 20260823).Count -eq 0) {
        throw 'Offline match benchmark SHA 불일치가 거부되지 않았습니다.'
    }
    $result.repeat_mismatch_count = 1
    if (@(Get-DxaOfflineMatchBenchmarkValidationErrors `
            -Result $result `
            -CommitSha $initial.commit_sha).Count -eq 0) {
        throw 'Offline match benchmark repeat mismatch가 거부되지 않았습니다.'
    }
    $result.repeat_mismatch_count = 0

    $savedWinner = $result.winner
    $result.PSObject.Properties.Remove('winner')
    if (@(Get-DxaOfflineMatchBenchmarkValidationErrors `
            -Result $result `
            -CommitSha $initial.commit_sha).Count -eq 0) {
        throw 'Offline match benchmark winner 누락이 거부되지 않았습니다.'
    }
    $result | Add-Member -NotePropertyName winner -NotePropertyValue $savedWinner

    $result.finished_tick = 100
    if (@(Get-DxaOfflineMatchBenchmarkValidationErrors `
            -Result $result `
            -CommitSha $initial.commit_sha).Count -eq 0) {
        throw 'Offline match benchmark 종료 tick 범위 오류가 거부되지 않았습니다.'
    }
    $result.finished_tick = 16147

    $savedChecksum = $result.event_checksum
    $result.PSObject.Properties.Remove('event_checksum')
    if (@(Get-DxaOfflineMatchBenchmarkValidationErrors `
            -Result $result `
            -CommitSha $initial.commit_sha).Count -eq 0) {
        throw 'Offline match benchmark checksum 누락이 거부되지 않았습니다.'
    }
    $result | Add-Member -NotePropertyName event_checksum -NotePropertyValue $savedChecksum

    $result.compiler.id = ''
    if (@(Get-DxaOfflineMatchBenchmarkValidationErrors `
            -Result $result `
            -CommitSha $initial.commit_sha).Count -eq 0) {
        throw 'Offline match benchmark compiler 누락이 거부되지 않았습니다.'
    }
    $result.compiler.id = 'MSVC'

    @(
        [pscustomobject]@{ tick = 1; elapsed_ms = 'NaN'; alive_contenders = 24; alive_neutrals = 100; event_count = 0 }
        [pscustomobject]@{ tick = 2; elapsed_ms = 0.02; alive_contenders = 24; alive_neutrals = 100; event_count = 1 }
    ) | Export-Csv -LiteralPath $ticksPath -NoTypeInformation -Encoding utf8
    if (@(Get-DxaOfflineMatchTickValidationErrors `
            -TicksPath $ticksPath `
            -ExpectedFinishedTick 3).Count -eq 0) {
        throw 'Offline match benchmark 잘린 비정상 tick CSV가 거부되지 않았습니다.'
    }
}
finally {
    if (Test-Path -LiteralPath $resolvedTemporaryRoot) {
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
