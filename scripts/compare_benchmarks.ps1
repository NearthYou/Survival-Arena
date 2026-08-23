[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$ForwardRun,

    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$HybridRun
)

$ErrorActionPreference = 'Stop'

function Read-DxaBenchmarkSummary {
    param(
        [Parameter(Mandatory)]
        [string]$RunDirectory
    )

    $resolvedRun = [IO.Path]::GetFullPath($RunDirectory)
    $summaryPath = Join-Path $resolvedRun 'summary.json'
    if (-not (Test-Path -LiteralPath $summaryPath -PathType Leaf)) {
        throw "Benchmark summary를 찾지 못했습니다: $summaryPath"
    }
    $summary = Get-Content -Raw -Encoding utf8 -LiteralPath $summaryPath |
        ConvertFrom-Json
    [pscustomobject]@{
        directory = $resolvedRun
        summary_path = $summaryPath
        summary = $summary
    }
}

function Get-DxaMetricValue {
    param(
        [Parameter(Mandatory)]
        [psobject]$Summary,

        [Parameter(Mandatory)]
        [string]$MetricName,

        [Parameter(Mandatory)]
        [string]$StatisticName
    )

    $metric = $Summary.metrics.$MetricName
    if ($null -eq $metric) {
        throw "Benchmark metric이 없습니다: $MetricName"
    }
    $rawValue = $metric.$StatisticName
    if ($null -eq $rawValue) {
        throw "Benchmark metric 통계가 없습니다: $MetricName.$StatisticName"
    }
    $value = [double]$rawValue
    if ([double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -lt 0.0) {
        throw "Benchmark metric 값이 유효하지 않습니다: $MetricName.$StatisticName"
    }
    return $value
}

function New-DxaMetricComparison {
    param(
        [Parameter(Mandatory)]
        [double]$ForwardValue,

        [Parameter(Mandatory)]
        [double]$HybridValue
    )

    if ($ForwardValue -eq 0.0) {
        throw '0인 기준값으로 변화율을 계산할 수 없습니다.'
    }
    [ordered]@{
        forward = $ForwardValue
        hybrid = $HybridValue
        percent_change = [Math]::Round(
            (($HybridValue - $ForwardValue) / $ForwardValue) * 100.0,
            6)
    }
}

$forward = Read-DxaBenchmarkSummary -RunDirectory $ForwardRun
$hybrid = Read-DxaBenchmarkSummary -RunDirectory $HybridRun
if ($forward.directory -eq $hybrid.directory) {
    throw '같은 benchmark run은 서로 비교할 수 없습니다.'
}

$forwardSummary = $forward.summary
$hybridSummary = $hybrid.summary
$forwardSchema = [uint32]$forwardSummary.schema_version
$hybridSchema = [uint32]$hybridSummary.schema_version
if ($forwardSchema -notin @(1, 2)) {
    throw "지원하지 않는 forward schema입니다: $forwardSchema"
}
if ($forwardSchema -eq 2 -and $forwardSummary.render_path -ne 'forward') {
    throw 'Forward run의 render path가 forward가 아닙니다.'
}
if ($forwardSchema -eq 1 -and
    $null -ne $forwardSummary.render_path -and
    $forwardSummary.render_path -ne 'forward') {
    throw 'Schema 1 run은 forward 기준선으로만 사용할 수 있습니다.'
}
if ($hybridSchema -ne 2 -or $hybridSummary.render_path -ne 'hybrid-deferred') {
    throw 'Hybrid run은 schema 2 hybrid-deferred 결과여야 합니다.'
}

if ([uint32]$forwardSummary.seed -ne [uint32]$hybridSummary.seed) {
    throw 'Benchmark seed가 일치하지 않습니다.'
}
if ([uint32]$forwardSummary.resolution.width -ne
        [uint32]$hybridSummary.resolution.width -or
    [uint32]$forwardSummary.resolution.height -ne
        [uint32]$hybridSummary.resolution.height) {
    throw 'Benchmark 해상도가 일치하지 않습니다.'
}
if ([string]$forwardSummary.adapter -ne [string]$hybridSummary.adapter) {
    throw 'Benchmark GPU adapter가 일치하지 않습니다.'
}

$forwardGpuMetric = if ($forwardSchema -eq 1) {
    'gpu_forward_ms'
}
else {
    'gpu_total_ms'
}
$cpuForward = Get-DxaMetricValue $forwardSummary 'cpu_frame_ms' 'p95'
$cpuHybrid = Get-DxaMetricValue $hybridSummary 'cpu_frame_ms' 'p95'
$gpuForward = Get-DxaMetricValue $forwardSummary $forwardGpuMetric 'p95'
$gpuHybrid = Get-DxaMetricValue $hybridSummary 'gpu_total_ms' 'p95'
$drawForward = Get-DxaMetricValue $forwardSummary 'draw_calls' 'p50'
$drawHybrid = Get-DxaMetricValue $hybridSummary 'draw_calls' 'p50'
$memoryForward = Get-DxaMetricValue $forwardSummary 'working_set_bytes' 'p95'
$memoryHybrid = Get-DxaMetricValue $hybridSummary 'working_set_bytes' 'p95'

$comparisonPath = Join-Path $hybrid.directory 'comparison.json'
if (Test-Path -LiteralPath $comparisonPath) {
    throw "Benchmark comparison 파일이 이미 존재합니다: $comparisonPath"
}

$comparison = [ordered]@{
    schema_version = 1
    generated_at = [DateTimeOffset]::UtcNow.ToString('o')
    forward = [ordered]@{
        run_id = Split-Path -Leaf $forward.directory
        schema_version = $forwardSchema
        commit_sha = [string]$forwardSummary.commit_sha
        render_path = 'forward'
    }
    hybrid = [ordered]@{
        run_id = Split-Path -Leaf $hybrid.directory
        schema_version = $hybridSchema
        commit_sha = [string]$hybridSummary.commit_sha
        render_path = 'hybrid-deferred'
    }
    comparable_scene = [ordered]@{
        seed = [uint32]$forwardSummary.seed
        width = [uint32]$forwardSummary.resolution.width
        height = [uint32]$forwardSummary.resolution.height
        adapter = [string]$forwardSummary.adapter
    }
    metrics = [ordered]@{
        cpu_frame_p95_ms = New-DxaMetricComparison $cpuForward $cpuHybrid
        gpu_total_p95_ms = New-DxaMetricComparison $gpuForward $gpuHybrid
        draw_calls_p50 = New-DxaMetricComparison $drawForward $drawHybrid
        working_set_p95_bytes = New-DxaMetricComparison $memoryForward $memoryHybrid
    }
}
[IO.File]::WriteAllText(
    $comparisonPath,
    ($comparison | ConvertTo-Json -Depth 8),
    [Text.UTF8Encoding]::new($false))

Write-Output "Benchmark 비교 완료: $comparisonPath"
