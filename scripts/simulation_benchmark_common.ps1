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
