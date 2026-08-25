Set-StrictMode -Version Latest

function Get-DxaNetworkLoadGitSnapshot {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot
    )

    $status = @(& git -C $RepositoryRoot status --porcelain=v1 --untracked-files=all)
    if ($LASTEXITCODE -ne 0) {
        throw 'Network load Git 상태를 확인하지 못했습니다.'
    }
    $commitSha = (@(& git -C $RepositoryRoot rev-parse HEAD) -join '').Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($commitSha)) {
        throw 'Network load commit SHA를 확인하지 못했습니다.'
    }
    $shortSha = (@(& git -C $RepositoryRoot rev-parse --short=8 HEAD) -join '').Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($shortSha)) {
        throw 'Network load short SHA를 확인하지 못했습니다.'
    }
    $branch = (@(& git -C $RepositoryRoot branch --show-current) -join '').Trim()
    if ($LASTEXITCODE -ne 0) {
        throw 'Network load branch를 확인하지 못했습니다.'
    }

    [pscustomobject]@{
        commit_sha = $commitSha
        short_sha = $shortSha
        branch = $branch
        clean = $status.Count -eq 0
    }
}

function Assert-DxaNetworkLoadRequest {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot,

        [AllowEmptyString()]
        [string]$CommitSha,

        [Parameter(Mandatory)]
        [string]$OutputDirectory,

        [ValidateSet('full-state')]
        [string]$ReplicationMode,

        [int]$Matches,
        [uint32[]]$Seeds,
        [int]$BotCount
    )

    if ([string]::IsNullOrWhiteSpace($CommitSha)) {
        throw 'Network load 실행에는 commit SHA가 필요합니다.'
    }
    if ($Matches -lt 1 -or $Matches -gt 3) {
        throw 'Network load match 수는 1부터 3이어야 합니다.'
    }
    if ($null -eq $Seeds -or $Seeds.Count -ne $Matches) {
        throw 'Network load seed 수가 match 수와 일치해야 합니다.'
    }
    if ($BotCount -ne 23) {
        throw 'Network load bot 수는 정확히 23이어야 합니다.'
    }
    if ($ReplicationMode -ne 'full-state') {
        throw '현재 runner는 full-state mode만 지원합니다.'
    }

    $resolvedRepository = [IO.Path]::GetFullPath($RepositoryRoot)
    $resolvedOutput = [IO.Path]::GetFullPath($OutputDirectory)
    $repositoryPrefix = $resolvedRepository.TrimEnd(
        [IO.Path]::DirectorySeparatorChar,
        [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (-not $resolvedOutput.StartsWith(
            $repositoryPrefix,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Network load evidence 경로는 repository 내부여야 합니다.'
    }
    if (Test-Path -LiteralPath $resolvedOutput) {
        throw "Network load 실행 디렉터리가 이미 존재합니다: $resolvedOutput"
    }

    $snapshot = Get-DxaNetworkLoadGitSnapshot -RepositoryRoot $resolvedRepository
    if (-not $snapshot.clean) {
        throw 'Network load는 깨끗한 commit에서만 실행할 수 있습니다.'
    }
    if ($snapshot.commit_sha -ne $CommitSha) {
        throw "Network load HEAD가 요청 SHA와 다릅니다. 예상: $CommitSha, 실제: $($snapshot.commit_sha)"
    }
    return $snapshot
}

function Invoke-DxaNetworkLoadGuarded {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot,

        [AllowEmptyString()]
        [string]$CommitSha,

        [Parameter(Mandatory)]
        [string]$OutputDirectory,

        [ValidateSet('full-state')]
        [string]$ReplicationMode,

        [int]$Matches,
        [uint32[]]$Seeds,
        [int]$BotCount,

        [Parameter(Mandatory)]
        [scriptblock]$Action
    )

    $snapshot = Assert-DxaNetworkLoadRequest `
        -RepositoryRoot $RepositoryRoot `
        -CommitSha $CommitSha `
        -OutputDirectory $OutputDirectory `
        -ReplicationMode $ReplicationMode `
        -Matches $Matches `
        -Seeds $Seeds `
        -BotCount $BotCount
    $actionResult = & $Action
    [pscustomobject]@{
        git = $snapshot
        action_result = $actionResult
    }
}

function Write-DxaNetworkLoadUtf8 {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [AllowEmptyString()]
        [string]$Contents
    )

    [IO.File]::WriteAllText(
        $Path,
        $Contents,
        [Text.UTF8Encoding]::new($false))
}

function ConvertTo-DxaProcessArgumentString {
    [CmdletBinding()]
    param(
        [object[]]$Arguments
    )

    return (@($Arguments | ForEach-Object {
        $value = [string]$_
        if ($value -notmatch '[\s"]') {
            $value
        }
        else {
            '"' + $value.Replace('"', '\"') + '"'
        }
    }) -join ' ')
}

function Start-DxaLoggedProcess {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$FilePath,

        [object[]]$Arguments,

        [Parameter(Mandatory)]
        [string]$StdoutPath,

        [Parameter(Mandatory)]
        [string]$StderrPath
    )

    if (-not (Test-Path -LiteralPath $FilePath -PathType Leaf)) {
        throw "Network load executable을 찾지 못했습니다: $FilePath"
    }
    $targetArguments = ConvertTo-DxaProcessArgumentString -Arguments $Arguments
    $targetCommand = '"' + $FilePath + '" ' + $targetArguments
    $targetCommand += ' 1>"' + $StdoutPath + '" 2>"' + $StderrPath + '"'
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $env:ComSpec
    $startInfo.Arguments = '/d /s /c "' + $targetCommand + '"'
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw "Network load process를 시작하지 못했습니다: $FilePath"
    }
    return $process
}

function Stop-DxaProcessTreeIfRunning {
    [CmdletBinding()]
    param(
        [AllowNull()]
        [Diagnostics.Process]$Process
    )

    if ($null -eq $Process) {
        return
    }
    $Process.Refresh()
    if (-not $Process.HasExited) {
        & taskkill /PID $Process.Id /T /F 2>&1 | Out-Null
        $Process.WaitForExit(5000) | Out-Null
    }
}

function Wait-DxaNetworkLoadProcesses {
    [CmdletBinding()]
    param(
        [Diagnostics.Process[]]$Processes,
        [int]$TimeoutSeconds
    )

    $deadline = [DateTimeOffset]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTimeOffset]::UtcNow -lt $deadline) {
        $allExited = $true
        foreach ($process in $Processes) {
            $process.Refresh()
            if (-not $process.HasExited) {
                $allExited = $false
            }
        }
        if ($allExited) {
            foreach ($process in $Processes) {
                $process.WaitForExit()
            }
            return
        }
        Start-Sleep -Milliseconds 100
    }
    throw 'Network load client process 대기가 시간 초과됐습니다.'
}

function Get-DxaNetworkLoadMatchSeed {
    [CmdletBinding()]
    param(
        [uint64]$MatchId,
        [uint32]$SeedBase = 20260824
    )

    $low = [uint32]($MatchId -band [uint64]4294967295)
    $high = [uint32]($MatchId -shr 32)
    return [uint32]($SeedBase -bxor $low -bxor $high)
}

function ConvertTo-DxaEvidenceCommandText {
    [CmdletBinding()]
    param(
        [string[]]$CommandLines,
        [Parameter(Mandatory)]
        [string]$RepositoryRoot,
        [Parameter(Mandatory)]
        [string]$RunDirectory
    )

    $text = $CommandLines -join "`n"
    $text = $text.Replace($RunDirectory, '<RUN_DIRECTORY>')
    return $text.Replace($RepositoryRoot, '<REPOSITORY_ROOT>')
}

function Get-DxaNearestRankP95 {
    [CmdletBinding()]
    param(
        [double[]]$Values
    )

    if ($null -eq $Values -or $Values.Count -eq 0) {
        return 0.0
    }
    $sorted = @($Values | Sort-Object)
    $rank = $sorted.Count - [Math]::Floor($sorted.Count / 20.0)
    return [double]$sorted[[int]$rank - 1]
}
