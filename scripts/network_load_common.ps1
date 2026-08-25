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

        [ValidateSet(
            'full-state',
            'interest-full',
            'interest-quantized',
            'interest-delta')]
        [string]$ReplicationMode,

        [int]$Matches,
        [uint32[]]$Seeds,
        [int]$BotCount,
        [int]$SoakMinutes = 0
    )

    if ([string]::IsNullOrWhiteSpace($CommitSha)) {
        throw 'Network load 실행에는 commit SHA가 필요합니다.'
    }
    if ($Matches -lt 1 -or $Matches -gt 3) {
        throw 'Network load match 수는 1부터 3이어야 합니다.'
    }
    if ($SoakMinutes -ne 0 -and $SoakMinutes -ne 30) {
        throw 'Network load soak은 0분 또는 30분이어야 합니다.'
    }
    if ($null -eq $Seeds -or $Seeds.Count -ne $Matches) {
        throw 'Network load seed 수가 실행 mode와 일치해야 합니다.'
    }
    if ($BotCount -ne 23) {
        throw 'Network load bot 수는 정확히 23이어야 합니다.'
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

        [ValidateSet(
            'full-state',
            'interest-full',
            'interest-quantized',
            'interest-delta')]
        [string]$ReplicationMode,

        [int]$Matches,
        [uint32[]]$Seeds,
        [int]$BotCount,
        [int]$SoakMinutes = 0,

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
        -BotCount $BotCount `
        -SoakMinutes $SoakMinutes
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
        [int]$TimeoutSeconds,
        [scriptblock]$OnSample,
        [int]$SampleIntervalSeconds = 10
    )

    if ($SampleIntervalSeconds -le 0) {
        throw 'Network load sample interval은 양수여야 합니다.'
    }
    $deadline = [DateTimeOffset]::UtcNow.AddSeconds($TimeoutSeconds)
    $nextSample = [DateTimeOffset]::MinValue
    while ([DateTimeOffset]::UtcNow -lt $deadline) {
        $now = [DateTimeOffset]::UtcNow
        if ($null -ne $OnSample -and $now -ge $nextSample) {
            & $OnSample
            $nextSample = $now.AddSeconds($SampleIntervalSeconds)
        }
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

function Get-DxaProcessTreeWorkingSetBytes {
    [CmdletBinding()]
    param(
        [AllowNull()]
        [Diagnostics.Process]$Process
    )

    if ($null -eq $Process) {
        return [uint64]0
    }
    try {
        $Process.Refresh()
        if ($Process.HasExited) {
            return [uint64]0
        }
        $rootId = [uint32]$Process.Id
        $processRows = @(Get-CimInstance Win32_Process |
            Select-Object ProcessId, ParentProcessId)
        $pending = [Collections.Generic.Queue[uint32]]::new()
        $pending.Enqueue($rootId)
        $ids = [Collections.Generic.HashSet[uint32]]::new()
        while ($pending.Count -gt 0) {
            $current = $pending.Dequeue()
            if (-not $ids.Add($current)) {
                continue
            }
            foreach ($child in $processRows) {
                if ([uint32]$child.ParentProcessId -eq $current) {
                    $pending.Enqueue([uint32]$child.ProcessId)
                }
            }
        }
        [uint64]$total = 0
        foreach ($processId in $ids) {
            $sample = Get-Process -Id $processId -ErrorAction SilentlyContinue
            if ($null -ne $sample) {
                $total += [uint64]$sample.WorkingSet64
            }
        }
        return $total
    }
    catch {
        return [uint64]0
    }
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

function Test-DxaMonotonicWorkingSetIncrease {
    [CmdletBinding()]
    param(
        [object[]]$Rows,
        [int]$WindowSeconds = 900,
        [string]$ProcessName = 'game_server'
    )

    if ($WindowSeconds -le 0) {
        throw 'Working set window는 양수여야 합니다.'
    }
    $samples = @($Rows |
        Where-Object { [string]$_.process -eq $ProcessName } |
        Sort-Object timestamp)
    if ($samples.Count -lt 2) {
        return $false
    }
    $latest = [DateTimeOffset]$samples[-1].timestamp
    $cutoff = $latest.AddSeconds(-$WindowSeconds)
    $window = @($samples | Where-Object {
        [DateTimeOffset]$_.timestamp -ge $cutoff
    })
    if ($window.Count -lt 2) {
        return $false
    }
    for ($index = 1; $index -lt $window.Count; ++$index) {
        if ([uint64]$window[$index].working_set_bytes -le
            [uint64]$window[$index - 1].working_set_bytes) {
            return $false
        }
    }
    return $true
}

function Get-DxaNetworkLoadAggregate {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$ParentDirectory,

        [Parameter(Mandatory)]
        [string]$ExpectedCommitSha,

        [Parameter(Mandatory)]
        [uint32[]]$ExpectedSeeds,

        [int]$ExpectedParticipantCount = 24,
        [int]$WorkingSetWindowSeconds = 900
    )

    if ([string]::IsNullOrWhiteSpace($ExpectedCommitSha)) {
        throw 'Aggregate commit SHA가 비어 있습니다.'
    }
    if ($ExpectedParticipantCount -le 0) {
        throw 'Aggregate participant count는 양수여야 합니다.'
    }
    if ($null -eq $ExpectedSeeds -or $ExpectedSeeds.Count -eq 0) {
        throw 'Aggregate seed가 비어 있습니다.'
    }
    if (-not (Test-Path -LiteralPath $ParentDirectory -PathType Container)) {
        throw 'Aggregate parent directory를 찾지 못했습니다.'
    }

    $matchDirectories = @(Get-ChildItem `
        -LiteralPath $ParentDirectory `
        -Directory |
        Where-Object { $_.Name -match '^match-[0-9]+$' } |
        Sort-Object Name)
    if ($matchDirectories.Count -ne $ExpectedSeeds.Count) {
        throw 'Aggregate match directory 수와 seed 수가 다릅니다.'
    }

    $tickDurations = [Collections.Generic.List[double]]::new()
    $recipientRates = [Collections.Generic.List[double]]::new()
    $workingSetRows = [Collections.Generic.List[object]]::new()
    $results = [Collections.Generic.List[object]]::new()
    [uint64]$tickSampleCount = 0
    [uint64]$clientRowCount = 0
    [uint64]$gameReceivedBytes = 0
    [uint64]$failedProcessCount = 0
    [uint64]$protocolErrors = 0
    [uint64]$shapedQueueOverflows = 0
    [uint64]$secretLeakCount = 0

    for ($matchIndex = 0; $matchIndex -lt $matchDirectories.Count; ++$matchIndex) {
        $directory = $matchDirectories[$matchIndex].FullName
        $metadataPath = Join-Path $directory 'match.json'
        $tickPath = Join-Path $directory 'server-ticks.csv'
        $clientPath = Join-Path $directory 'clients.csv'
        $workingSetPath = Join-Path $directory 'working-set.csv'
        foreach ($requiredPath in @(
                $metadataPath,
                $tickPath,
                $clientPath,
                $workingSetPath)) {
            if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
                throw "Aggregate evidence file이 없습니다: $requiredPath"
            }
        }

        $metadata = Get-Content -Raw -LiteralPath $metadataPath |
            ConvertFrom-Json
        if ([string]$metadata.commit_sha -ne $ExpectedCommitSha) {
            throw 'Aggregate match commit SHA가 요청과 다릅니다.'
        }
        if ([uint32]$metadata.seed -ne $ExpectedSeeds[$matchIndex]) {
            throw 'Aggregate match seed 순서가 요청과 다릅니다.'
        }
        if ([int]$metadata.participant_count -ne $ExpectedParticipantCount) {
            throw 'Aggregate participant count가 요청과 다릅니다.'
        }
        if ([int]$metadata.exit_code -ne 0) {
            ++$failedProcessCount
        }
        $protocolErrors += [uint64]$metadata.protocol_errors
        $shapedQueueOverflows += [uint64]$metadata.shaped_queue_overflows
        $secretLeakCount += [uint64]$metadata.secret_leak_count
        $results.Add($metadata)

        $ticks = @(Import-Csv -LiteralPath $tickPath)
        if ($ticks.Count -eq 0) {
            throw 'Aggregate server tick sample이 비어 있습니다.'
        }
        foreach ($tick in $ticks) {
            $duration = [double]$tick.duration_ns
            if ($duration -lt 0.0) {
                throw 'Aggregate server tick duration이 음수입니다.'
            }
            $tickDurations.Add($duration)
            ++$tickSampleCount
        }

        $clients = @(Import-Csv -LiteralPath $clientPath)
        if ($clients.Count -ne $ExpectedParticipantCount) {
            throw 'Aggregate client row 수가 participant count와 다릅니다.'
        }
        foreach ($client in $clients) {
            $bytes = [uint64]$client.game_received_bytes
            $seconds = [double]$client.measurement_seconds
            if ($seconds -le 0.0) {
                throw 'Aggregate client measurement seconds가 양수가 아닙니다.'
            }
            if ([int]$client.exit_code -ne 0) {
                ++$failedProcessCount
            }
            $protocolErrors += [uint64]$client.protocol_errors
            $shapedQueueOverflows += [uint64]$client.shaped_queue_overflows
            $gameReceivedBytes += $bytes
            ++$clientRowCount
            $recipientRates.Add($bytes / $seconds / 1024.0)
        }

        $workingRows = @(Import-Csv -LiteralPath $workingSetPath)
        foreach ($workingRow in $workingRows) {
            $timestamp = [DateTimeOffset]::Parse(
                [string]$workingRow.timestamp_utc,
                [Globalization.CultureInfo]::InvariantCulture)
            $workingSetRows.Add([pscustomobject]@{
                timestamp = $timestamp
                process = [string]$workingRow.process
                working_set_bytes = [uint64]$workingRow.working_set_bytes
            })
        }
    }

    $tickP95Ms = (Get-DxaNearestRankP95 `
        -Values $tickDurations.ToArray()) / 1000000.0
    $averageRecipientRate = [double](
        ($recipientRates | Measure-Object -Average).Average)
    $recipientP95 = Get-DxaNearestRankP95 -Values $recipientRates.ToArray()
    $monotonicIncrease = Test-DxaMonotonicWorkingSetIncrease `
        -Rows $workingSetRows.ToArray() `
        -WindowSeconds $WorkingSetWindowSeconds
    $gameServerWorkingSetSampleCount = @($workingSetRows |
        Where-Object { [string]$_.process -eq 'game_server' }).Count

    return [pscustomobject]@{
        schema_version = 2
        commit_sha = $ExpectedCommitSha
        match_count = $matchDirectories.Count
        seeds = $ExpectedSeeds
        participant_count = $ExpectedParticipantCount
        results = $results.ToArray()
        raw = [pscustomobject]@{
            tick_sample_count = $tickSampleCount
            client_row_count = $clientRowCount
            game_received_bytes = $gameReceivedBytes
            failed_process_count = $failedProcessCount
            protocol_errors = $protocolErrors
            shaped_queue_overflows = $shapedQueueOverflows
            secret_leak_count = $secretLeakCount
            working_set_sample_count = $workingSetRows.Count
            game_server_working_set_sample_count =
                $gameServerWorkingSetSampleCount
        }
        metrics = [pscustomobject]@{
            server_tick_p95_ms = [Math]::Round($tickP95Ms, 6)
            participant_average_received_kib_per_second = [Math]::Round(
                $averageRecipientRate,
                6)
            recipient_received_kib_per_second_p95 = [Math]::Round(
                $recipientP95,
                6)
        }
        guards = [pscustomobject]@{
            monotonic_working_set_increase = $monotonicIncrease
            working_set_window_seconds = $WorkingSetWindowSeconds
        }
    }
}

