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
        warmup_frames = 120
        measured_frames = 600
        sample_count = 600
        gpu_sample_count = 600
        gpu_missing_samples = 0
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
        warmup_frames = 120
        measured_frames = 600
        sample_count = 600
        gpu_sample_count = 600
        gpu_missing_samples = 0
        gpu_total_sample_count = 600
        gpu_shadow_sample_count = 600
        gpu_gbuffer_sample_count = 600
        gpu_lighting_sample_count = 600
        gpu_transparent_sample_count = 600
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
        warmup_frames = 120
        measured_frames = 600
        sample_count = 600
        gpu_sample_count = 600
        gpu_missing_samples = 0
        gpu_total_sample_count = 600
        gpu_shadow_sample_count = 600
        gpu_gbuffer_sample_count = 600
        gpu_lighting_sample_count = 600
        gpu_transparent_sample_count = 600
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

    $windowMismatchRun = Join-Path $resolvedTemporaryRoot 'window-mismatch'
    Write-TestSummary -Directory $windowMismatchRun -Summary @{
        schema_version = 2
        render_path = 'hybrid-deferred'
        seed = 20260823
        resolution = @{ width = 1920; height = 1080 }
        adapter = 'Test GPU'
        commit_sha = 'window-mismatch-sha'
        warmup_frames = 0
        measured_frames = 600
        sample_count = 600
        gpu_sample_count = 600
        gpu_missing_samples = 0
        gpu_total_sample_count = 600
        gpu_shadow_sample_count = 600
        gpu_gbuffer_sample_count = 600
        gpu_lighting_sample_count = 600
        gpu_transparent_sample_count = 600
        metrics = @{
            cpu_frame_ms = @{ p95 = 3.0 }
            gpu_total_ms = @{ p95 = 2.0 }
            draw_calls = @{ p50 = 500.0 }
            working_set_bytes = @{ p95 = 2200.0 }
        }
    }
    $windowMismatchRejected = $false
    try {
        & $comparisonScript -ForwardRun $forwardRun -HybridRun $windowMismatchRun
    }
    catch {
        $windowMismatchRejected =
            $_.Exception.Message -eq 'Benchmark 측정 구간이 일치하지 않습니다.'
    }
    if (-not $windowMismatchRejected) {
        throw '다른 camera measurement window 비교가 거부되지 않았습니다.'
    }

    $incompleteRun = Join-Path $resolvedTemporaryRoot 'incomplete'
    Write-TestSummary -Directory $incompleteRun -Summary @{
        schema_version = 2
        render_path = 'hybrid-deferred'
        seed = 20260823
        resolution = @{ width = 1920; height = 1080 }
        adapter = 'Test GPU'
        commit_sha = 'incomplete-sha'
        warmup_frames = 120
        measured_frames = 600
        sample_count = 600
        gpu_sample_count = 599
        gpu_missing_samples = 1
        gpu_total_sample_count = 599
        gpu_shadow_sample_count = 599
        gpu_gbuffer_sample_count = 599
        gpu_lighting_sample_count = 599
        gpu_transparent_sample_count = 599
        metrics = @{
            cpu_frame_ms = @{ p95 = 3.0 }
            gpu_total_ms = @{ p95 = 2.0 }
            draw_calls = @{ p50 = 500.0 }
            working_set_bytes = @{ p95 = 2200.0 }
        }
    }
    $incompleteRejected = $false
    try {
        & $comparisonScript -ForwardRun $forwardRun -HybridRun $incompleteRun
    }
    catch {
        $incompleteRejected = $_.Exception.Message -eq
            '누락된 GPU sample이 있는 benchmark는 비교할 수 없습니다.'
    }
    if (-not $incompleteRejected) {
        throw '누락 sample benchmark 비교가 거부되지 않았습니다.'
    }
}
finally {
    if (Test-Path -LiteralPath $resolvedTemporaryRoot) {
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
