function Assert-DxaSimulationOutputDirectoryAvailable {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$OutputDirectory
    )

    if (Test-Path -LiteralPath $OutputDirectory) {
        throw "Simulation benchmark 실행 디렉터리가 이미 존재합니다: $OutputDirectory"
    }
}

function Test-DxaFiniteNonNegativeNumber {
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

function Get-DxaSimulationSampleValidationErrors {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$SamplesPath,

        [Parameter(Mandatory)]
        [string[]]$ExpectedCases,

        [Parameter(Mandatory)]
        [ValidateRange(1, [uint32]::MaxValue)]
        [uint32]$ExpectedSampleCount
    )

    $errors = [Collections.Generic.List[string]]::new()
    if (-not (Test-Path -LiteralPath $SamplesPath -PathType Leaf)) {
        $errors.Add("Simulation benchmark sample CSV가 없습니다: $SamplesPath")
        return $errors
    }

    try {
        $rows = @(Import-Csv -LiteralPath $SamplesPath)
    }
    catch {
        $errors.Add("Simulation benchmark sample CSV를 읽지 못했습니다: $($_.Exception.Message)")
        return $errors
    }

    $expectedRowCount = $ExpectedCases.Count * [int]$ExpectedSampleCount
    if ($rows.Count -ne $expectedRowCount) {
        $errors.Add(
            "Simulation benchmark sample 행 수가 다릅니다. 예상: $expectedRowCount, 실제: $($rows.Count)")
    }

    $unexpectedCases = @(
        $rows.case |
            Sort-Object -Unique |
            Where-Object { $_ -notin $ExpectedCases }
    )
    if ($unexpectedCases.Count -ne 0) {
        $errors.Add("Simulation benchmark에 알 수 없는 case가 있습니다: $($unexpectedCases -join ', ')")
    }

    foreach ($caseName in $ExpectedCases) {
        $caseRows = @($rows | Where-Object { $_.case -eq $caseName })
        if ($caseRows.Count -ne $ExpectedSampleCount) {
            $errors.Add(
                "Simulation benchmark case sample 수가 다릅니다. case: $caseName, 예상: $ExpectedSampleCount, 실제: $($caseRows.Count)")
            continue
        }

        $actualIndexes = @($caseRows.sample_index | ForEach-Object { [uint32]$_ } | Sort-Object)
        $expectedIndexes = @(1..$ExpectedSampleCount)
        if (($actualIndexes -join ',') -ne ($expectedIndexes -join ',')) {
            $errors.Add("Simulation benchmark sample index가 연속적이지 않습니다: $caseName")
        }
        foreach ($row in $caseRows) {
            if (-not (Test-DxaFiniteNonNegativeNumber -Value $row.elapsed_ms)) {
                $errors.Add("Simulation benchmark elapsed_ms가 유효하지 않습니다: $caseName")
                break
            }
        }
    }
    return $errors
}

function Get-DxaSimulationBenchmarkValidationErrors {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [psobject]$Result,

        [Parameter(Mandatory)]
        [string]$CommitSha,

        [uint32]$Seed = 20260823,
        [uint32]$NavQueryCount = 100000,
        [uint32]$AabbQueryCount = 20000,
        [uint32]$PickQueryCount = 20000,
        [uint32]$AiDecisionCount = 100000
    )

    $errors = [Collections.Generic.List[string]]::new()
    if ([uint32]$Result.schema_version -ne 1 -or
        [string]$Result.commit_sha -ne $CommitSha -or
        [uint32]$Result.seed -ne $Seed -or
        [uint32]$Result.sample_count -ne 5) {
        $errors.Add('Simulation benchmark 메타데이터가 실행 인자와 일치하지 않습니다.')
    }
    if ([uint64]$Result.mismatch_count -ne 0) {
        $errors.Add("Simulation benchmark 결과 불일치가 있습니다: $($Result.mismatch_count)건")
    }
    if ([string]::IsNullOrWhiteSpace([string]$Result.result_checksum)) {
        $errors.Add('Simulation benchmark result checksum이 없습니다.')
    }
    if ([string]::IsNullOrWhiteSpace([string]$Result.compiler.id) -or
        [string]::IsNullOrWhiteSpace([string]$Result.compiler.version) -or
        [uint32]$Result.cpu.logical_processors -eq 0) {
        $errors.Add('Simulation benchmark CPU 또는 compiler 정보가 없습니다.')
    }
    if ([uint32]$Result.workload.nav_queries -ne $NavQueryCount -or
        [uint32]$Result.workload.aabb_queries -ne $AabbQueryCount -or
        [uint32]$Result.workload.pick_queries -ne $PickQueryCount -or
        [uint32]$Result.workload.ai_decisions -ne $AiDecisionCount) {
        $errors.Add('Simulation benchmark workload가 잠긴 실행 조건과 다릅니다.')
    }

    $requiredCases = @(
        'nav_linear',
        'nav_grid',
        'spatial_linear_aabb',
        'spatial_quadtree_aabb',
        'spatial_linear_pick',
        'spatial_quadtree_pick',
        'ai_fsm',
        'ai_behavior_tree'
    )
    foreach ($caseName in $requiredCases) {
        $property = $Result.cases.PSObject.Properties[$caseName]
        if ($null -eq $property -or
            [string]::IsNullOrWhiteSpace([string]$property.Value.checksum)) {
            $errors.Add("Simulation benchmark case가 없거나 checksum이 없습니다: $caseName")
            continue
        }
        if (@($property.Value.samples_ms).Count -ne 5 -or
            -not (Test-DxaFiniteNonNegativeNumber -Value $property.Value.median_ms)) {
            $errors.Add("Simulation benchmark case sample 또는 median이 유효하지 않습니다: $caseName")
            continue
        }
        foreach ($sample in @($property.Value.samples_ms)) {
            if (-not (Test-DxaFiniteNonNegativeNumber -Value $sample)) {
                $errors.Add("Simulation benchmark case 시간 sample이 유효하지 않습니다: $caseName")
                break
            }
        }
    }

    $checksumPairs = @(
        @('nav_linear', 'nav_grid'),
        @('spatial_linear_aabb', 'spatial_quadtree_aabb'),
        @('spatial_linear_pick', 'spatial_quadtree_pick'),
        @('ai_fsm', 'ai_behavior_tree')
    )
    foreach ($pair in $checksumPairs) {
        $left = $Result.cases.PSObject.Properties[$pair[0]]
        $right = $Result.cases.PSObject.Properties[$pair[1]]
        if ($null -ne $left -and $null -ne $right -and
            [string]$left.Value.checksum -ne [string]$right.Value.checksum) {
            $errors.Add("Simulation benchmark checksum pair가 다릅니다: $($pair[0]), $($pair[1])")
        }
    }
    return $errors
}
