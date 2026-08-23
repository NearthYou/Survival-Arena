function Assert-DxaOfflineMatchOutputDirectoryAvailable {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$OutputDirectory
    )

    if (Test-Path -LiteralPath $OutputDirectory) {
        throw "Offline match benchmark 실행 디렉터리가 이미 존재합니다: $OutputDirectory"
    }
}

function Test-DxaOfflineMatchFiniteNonNegativeNumber {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [object]$Value
    )

    try {
        $number = [Convert]::ToDouble(
            $Value,
            [Globalization.CultureInfo]::InvariantCulture)
    }
    catch {
        return $false
    }
    return -not [double]::IsNaN($number) -and
        -not [double]::IsInfinity($number) -and
        $number -ge 0.0
}

function Get-DxaOfflineMatchBenchmarkValidationErrors {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [psobject]$Result,

        [Parameter(Mandatory)]
        [string]$CommitSha,

        [uint32]$Seed = 20260823
    )

    $errors = [Collections.Generic.List[string]]::new()
    if ([uint32]$Result.schema_version -ne 1 -or
        [string]$Result.commit_sha -ne $CommitSha -or
        [uint32]$Result.seed -ne $Seed) {
        $errors.Add('Offline match benchmark 메타데이터가 실행 인자와 일치하지 않습니다.')
    }
    if ([uint64]$Result.repeat_mismatch_count -ne 0) {
        $errors.Add("Offline match benchmark 반복 결과가 다릅니다: $($Result.repeat_mismatch_count)건")
    }

    $winnerProperty = $Result.PSObject.Properties['winner']
    if ($null -eq $winnerProperty -or $null -eq $winnerProperty.Value) {
        $errors.Add('Offline match benchmark winner가 없습니다.')
    }
    if ([string]$Result.end_reason -notin @('last_survivor', 'time_limit')) {
        $errors.Add('Offline match benchmark 종료 이유가 유효하지 않습니다.')
    }
    if ([uint32]$Result.finished_tick -lt 14400 -or
        [uint32]$Result.finished_tick -gt 18000) {
        $errors.Add("Offline match benchmark 종료 tick이 범위를 벗어났습니다: $($Result.finished_tick)")
    }

    $checksumProperty = $Result.PSObject.Properties['event_checksum']
    if ($null -eq $checksumProperty -or
        [string]::IsNullOrWhiteSpace([string]$checksumProperty.Value)) {
        $errors.Add('Offline match benchmark event checksum이 없습니다.')
    }
    foreach ($metricName in @('p50', 'p95', 'max')) {
        $metric = $Result.tick_ms.PSObject.Properties[$metricName]
        if ($null -eq $metric -or
            -not (Test-DxaOfflineMatchFiniteNonNegativeNumber -Value $metric.Value)) {
            $errors.Add("Offline match benchmark tick metric이 유효하지 않습니다: $metricName")
        }
    }
    if ((Test-DxaOfflineMatchFiniteNonNegativeNumber -Value $Result.tick_ms.p95) -and
        [double]$Result.tick_ms.p95 -gt 33.3) {
        $errors.Add("Offline match benchmark tick P95가 33.3ms를 초과했습니다: $($Result.tick_ms.p95)")
    }
    if ([uint32]$Result.population.contenders -ne 24 -or
        [uint32]$Result.population.neutrals -ne 100) {
        $errors.Add('Offline match benchmark population이 24명과 100마리가 아닙니다.')
    }
    if ([string]::IsNullOrWhiteSpace([string]$Result.compiler.id) -or
        [string]::IsNullOrWhiteSpace([string]$Result.compiler.version) -or
        [uint32]$Result.cpu.logical_processors -eq 0) {
        $errors.Add('Offline match benchmark CPU 또는 compiler 정보가 없습니다.')
    }
    return $errors
}

function Get-DxaOfflineMatchTickValidationErrors {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$TicksPath,

        [Parameter(Mandatory)]
        [ValidateRange(1, [uint32]::MaxValue)]
        [uint32]$ExpectedFinishedTick
    )

    $errors = [Collections.Generic.List[string]]::new()
    if (-not (Test-Path -LiteralPath $TicksPath -PathType Leaf)) {
        $errors.Add("Offline match benchmark tick CSV가 없습니다: $TicksPath")
        return $errors
    }
    try {
        $rows = @(Import-Csv -LiteralPath $TicksPath)
    }
    catch {
        $errors.Add("Offline match benchmark tick CSV를 읽지 못했습니다: $($_.Exception.Message)")
        return $errors
    }
    if ($rows.Count -ne $ExpectedFinishedTick) {
        $errors.Add(
            "Offline match benchmark tick 행 수가 다릅니다. 예상: $ExpectedFinishedTick, 실제: $($rows.Count)")
    }

    if ($rows.Count -gt 0) {
        $requiredColumns = @(
            'tick',
            'elapsed_ms',
            'alive_contenders',
            'alive_neutrals',
            'event_count')
        $actualColumns = @($rows[0].PSObject.Properties.Name)
        $missingColumns = @($requiredColumns | Where-Object { $_ -notin $actualColumns })
        if ($missingColumns.Count -gt 0) {
            $errors.Add(
                "Offline match benchmark tick 필수 열이 없습니다: $($missingColumns -join ', ')")
            return $errors
        }
    }

    for ($index = 0; $index -lt $rows.Count; ++$index) {
        $row = $rows[$index]
        $expectedTick = $index + 1
        $actualTick = 0U
        $aliveContenders = 0U
        $aliveNeutrals = 0U
        $eventCount = 0U
        $integerStyle = [Globalization.NumberStyles]::None
        $culture = [Globalization.CultureInfo]::InvariantCulture
        if (-not [uint32]::TryParse(
                [string]$row.tick, $integerStyle, $culture, [ref]$actualTick) -or
            -not [uint32]::TryParse(
                [string]$row.alive_contenders,
                $integerStyle,
                $culture,
                [ref]$aliveContenders) -or
            -not [uint32]::TryParse(
                [string]$row.alive_neutrals,
                $integerStyle,
                $culture,
                [ref]$aliveNeutrals) -or
            -not [uint32]::TryParse(
                [string]$row.event_count,
                $integerStyle,
                $culture,
                [ref]$eventCount)) {
            $errors.Add("Offline match benchmark tick 행 숫자를 읽지 못했습니다: $expectedTick")
            continue
        }
        if ($actualTick -ne $expectedTick) {
            $errors.Add("Offline match benchmark tick 순서가 연속적이지 않습니다: $actualTick")
            break
        }
        if (-not (Test-DxaOfflineMatchFiniteNonNegativeNumber -Value $row.elapsed_ms)) {
            $errors.Add("Offline match benchmark elapsed_ms가 유효하지 않습니다: $actualTick")
            break
        }
        if ($aliveContenders -gt 24 -or $aliveNeutrals -gt 100) {
            $errors.Add("Offline match benchmark alive count가 population을 초과했습니다: $actualTick")
            break
        }
        $null = $eventCount
    }
    return $errors
}
