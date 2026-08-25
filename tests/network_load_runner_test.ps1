[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$RepositoryRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $RepositoryRoot 'scripts/network_load_common.ps1')

$temporaryRoot = Join-Path (
    [IO.Path]::GetTempPath()) (
    'dxa-network-load-runner-' + [Guid]::NewGuid().ToString('N'))
$resolvedTemporaryRoot = [IO.Path]::GetFullPath($temporaryRoot)
$resolvedSystemTemporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
if (-not $resolvedTemporaryRoot.StartsWith(
        $resolvedSystemTemporaryRoot,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Network load 임시 테스트 경로가 시스템 temp 밖입니다.'
}

function Assert-GuardRejected {
    param(
        [Parameter(Mandatory)]
        [scriptblock]$Invocation,

        [Parameter(Mandatory)]
        [string]$Name,

        [Parameter(Mandatory)]
        [string]$MarkerPath
    )

    if (Test-Path -LiteralPath $MarkerPath) {
        Remove-Item -LiteralPath $MarkerPath -Force
    }
    $rejected = $false
    try {
        & $Invocation
    }
    catch {
        $rejected = $true
    }
    if (-not $rejected) {
        throw "$Name guard가 요청을 거부하지 않았습니다."
    }
    if (Test-Path -LiteralPath $MarkerPath) {
        throw "$Name guard가 process action 이후에 실패했습니다."
    }
}

function New-FakeNetworkLoadMatch {
    param(
        [string]$ParentDirectory,
        [int]$Ordinal,
        [uint32]$Seed,
        [string]$CommitSha,
        [int]$ExitCode = 0,
        [switch]$MonotonicWorkingSet
    )

    $matchDirectory = Join-Path $ParentDirectory ('match-{0:D3}' -f $Ordinal)
    New-Item -ItemType Directory -Path $matchDirectory -Force | Out-Null
    $metadata = [ordered]@{
        schema_version = 2
        commit_sha = $CommitSha
        seed = $Seed
        participant_count = 24
        match_id = $Ordinal
        exit_code = $ExitCode
        protocol_errors = 0
        shaped_queue_overflows = 0
        secret_leak_count = 0
    }
    Write-DxaNetworkLoadUtf8 `
        -Path (Join-Path $matchDirectory 'match.json') `
        -Contents ($metadata | ConvertTo-Json -Depth 4)

    $tickRows = 1..20 | ForEach-Object {
        [pscustomobject]@{
            match_id = $Ordinal
            sample_index = $_ - 1
            duration_ns = [uint64]($_ * 1000000)
        }
    }
    Write-DxaNetworkLoadUtf8 `
        -Path (Join-Path $matchDirectory 'server-ticks.csv') `
        -Contents ((@($tickRows | ConvertTo-Csv -NoTypeInformation) -join "`n") + "`n")

    $clientRows = 1..24 | ForEach-Object {
        [pscustomobject]@{
            match_id = $Ordinal
            player_id = $_
            game_received_bytes = [uint64]($_ * 1024)
            measurement_seconds = 2.0
            exit_code = 0
            protocol_errors = 0
            shaped_queue_overflows = 0
        }
    }
    Write-DxaNetworkLoadUtf8 `
        -Path (Join-Path $matchDirectory 'clients.csv') `
        -Contents ((@($clientRows | ConvertTo-Csv -NoTypeInformation) -join "`n") + "`n")

    $workingSetPath = Join-Path $matchDirectory 'working-set.csv'
    if ($Ordinal -eq 1) {
        $lastWorkingSet = if ($MonotonicWorkingSet) { 120 } else { 90 }
        $workingSetRows = @(
            [pscustomobject]@{
                timestamp_utc = '2026-08-26T00:00:00Z'
                process = 'game_server'
                working_set_bytes = 100
            },
            [pscustomobject]@{
                timestamp_utc = '2026-08-26T00:00:10Z'
                process = 'game_server'
                working_set_bytes = 110
            },
            [pscustomobject]@{
                timestamp_utc = '2026-08-26T00:00:20Z'
                process = 'game_server'
                working_set_bytes = $lastWorkingSet
            })
        Write-DxaNetworkLoadUtf8 `
            -Path $workingSetPath `
            -Contents ((@($workingSetRows | ConvertTo-Csv -NoTypeInformation) -join "`n") + "`n")
    }
    else {
        Write-DxaNetworkLoadUtf8 `
            -Path $workingSetPath `
            -Contents "timestamp_utc,process,working_set_bytes`n"
    }
    return $matchDirectory
}

function Assert-AggregateRejectedWithoutResult {
    param(
        [scriptblock]$Invocation,
        [string]$ParentDirectory,
        [string]$Name
    )

    $resultPath = Join-Path $ParentDirectory 'RESULT.md'
    if (Test-Path -LiteralPath $resultPath) {
        Remove-Item -LiteralPath $resultPath -Force
    }
    $rejected = $false
    try {
        & $Invocation
    }
    catch {
        $rejected = $true
    }
    if (-not $rejected) {
        throw "$Name aggregate가 요청을 거부하지 않았습니다."
    }
    if (Test-Path -LiteralPath $resultPath) {
        throw "$Name aggregate가 실패 뒤 RESULT.md를 남겼습니다."
    }
}

New-Item -ItemType Directory -Path $resolvedTemporaryRoot | Out-Null
try {
    & git -C $resolvedTemporaryRoot init --quiet
    & git -C $resolvedTemporaryRoot config user.name 'DXA Test'
    & git -C $resolvedTemporaryRoot config user.email 'dxa-test@example.invalid'
    Write-DxaNetworkLoadUtf8 `
        -Path (Join-Path $resolvedTemporaryRoot 'tracked.txt') `
        -Contents 'baseline'
    & git -C $resolvedTemporaryRoot add tracked.txt
    & git -C $resolvedTemporaryRoot commit --quiet -m 'network load baseline'
    if ($LASTEXITCODE -ne 0) {
        throw 'Network load 임시 Git 저장소 준비가 실패했습니다.'
    }

    $snapshot = Get-DxaNetworkLoadGitSnapshot -RepositoryRoot $resolvedTemporaryRoot
    $marker = Join-Path $resolvedTemporaryRoot 'process-started.marker'
    $output = Join-Path $resolvedTemporaryRoot 'evidence/run-001'
    $action = {
        Write-DxaNetworkLoadUtf8 -Path $marker -Contents 'started'
    }

    Assert-GuardRejected -Name 'missing SHA' -MarkerPath $marker -Invocation {
        Invoke-DxaNetworkLoadGuarded `
            -RepositoryRoot $resolvedTemporaryRoot `
            -CommitSha '' `
            -OutputDirectory $output `
            -ReplicationMode full-state `
            -Matches 1 `
            -Seeds @(20260825) `
            -BotCount 23 `
            -Action $action
    }

    [IO.File]::AppendAllText(
        (Join-Path $resolvedTemporaryRoot 'tracked.txt'),
        'dirty',
        [Text.UTF8Encoding]::new($false))
    Assert-GuardRejected -Name 'dirty tree' -MarkerPath $marker -Invocation {
        Invoke-DxaNetworkLoadGuarded `
            -RepositoryRoot $resolvedTemporaryRoot `
            -CommitSha $snapshot.commit_sha `
            -OutputDirectory $output `
            -ReplicationMode full-state `
            -Matches 1 `
            -Seeds @(20260825) `
            -BotCount 23 `
            -Action $action
    }
    & git -C $resolvedTemporaryRoot add tracked.txt
    & git -C $resolvedTemporaryRoot commit --quiet -m 'clean again'
    $snapshot = Get-DxaNetworkLoadGitSnapshot -RepositoryRoot $resolvedTemporaryRoot

    New-Item -ItemType Directory -Path $output -Force | Out-Null
    Assert-GuardRejected -Name 'reused output' -MarkerPath $marker -Invocation {
        Invoke-DxaNetworkLoadGuarded `
            -RepositoryRoot $resolvedTemporaryRoot `
            -CommitSha $snapshot.commit_sha `
            -OutputDirectory $output `
            -ReplicationMode full-state `
            -Matches 1 `
            -Seeds @(20260825) `
            -BotCount 23 `
            -Action $action
    }
    Remove-Item -LiteralPath $output -Recurse -Force

    Assert-GuardRejected -Name 'bot count' -MarkerPath $marker -Invocation {
        Invoke-DxaNetworkLoadGuarded `
            -RepositoryRoot $resolvedTemporaryRoot `
            -CommitSha $snapshot.commit_sha `
            -OutputDirectory $output `
            -ReplicationMode full-state `
            -Matches 1 `
            -Seeds @(20260825) `
            -BotCount 22 `
            -Action $action
    }

    $valid = Invoke-DxaNetworkLoadGuarded `
        -RepositoryRoot $resolvedTemporaryRoot `
        -CommitSha $snapshot.commit_sha `
        -OutputDirectory $output `
        -ReplicationMode full-state `
        -Matches 1 `
        -Seeds @(20260825) `
        -BotCount 23 `
        -Action $action
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) {
        throw '유효한 Network load 요청이 process action을 실행하지 않았습니다.'
    }
    if ($valid.git.commit_sha -ne $snapshot.commit_sha) {
        throw '유효한 Network load guard가 다른 commit을 반환했습니다.'
    }
    Remove-Item -LiteralPath $marker -Force
    foreach ($mode in @(
            'full-state',
            'interest-full',
            'interest-quantized',
            'interest-delta')) {
        $modeSnapshot = Assert-DxaNetworkLoadRequest `
            -RepositoryRoot $resolvedTemporaryRoot `
            -CommitSha $snapshot.commit_sha `
            -OutputDirectory $output `
            -ReplicationMode $mode `
            -Matches 1 `
            -Seeds @(20260825) `
            -BotCount 23
        if ($modeSnapshot.commit_sha -ne $snapshot.commit_sha) {
            throw "Network load mode guard가 실패했습니다: $mode"
        }
    }
    $soakSnapshot = Assert-DxaNetworkLoadRequest `
        -RepositoryRoot $resolvedTemporaryRoot `
        -CommitSha $snapshot.commit_sha `
        -OutputDirectory $output `
        -ReplicationMode interest-delta `
        -Matches 3 `
        -Seeds @(20260825, 20260826, 20260827) `
        -BotCount 23 `
        -SoakMinutes 30
    if ($soakSnapshot.commit_sha -ne $snapshot.commit_sha) {
        throw 'Network load soak guard가 실패했습니다.'
    }
    $priorEvidence = Join-Path $resolvedTemporaryRoot 'evidence/prior-run/raw.csv'
    New-Item -ItemType Directory -Path (Split-Path -Parent $priorEvidence) |
        Out-Null
    Write-DxaNetworkLoadUtf8 -Path $priorEvidence -Contents "value`n1`n"
    $evidenceOnlySnapshot = Assert-DxaNetworkLoadRequest `
        -RepositoryRoot $resolvedTemporaryRoot `
        -CommitSha $snapshot.commit_sha `
        -OutputDirectory (Join-Path $resolvedTemporaryRoot 'evidence/run-new') `
        -ReplicationMode interest-delta `
        -Matches 1 `
        -Seeds @(20260825) `
        -BotCount 23
    if (-not $evidenceOnlySnapshot.evidence_only_changes) {
        throw 'Network load guard가 기존 untracked evidence를 구분하지 못했습니다.'
    }
    Remove-Item -LiteralPath (Join-Path $resolvedTemporaryRoot 'evidence') `
        -Recurse `
        -Force
    $p95 = Get-DxaNearestRankP95 -Values ([double[]](1..20))
    if ($p95 -ne 19.0) {
        throw "Network load nearest-rank P95가 잘못됐습니다: $p95"
    }
    $quoted = ConvertTo-DxaProcessArgumentString -Arguments @(
        'alpha',
        'path with space',
        'quote"value')
    if ($quoted -ne 'alpha "path with space" "quote\"value"') {
        throw "Network load process argument quoting이 잘못됐습니다: $quoted"
    }
    $exitStdout = Join-Path $resolvedTemporaryRoot 'exit.stdout.log'
    $exitStderr = Join-Path $resolvedTemporaryRoot 'exit.stderr.log'
    $exitProcess = Start-DxaLoggedProcess `
        -FilePath $env:ComSpec `
        -Arguments @('/c', 'exit', '7') `
        -StdoutPath $exitStdout `
        -StderrPath $exitStderr
    Wait-DxaNetworkLoadProcesses -Processes @($exitProcess) -TimeoutSeconds 5
    if ($null -eq $exitProcess.ExitCode -or $exitProcess.ExitCode -ne 7) {
        throw "Redirected process ExitCode가 확정되지 않았습니다: $($exitProcess.ExitCode)"
    }
    $currentWorkingSet = Get-DxaProcessTreeWorkingSetBytes `
        -Process ([Diagnostics.Process]::GetCurrentProcess())
    if ($currentWorkingSet -eq 0) {
        throw 'Network load process tree working set 표본이 비어 있습니다.'
    }
    if ((Get-DxaNetworkLoadMatchSeed -MatchId 1) -ne 20260825 -or
        (Get-DxaNetworkLoadMatchSeed -MatchId ([uint64]4294967297)) -ne
            20260824) {
        throw 'Network load MatchId seed 계산이 잘못됐습니다.'
    }
    $privateRoot = 'C:\Users\private-user\repo'
    $privateRun = Join-Path $privateRoot 'docs/run-001'
    $sanitized = ConvertTo-DxaEvidenceCommandText `
        -CommandLines @(
            "$privateRoot\client.exe --output $privateRun") `
        -RepositoryRoot $privateRoot `
        -RunDirectory $privateRun
    if ($sanitized -ne
            '<REPOSITORY_ROOT>\client.exe --output <RUN_DIRECTORY>' -or
        $sanitized.Contains('private-user')) {
        throw 'Network load command path 개인정보 치환이 실패했습니다.'
    }

    $aggregateRoot = Join-Path $resolvedTemporaryRoot 'aggregate'
    New-Item -ItemType Directory -Path $aggregateRoot | Out-Null
    1..3 | ForEach-Object {
        New-FakeNetworkLoadMatch `
            -ParentDirectory $aggregateRoot `
            -Ordinal $_ `
            -Seed ([uint32](100 + $_)) `
            -CommitSha $snapshot.commit_sha | Out-Null
    }
    $aggregate = Get-DxaNetworkLoadAggregate `
        -ParentDirectory $aggregateRoot `
        -ExpectedCommitSha $snapshot.commit_sha `
        -ExpectedSeeds ([uint32[]]@(101, 102, 103)) `
        -ExpectedParticipantCount 24 `
        -WorkingSetWindowSeconds 900
    if ($aggregate.raw.tick_sample_count -ne 60 -or
        $aggregate.raw.client_row_count -ne 72 -or
        $aggregate.raw.game_received_bytes -ne 921600) {
        throw 'Network load aggregate raw count 합계가 잘못됐습니다.'
    }
    if ($aggregate.metrics.server_tick_p95_ms -ne 19.0 -or
        $aggregate.metrics.participant_average_received_kib_per_second -ne 6.25 -or
        $aggregate.metrics.recipient_received_kib_per_second_p95 -ne 11.5) {
        throw 'Network load aggregate P95 또는 KiB/s 계산이 잘못됐습니다.'
    }
    if ($aggregate.guards.monotonic_working_set_increase) {
        throw 'Network load aggregate가 감소하는 working set을 누수로 판단했습니다.'
    }

    Assert-AggregateRejectedWithoutResult `
        -Name 'commit SHA mismatch' `
        -ParentDirectory $aggregateRoot `
        -Invocation {
            Get-DxaNetworkLoadAggregate `
                -ParentDirectory $aggregateRoot `
                -ExpectedCommitSha ('0' * 40) `
                -ExpectedSeeds ([uint32[]]@(101, 102, 103)) `
                -ExpectedParticipantCount 24 | Out-Null
        }
    Assert-AggregateRejectedWithoutResult `
        -Name 'seed count mismatch' `
        -ParentDirectory $aggregateRoot `
        -Invocation {
            Get-DxaNetworkLoadAggregate `
                -ParentDirectory $aggregateRoot `
                -ExpectedCommitSha $snapshot.commit_sha `
                -ExpectedSeeds ([uint32[]]@(101, 102)) `
                -ExpectedParticipantCount 24 | Out-Null
        }
    Assert-AggregateRejectedWithoutResult `
        -Name 'participant mismatch' `
        -ParentDirectory $aggregateRoot `
        -Invocation {
            Get-DxaNetworkLoadAggregate `
                -ParentDirectory $aggregateRoot `
                -ExpectedCommitSha $snapshot.commit_sha `
                -ExpectedSeeds ([uint32[]]@(101, 102, 103)) `
                -ExpectedParticipantCount 23 | Out-Null
        }

    Write-DxaNetworkLoadAggregate `
        -ParentDirectory $aggregateRoot `
        -ExpectedCommitSha $snapshot.commit_sha `
        -ExpectedSeeds ([uint32[]]@(101, 102, 103)) `
        -ExpectedParticipantCount 24 `
        -WorkingSetWindowSeconds 900 | Out-Null
    if (-not (Test-Path -LiteralPath (Join-Path $aggregateRoot 'RESULT.md'))) {
        throw 'Network load aggregate가 유효한 RESULT.md를 만들지 않았습니다.'
    }

    $failedMatchPath = Join-Path $aggregateRoot 'match-002/match.json'
    $failedMatch = Get-Content -Raw -LiteralPath $failedMatchPath | ConvertFrom-Json
    $failedMatch.exit_code = 7
    Write-DxaNetworkLoadUtf8 `
        -Path $failedMatchPath `
        -Contents ($failedMatch | ConvertTo-Json -Depth 4)
    Assert-AggregateRejectedWithoutResult `
        -Name 'nonzero exit' `
        -ParentDirectory $aggregateRoot `
        -Invocation {
            Write-DxaNetworkLoadAggregate `
                -ParentDirectory $aggregateRoot `
                -ExpectedCommitSha $snapshot.commit_sha `
                -ExpectedSeeds ([uint32[]]@(101, 102, 103)) `
                -ExpectedParticipantCount 24 | Out-Null
        }
    $failedMatch.exit_code = 0
    Write-DxaNetworkLoadUtf8 `
        -Path $failedMatchPath `
        -Contents ($failedMatch | ConvertTo-Json -Depth 4)

    Remove-Item -LiteralPath (Join-Path $aggregateRoot 'match-001') -Recurse -Force
    New-FakeNetworkLoadMatch `
        -ParentDirectory $aggregateRoot `
        -Ordinal 1 `
        -Seed 101 `
        -CommitSha $snapshot.commit_sha `
        -MonotonicWorkingSet | Out-Null
    $leakAggregate = Get-DxaNetworkLoadAggregate `
        -ParentDirectory $aggregateRoot `
        -ExpectedCommitSha $snapshot.commit_sha `
        -ExpectedSeeds ([uint32[]]@(101, 102, 103)) `
        -ExpectedParticipantCount 24 `
        -WorkingSetWindowSeconds 900
    if (-not $leakAggregate.guards.monotonic_working_set_increase) {
        throw 'Network load aggregate가 단조 working set 증가를 감지하지 못했습니다.'
    }
    Assert-AggregateRejectedWithoutResult `
        -Name 'monotonic working set' `
        -ParentDirectory $aggregateRoot `
        -Invocation {
            Write-DxaNetworkLoadAggregate `
                -ParentDirectory $aggregateRoot `
                -ExpectedCommitSha $snapshot.commit_sha `
                -ExpectedSeeds ([uint32[]]@(101, 102, 103)) `
                -ExpectedParticipantCount 24 `
                -WorkingSetWindowSeconds 900 | Out-Null
        }

    $runnerText = Get-Content `
        -Raw `
        -Encoding utf8 `
        -LiteralPath (Join-Path $RepositoryRoot 'scripts/run_network_load.ps1')
    foreach ($requiredRunnerMarker in @(
            "'interest-full'",
            "'interest-quantized'",
            "'interest-delta'",
            "'--udp-loss-basis-points', '200'",
            'SoakMinutes',
            "'match-{0:D3}'",
            'measurement_seconds',
            'docker-asan-command.txt')) {
        if (-not $runnerText.Contains($requiredRunnerMarker)) {
            throw "Network load runner marker가 없습니다: $requiredRunnerMarker"
        }
    }
    if ([regex]::Matches($runnerText, "'--replication-mode'").Count -lt 2) {
        throw 'Network load runner가 server와 DX11 client mode를 함께 고정하지 않았습니다.'
    }

    $serverExecutable = Join-Path $RepositoryRoot (
        'out/build/windows-msvc-vs-debug/apps/game_server/Debug/' +
        'dxa_game_server.exe')
    if (-not (Test-Path -LiteralPath $serverExecutable -PathType Leaf)) {
        throw "Game server metrics smoke binary가 없습니다: $serverExecutable"
    }
    $metricsDirectory = Join-Path $resolvedTemporaryRoot 'metrics-smoke'
    New-Item -ItemType Directory -Path $metricsDirectory | Out-Null
    $tcpListener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
    $tcpListener.Start()
    $controlPort = [uint16]$tcpListener.LocalEndpoint.Port
    $tcpListener.Stop()
    do {
        $tcpListener = [Net.Sockets.TcpListener]::new(
            [Net.IPAddress]::Loopback,
            0)
        $tcpListener.Start()
        $gameTcpPort = [uint16]$tcpListener.LocalEndpoint.Port
        $tcpListener.Stop()
    } while ($gameTcpPort -eq $controlPort)
    $udp = [Net.Sockets.UdpClient]::new(0)
    $gameUdpPort = [uint16]$udp.Client.LocalEndPoint.Port
    $udp.Dispose()
    $serverArguments = @(
        '--lobby-control-port', $controlPort,
        '--game-tcp-port', $gameTcpPort,
        '--game-udp-port', $gameUdpPort,
        '--replication-mode', 'full-state',
        '--metrics-output-root', $metricsDirectory)
    $serverStdout = Join-Path $resolvedTemporaryRoot 'server.stdout.log'
    $serverStderr = Join-Path $resolvedTemporaryRoot 'server.stderr.log'
    $server = Start-DxaLoggedProcess `
        -FilePath $serverExecutable `
        -Arguments $serverArguments `
        -StdoutPath $serverStdout `
        -StderrPath $serverStderr
    try {
        $deadline = [DateTimeOffset]::UtcNow.AddSeconds(5)
        $tickPath = Join-Path $metricsDirectory 'server-ticks.csv'
        $replicationPath = Join-Path $metricsDirectory 'replication.csv'
        while ([DateTimeOffset]::UtcNow -lt $deadline -and
            (-not (Test-Path -LiteralPath $tickPath -PathType Leaf) -or
             -not (Test-Path -LiteralPath $replicationPath -PathType Leaf))) {
            $server.Refresh()
            if ($server.HasExited) {
                throw "Game server metrics smoke가 일찍 종료됐습니다: $($server.ExitCode)"
            }
            Start-Sleep -Milliseconds 50
        }
        if (-not (Test-Path -LiteralPath $tickPath -PathType Leaf) -or
            -not (Test-Path -LiteralPath $replicationPath -PathType Leaf)) {
            throw 'Game server metrics CSV header가 생성되지 않았습니다.'
        }
        $tickHeader = Get-Content -Encoding utf8 -TotalCount 1 -LiteralPath $tickPath
        $replicationHeader = Get-Content `
            -Encoding utf8 `
            -TotalCount 1 `
            -LiteralPath $replicationPath
        if ($tickHeader -notmatch '^match_id,sample_index,duration_ns' -or
            $tickHeader -notmatch 'shaped_queue_overflows$' -or
            $replicationHeader -notmatch '^match_id,sample_index,encode_duration_ns') {
            throw 'Game server metrics CSV header가 예상과 다릅니다.'
        }
    }
    finally {
        $server.Refresh()
        if (-not $server.HasExited) {
            Stop-DxaProcessTreeIfRunning -Process $server
        }
    }

    $duplicate = Start-DxaLoggedProcess `
        -FilePath $serverExecutable `
        -Arguments $serverArguments `
        -StdoutPath (Join-Path $resolvedTemporaryRoot 'duplicate.stdout.log') `
        -StderrPath (Join-Path $resolvedTemporaryRoot 'duplicate.stderr.log')
    if (-not $duplicate.WaitForExit(5000)) {
        Stop-DxaProcessTreeIfRunning -Process $duplicate
        throw '기존 metrics CSV를 사용한 server가 종료되지 않았습니다.'
    }
    if ($duplicate.ExitCode -eq 0) {
        throw 'Game server가 기존 metrics CSV 덮어쓰기를 거부하지 않았습니다.'
    }
    $duplicateError = Get-Content `
        -Raw `
        -ErrorAction SilentlyContinue `
        -LiteralPath (Join-Path $resolvedTemporaryRoot 'duplicate.stderr.log')
    if ($duplicateError -notmatch 'metrics output files already exist') {
        throw 'Game server duplicate smoke가 metrics overwrite 경계에서 실패하지 않았습니다.'
    }
}
finally {
    if (Test-Path -LiteralPath $resolvedTemporaryRoot) {
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
