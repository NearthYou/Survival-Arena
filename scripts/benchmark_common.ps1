function Get-DxaGitSnapshot {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot
    )

    $status = @(& git -C $RepositoryRoot status --porcelain=v1 --untracked-files=all)
    if ($LASTEXITCODE -ne 0) {
        throw 'Git 작업 트리 상태를 확인하지 못했습니다.'
    }

    $commitSha = @(& git -C $RepositoryRoot rev-parse HEAD) -join ''
    if ($LASTEXITCODE -ne 0) {
        throw 'Git commit SHA를 확인하지 못했습니다.'
    }
    $commitSha = $commitSha.Trim()

    $shortSha = @(& git -C $RepositoryRoot rev-parse --short=8 HEAD) -join ''
    if ($LASTEXITCODE -ne 0) {
        throw 'Git short SHA를 확인하지 못했습니다.'
    }
    $shortSha = $shortSha.Trim()

    $branch = @(& git -C $RepositoryRoot branch --show-current) -join ''
    if ($LASTEXITCODE -ne 0) {
        throw 'Git branch를 확인하지 못했습니다.'
    }

    [pscustomobject]@{
        commit_sha = $commitSha
        short_sha = $shortSha
        branch = $branch.Trim()
        clean = $status.Count -eq 0
    }
}

function Assert-DxaGitSnapshot {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [psobject]$Snapshot,

        [Parameter(Mandatory)]
        [string]$ExpectedCommitSha
    )

    if (-not $Snapshot.clean) {
        throw '기준선은 깨끗한 commit에서만 실행할 수 있습니다.'
    }
    if ($Snapshot.commit_sha -ne $ExpectedCommitSha) {
        throw "기준선 준비 중 HEAD가 바뀌었습니다. 예상: $ExpectedCommitSha, 실제: $($Snapshot.commit_sha)"
    }
}

function Get-DxaBenchmarkValidationErrors {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [psobject]$Summary,

        [Parameter(Mandatory)]
        [string]$CommitSha,

        [uint32]$Seed,
        [uint32]$Width,
        [uint32]$Height,
        [uint32]$MeasuredFrames,

        [AllowEmptyString()]
        [string]$ExpectedAdapter
    )

    $errors = [Collections.Generic.List[string]]::new()
    if ($Summary.commit_sha -ne $CommitSha -or
        [uint32]$Summary.seed -ne $Seed -or
        [uint32]$Summary.resolution.width -ne $Width -or
        [uint32]$Summary.resolution.height -ne $Height -or
        [uint32]$Summary.sample_count -ne $MeasuredFrames) {
        $errors.Add('Benchmark 요약이 실행 인자와 일치하지 않습니다.')
    }
    if (-not [string]::IsNullOrWhiteSpace($ExpectedAdapter) -and
        $Summary.adapter -ne $ExpectedAdapter) {
        $errors.Add("예상 GPU와 실제 GPU가 다릅니다. 예상: $ExpectedAdapter, 실제: $($Summary.adapter)")
    }
    if ([uint32]$Summary.gpu_missing_samples -ne 0) {
        $errors.Add("GPU timestamp가 누락됐습니다: $($Summary.gpu_missing_samples)개")
    }
    return $errors
}