function Assert-DxaNetworkLoadAggregateReady {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [object]$Aggregate,
        [int]$MinimumGameServerWorkingSetSamples = 0
    )

    if ($Aggregate.raw.failed_process_count -ne 0) {
        throw 'Aggregate process exit code가 0이 아닙니다.'
    }
    if ($Aggregate.raw.protocol_errors -ne 0) {
        throw 'Aggregate protocol error가 0이 아닙니다.'
    }
    if ($Aggregate.raw.shaped_queue_overflows -ne 0) {
        throw 'Aggregate shaped queue overflow가 0이 아닙니다.'
    }
    if ($Aggregate.raw.secret_leak_count -ne 0) {
        throw 'Aggregate secret leak count가 0이 아닙니다.'
    }
    if ($Aggregate.guards.monotonic_working_set_increase) {
        throw 'Aggregate working set이 측정 구간마다 증가했습니다.'
    }
    if ($Aggregate.raw.game_server_working_set_sample_count -lt
        $MinimumGameServerWorkingSetSamples) {
        throw 'Aggregate game server working set sample이 부족합니다.'
    }
}

function Write-DxaNetworkLoadAggregate {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$ParentDirectory,

        [Parameter(Mandatory)]
        [string]$ExpectedCommitSha,

        [Parameter(Mandatory)]
        [uint32[]]$ExpectedSeeds,

        [int]$ExpectedParticipantCount = 24,
        [int]$WorkingSetWindowSeconds = 900,
        [int]$MinimumGameServerWorkingSetSamples = 0
    )

    $aggregate = Get-DxaNetworkLoadAggregate `
        -ParentDirectory $ParentDirectory `
        -ExpectedCommitSha $ExpectedCommitSha `
        -ExpectedSeeds $ExpectedSeeds `
        -ExpectedParticipantCount $ExpectedParticipantCount `
        -WorkingSetWindowSeconds $WorkingSetWindowSeconds
    Assert-DxaNetworkLoadAggregateReady `
        -Aggregate $aggregate `
        -MinimumGameServerWorkingSetSamples $MinimumGameServerWorkingSetSamples

    $summaryPath = Join-Path $ParentDirectory 'summary.json'
    $resultPath = Join-Path $ParentDirectory 'RESULT.md'
    if ((Test-Path -LiteralPath $summaryPath) -or
        (Test-Path -LiteralPath $resultPath)) {
        throw 'Aggregate output file이 이미 존재합니다.'
    }
    Write-DxaNetworkLoadUtf8 `
        -Path $summaryPath `
        -Contents ($aggregate | ConvertTo-Json -Depth 8)
    $resultLines = @(
        '# Network load aggregate',
        '',
        "- commit: $ExpectedCommitSha",
        "- matches: $($aggregate.match_count)",
        "- participants per match: $ExpectedParticipantCount",
        "- server tick P95 ms: $($aggregate.metrics.server_tick_p95_ms)",
        "- participant average received KiB/s: $($aggregate.metrics.participant_average_received_kib_per_second)",
        "- recipient received KiB/s P95: $($aggregate.metrics.recipient_received_kib_per_second_p95)",
        '',
        'This report is computed from child raw evidence.'
    )
    Write-DxaNetworkLoadUtf8 `
        -Path $resultPath `
        -Contents (($resultLines -join "`n") + "`n")
    return $aggregate
}
