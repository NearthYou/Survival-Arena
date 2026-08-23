[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$RepositoryRoot
)

$ErrorActionPreference = 'Stop'
$temporaryRoot = Join-Path (
    [IO.Path]::GetTempPath()) (
    'dxa-benchmark-comparison-' + [Guid]::NewGuid().ToString('N'))
$resolvedTemporaryRoot = [IO.Path]::GetFullPath($temporaryRoot)
$resolvedSystemTemporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
if (-not $resolvedTemporaryRoot.StartsWith(
        $resolvedSystemTemporaryRoot,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw '임시 비교 테스트 경로가 시스템 temp 밖입니다.'
}

function Write-TestSummary {
    param(
        [Parameter(Mandatory)]
        [string]$Directory,

        [Parameter(Mandatory)]
        [hashtable]$Summary
    )

    New-Item -ItemType Directory -Path $Directory | Out-Null
    [IO.File]::WriteAllText(
        (Join-Path $Directory 'summary.json'),
        ($Summary | ConvertTo-Json -Depth 8),
        [Text.UTF8Encoding]::new($false))
}

New-Item -ItemType Directory -Path $resolvedTemporaryRoot | Out-Null
try {
    $forwardRun = Join-Path $resolvedTemporaryRoot 'forward'
    $hybridRun = Join-Path $resolvedTemporaryRoot 'hybrid'
    Write-TestSummary -Directory $forwardRun -Summary @{
        schema_version = 1
        seed = 20260823
        resolution = @{ width = 1920; height = 1080 }
        adapter = 'Test GPU'
        commit_sha = 'forward-sha'
        metrics = @{
            cpu_frame_ms = @{ p95 = 4.0 }
            gpu_forward_ms = @{ p95 = 3.0 }
            draw_calls = @{ p50 = 2240.0 }
            working_set_bytes = @{ p95 = 2000.0 }
        }
    }
    Write-TestSummary -Directory $hybridRun -Summary @{
        schema_version = 2
        render_path = 'hybrid-deferred'
        seed = 20260823
        resolution = @{ width = 1920; height = 1080 }
        adapter = 'Test GPU'
        commit_sha = 'hybrid-sha'
        metrics = @{
            cpu_frame_ms = @{ p95 = 3.0 }
            gpu_total_ms = @{ p95 = 2.0 }
            draw_calls = @{ p50 = 500.0 }
            working_set_bytes = @{ p95 = 2200.0 }
        }
    }

    $comparisonScript = Join-Path $RepositoryRoot 'scripts/compare_benchmarks.ps1'
    & $comparisonScript -ForwardRun $forwardRun -HybridRun $hybridRun
    $comparisonPath = Join-Path $hybridRun 'comparison.json'
    if (-not (Test-Path -LiteralPath $comparisonPath -PathType Leaf)) {
        throw '검증된 benchmark comparison이 생성되지 않았습니다.'
    }
    $comparison = Get-Content -Raw -Encoding utf8 -LiteralPath $comparisonPath |
        ConvertFrom-Json
    if ([double]$comparison.metrics.cpu_frame_p95_ms.percent_change -ne -25.0 -or
        [double]$comparison.metrics.gpu_total_p95_ms.percent_change -ne -33.333333) {
        throw 'Benchmark 비교 변화율이 예상과 다릅니다.'
    }

    $mismatchRun = Join-Path $resolvedTemporaryRoot 'seed-mismatch'
    Write-TestSummary -Directory $mismatchRun -Summary @{
        schema_version = 2
        render_path = 'hybrid-deferred'
        seed = 7
        resolution = @{ width = 1920; height = 1080 }
        adapter = 'Test GPU'
        commit_sha = 'mismatch-sha'
        metrics = @{
            cpu_frame_ms = @{ p95 = 3.0 }
            gpu_total_ms = @{ p95 = 2.0 }
            draw_calls = @{ p50 = 500.0 }
            working_set_bytes = @{ p95 = 2200.0 }
        }
    }
    $mismatchRejected = $false
    try {
        & $comparisonScript -ForwardRun $forwardRun -HybridRun $mismatchRun
    }
    catch {
        $mismatchRejected = $_.Exception.Message -eq 'Benchmark seed가 일치하지 않습니다.'
    }
    if (-not $mismatchRejected) {
        throw '다른 seed benchmark 비교가 거부되지 않았습니다.'
    }
    if (Test-Path -LiteralPath (Join-Path $mismatchRun 'comparison.json')) {
        throw '검증 실패 run에 comparison 파일이 남았습니다.'
    }
}
finally {
    if (Test-Path -LiteralPath $resolvedTemporaryRoot) {
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
