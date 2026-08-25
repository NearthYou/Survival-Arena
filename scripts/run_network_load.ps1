[CmdletBinding()]
param(
    [ValidateSet('full-state')]
    [string]$ReplicationMode = 'full-state',

    [ValidateRange(1, 3)]
    [int]$Matches = 1,

    [uint32[]]$Seeds = @(20260825),

    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$CommitSha,

    [switch]$Impairment,
    [switch]$Release,

    [ValidateNotNullOrEmpty()]
    [string]$OutputRoot = 'docs/benchmarks/network-load'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'network_load_common.ps1')

function Get-FreeTcpPort {
    $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
    try {
        $listener.Start()
        return [uint16]$listener.LocalEndpoint.Port
    }
    finally {
        $listener.Stop()
    }
}

function Get-FreeUdpPort {
    $client = [Net.Sockets.UdpClient]::new(0)
    try {
        return [uint16]$client.Client.LocalEndPoint.Port
    }
    finally {
        $client.Dispose()
    }
}

function Read-ProcessLog {
    param([string]$StdoutPath, [string]$StderrPath)

    $stdout = if (Test-Path -LiteralPath $StdoutPath) {
        Get-Content -Raw -ErrorAction SilentlyContinue -LiteralPath $StdoutPath
    }
    else { '' }
    $stderr = if (Test-Path -LiteralPath $StderrPath) {
        Get-Content -Raw -ErrorAction SilentlyContinue -LiteralPath $StderrPath
    }
    else { '' }
    return ([string]$stdout + "`n" + [string]$stderr)
}

function Wait-LogPattern {
    param(
        [Diagnostics.Process]$Process,
        [string]$StdoutPath,
        [string]$StderrPath,
        [string]$Pattern,
        [int]$TimeoutSeconds
    )

    $deadline = [DateTimeOffset]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTimeOffset]::UtcNow -lt $deadline) {
        $text = Read-ProcessLog -StdoutPath $StdoutPath -StderrPath $StderrPath
        $match = [regex]::Match($text, $Pattern)
        if ($match.Success) {
            return $match
        }
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "Process가 예상 log 전에 종료됐습니다. exit=$($Process.ExitCode) pattern=$Pattern"
        }
        Start-Sleep -Milliseconds 100
    }
    throw "Process log 대기가 시간 초과됐습니다. pattern=$Pattern"
}

function Get-DeterministicSecretPatterns {
    $patterns = [Collections.Generic.List[string]]::new()
    foreach ($start in @(1, 0x41)) {
        for ($participant = 0; $participant -lt 24; ++$participant) {
            $bytes = for ($index = 0; $index -lt 16; ++$index) {
                '{0:x2}' -f (($start + $participant + $index) -band 0xff)
            }
            $patterns.Add(($bytes -join ''))
        }
    }
    return $patterns
}

if ($Impairment) {
    throw 'UDP impairment는 Task 16 전에는 실행할 수 없습니다.'
}
if ($Seeds.Count -ne $Matches) {
    throw 'Seeds 수는 Matches와 같아야 합니다.'
}
$matchSeedBase = [uint32]20260824
for ($index = 0; $index -lt $Matches; ++$index) {
    $expectedSeed = $matchSeedBase -bxor [uint32]($index + 1)
    if ($Seeds[$index] -ne $expectedSeed) {
        throw "현재 lobby seed 순서와 요청 seed가 다릅니다. index=$index expected=$expectedSeed actual=$($Seeds[$index])"
    }
}

$shortSha = if ($CommitSha.Length -ge 8) { $CommitSha.Substring(0, 8) } else { $CommitSha }
$runId = '{0}-{1}-{2}' -f (
    Get-Date -Format 'yyyyMMdd-HHmmss'), $shortSha, $ReplicationMode
$resolvedOutputRoot = if ([IO.Path]::IsPathRooted($OutputRoot)) {
    [IO.Path]::GetFullPath($OutputRoot)
}
else {
    [IO.Path]::GetFullPath((Join-Path $repositoryRoot $OutputRoot))
}
$outputDirectory = Join-Path $resolvedOutputRoot $runId
$guard = Invoke-DxaNetworkLoadGuarded `
    -RepositoryRoot $repositoryRoot `
    -CommitSha $CommitSha `
    -OutputDirectory $outputDirectory `
    -ReplicationMode $ReplicationMode `
    -Matches $Matches `
    -Seeds $Seeds `
    -BotCount 23 `
    -Action {
        New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
        return $outputDirectory
    }

$configuration = if ($Release) { 'Release' } else { 'Debug' }
$presetSegment = if ($Release) { 'windows-msvc-vs-release' } else { 'windows-msvc-vs-debug' }
$buildRoot = Join-Path $repositoryRoot ("out/build/$presetSegment")
$lobbyExecutable = Join-Path $buildRoot "apps/lobby_server/$configuration/dxa_lobby_server.exe"
$gameExecutable = Join-Path $buildRoot "apps/game_server/$configuration/dxa_game_server.exe"
$clientExecutable = Join-Path $buildRoot "apps/client/$configuration/dxa_client.exe"
$botExecutable = Join-Path $buildRoot "apps/bot_client/$configuration/dxa_bot_client.exe"
foreach ($executable in @($lobbyExecutable, $gameExecutable, $clientExecutable, $botExecutable)) {
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Network load binary가 없습니다: $executable"
    }
}

$lobbyPort = Get-FreeTcpPort
do { $workerPort = Get-FreeTcpPort } while ($workerPort -eq $lobbyPort)
do {
    $gameTcpPort = Get-FreeTcpPort
} while ($gameTcpPort -eq $lobbyPort -or $gameTcpPort -eq $workerPort)
$gameUdpPort = Get-FreeUdpPort
$commandLines = [Collections.Generic.List[string]]::new()
$clientRows = [Collections.Generic.List[object]]::new()
$matchResults = [Collections.Generic.List[object]]::new()
$selectedAdapter = $null
$lobbyProcess = $null
$gameProcess = $null
$clientProcess = $null
$botProcess = $null
$startedAt = [DateTimeOffset]::UtcNow

try {
    $lobbyStdout = Join-Path $outputDirectory 'lobby.stdout.log'
    $lobbyStderr = Join-Path $outputDirectory 'lobby.stderr.log'
    $lobbyArguments = @(
        '--bind', '127.0.0.1',
        '--port', $lobbyPort,
        '--worker-bind', '127.0.0.1',
        '--worker-port', $workerPort)
    $commandLines.Add("$lobbyExecutable $(ConvertTo-DxaProcessArgumentString $lobbyArguments)")
    $lobbyProcess = Start-DxaLoggedProcess `
        -FilePath $lobbyExecutable `
        -Arguments $lobbyArguments `
        -StdoutPath $lobbyStdout `
        -StderrPath $lobbyStderr
    Wait-LogPattern `
        -Process $lobbyProcess `
        -StdoutPath $lobbyStdout `
        -StderrPath $lobbyStderr `
        -Pattern 'lobby_server_listening' `
        -TimeoutSeconds 10 | Out-Null

    $gameStdout = Join-Path $outputDirectory 'game.stdout.log'
    $gameStderr = Join-Path $outputDirectory 'game.stderr.log'
    $gameArguments = @(
        '--lobby-control-host', '127.0.0.1',
        '--lobby-control-port', $workerPort,
        '--advertise-host', '127.0.0.1',
        '--game-bind', '127.0.0.1',
        '--game-tcp-port', $gameTcpPort,
        '--game-udp-port', $gameUdpPort,
        '--replication-mode', $ReplicationMode,
        '--metrics-output-root', $outputDirectory)
    $commandLines.Add("$gameExecutable $(ConvertTo-DxaProcessArgumentString $gameArguments)")
    $gameProcess = Start-DxaLoggedProcess `
        -FilePath $gameExecutable `
        -Arguments $gameArguments `
        -StdoutPath $gameStdout `
        -StderrPath $gameStderr
    Wait-LogPattern `
        -Process $gameProcess `
        -StdoutPath $gameStdout `
        -StderrPath $gameStderr `
        -Pattern 'game_server_listening' `
        -TimeoutSeconds 10 | Out-Null

    for ($matchIndex = 0; $matchIndex -lt $Matches; ++$matchIndex) {
        $ordinal = $matchIndex + 1
        $clientStdout = Join-Path $outputDirectory "client-$ordinal.stdout.log"
        $clientStderr = Join-Path $outputDirectory "client-$ordinal.stderr.log"
        $clientArguments = @(
            '--warp',
            '--hidden',
            '--verify-render',
            '--render-path', 'hybrid-deferred',
            '--network-create',
            '--expected-players', '24',
            '--exit-on-match-result',
            '--lobby-host', '127.0.0.1',
            '--lobby-port', $lobbyPort,
            '--width', '1920',
            '--height', '1080')
        $commandLines.Add("$clientExecutable $(ConvertTo-DxaProcessArgumentString $clientArguments)")
        $clientProcess = Start-DxaLoggedProcess `
            -FilePath $clientExecutable `
            -Arguments $clientArguments `
            -StdoutPath $clientStdout `
            -StderrPath $clientStderr

        $roomMatch = Wait-LogPattern `
            -Process $clientProcess `
            -StdoutPath $clientStdout `
            -StderrPath $clientStderr `
            -Pattern 'network room=(\d+) state=waiting' `
            -TimeoutSeconds 60
        $roomId = [uint32]$roomMatch.Groups[1].Value

        $botStdout = Join-Path $outputDirectory "bot-$ordinal.stdout.log"
        $botStderr = Join-Path $outputDirectory "bot-$ordinal.stderr.log"
        $botArguments = @(
            '--host', '127.0.0.1',
            '--port', $lobbyPort,
            '--room', $roomId,
            '--count', '23',
            '--play')
        $commandLines.Add("$botExecutable $(ConvertTo-DxaProcessArgumentString $botArguments)")
        $botProcess = Start-DxaLoggedProcess `
            -FilePath $botExecutable `
            -Arguments $botArguments `
            -StdoutPath $botStdout `
            -StderrPath $botStderr

        Wait-DxaNetworkLoadProcesses `
            -Processes @($botProcess, $clientProcess) `
            -TimeoutSeconds 660
        $botProcess.Refresh()
        $clientProcess.Refresh()
        if ($botProcess.ExitCode -ne 0 -or $clientProcess.ExitCode -ne 0) {
            throw "Network load client 실패: client=$($clientProcess.ExitCode) bot=$($botProcess.ExitCode)"
        }

        $clientText = Read-ProcessLog -StdoutPath $clientStdout -StderrPath $clientStderr
        $botText = Read-ProcessLog -StdoutPath $botStdout -StderrPath $botStderr
        $adapterMatch = [regex]::Match($clientText, 'graphics adapter=([^\r\n]+)')
        if (-not $adapterMatch.Success) {
            throw 'Network load DX11 adapter log를 찾지 못했습니다.'
        }
        $currentAdapter = $adapterMatch.Groups[1].Value.Trim()
        if ($null -eq $selectedAdapter) {
            $selectedAdapter = $currentAdapter
        }
        elseif ($selectedAdapter -ne $currentAdapter) {
            throw 'Network load match 사이에 DX11 adapter가 바뀌었습니다.'
        }
        $clientResult = [regex]::Match(
            $clientText,
            'network match=(\d+) player=(\d+) state=finished winner=(\d+|none) tick=(\d+) reason=(\d+) snapshots_applied=(\d+) tcp_received_bytes=(\d+) udp_received_bytes=(\d+)')
        $botResult = [regex]::Match(
            $botText,
            'bot result match=(\d+) winner=(\d+|none) reason=(\d+) tick=(\d+) sessions=(\d+) exit=(\d+)')
        if (-not $clientResult.Success -or -not $botResult.Success) {
            throw 'Network load result log를 해석하지 못했습니다.'
        }
        if ([uint64]$clientResult.Groups[6].Value -lt 2 -or
            [uint64]$clientResult.Groups[7].Value -eq 0 -or
            [uint64]$clientResult.Groups[8].Value -eq 0) {
            throw 'Network load DX11 client metric이 유효하지 않습니다.'
        }
        if ($clientResult.Groups[1].Value -ne $botResult.Groups[1].Value -or
            $clientResult.Groups[3].Value -ne $botResult.Groups[2].Value -or
            $clientResult.Groups[4].Value -ne $botResult.Groups[4].Value -or
            $clientResult.Groups[5].Value -ne $botResult.Groups[3].Value -or
            $botResult.Groups[5].Value -ne '23' -or
            $botResult.Groups[6].Value -ne '0') {
            throw 'Network load client와 bot result가 일치하지 않습니다.'
        }
        $matchId = [uint64]$clientResult.Groups[1].Value
        $actualSeed = $matchSeedBase `
            -bxor [uint32]($matchId -band [uint64]0xffffffff) `
            -bxor [uint32]($matchId -shr 32)
        if ($actualSeed -ne $Seeds[$matchIndex]) {
            throw 'Network load 실제 MatchId의 seed가 요청과 다릅니다.'
        }

        $clientRows.Add([pscustomobject]@{
            match_id = $matchId
            client_kind = 'dx11'
            player_id = [uint32]$clientResult.Groups[2].Value
            snapshots_applied = [uint64]$clientResult.Groups[6].Value
            tcp_received_bytes = [uint64]$clientResult.Groups[7].Value
            udp_received_bytes = [uint64]$clientResult.Groups[8].Value
            discarded_snapshots = 0
            keyframe_requests = 0
            exit_code = 0
        })
        $botSessions = [regex]::Matches(
            $botText,
            'bot session player=(\d+) match=(\d+) snapshots_applied=(\d+) tcp_received_bytes=(\d+) udp_received_bytes=(\d+) discarded_snapshots=(\d+) keyframe_requests=(\d+) exit=(\d+)')
        if ($botSessions.Count -ne 23) {
            throw "Network load bot session row 수가 23이 아닙니다: $($botSessions.Count)"
        }
        foreach ($session in $botSessions) {
            if ([uint64]$session.Groups[2].Value -ne $matchId -or
                [int]$session.Groups[8].Value -ne 0 -or
                [uint64]$session.Groups[3].Value -lt 2 -or
                [uint64]$session.Groups[4].Value -eq 0 -or
                [uint64]$session.Groups[5].Value -eq 0) {
                throw 'Network load bot session metric이 유효하지 않습니다.'
            }
            $clientRows.Add([pscustomobject]@{
                match_id = $matchId
                client_kind = 'bot'
                player_id = [uint32]$session.Groups[1].Value
                snapshots_applied = [uint64]$session.Groups[3].Value
                tcp_received_bytes = [uint64]$session.Groups[4].Value
                udp_received_bytes = [uint64]$session.Groups[5].Value
                discarded_snapshots = [uint64]$session.Groups[6].Value
                keyframe_requests = [uint64]$session.Groups[7].Value
                exit_code = [int]$session.Groups[8].Value
            })
        }
        $matchResults.Add([pscustomobject]@{
            match_id = $matchId
            room_id = $roomId
            seed = $Seeds[$matchIndex]
            winner = $clientResult.Groups[3].Value
            reason = [uint32]$clientResult.Groups[5].Value
            finished_tick = [uint32]$clientResult.Groups[4].Value
        })
        $botProcess = $null
        $clientProcess = $null
    }

    $tickPath = Join-Path $outputDirectory 'server-ticks.csv'
    $replicationPath = Join-Path $outputDirectory 'replication.csv'
    $metricsDeadline = [DateTimeOffset]::UtcNow.AddSeconds(30)
    while ([DateTimeOffset]::UtcNow -lt $metricsDeadline) {
        if ((Test-Path -LiteralPath $tickPath -PathType Leaf) -and
            (Test-Path -LiteralPath $replicationPath -PathType Leaf)) {
            $tickRows = @(Import-Csv -LiteralPath $tickPath)
            $replicationRows = @(Import-Csv -LiteralPath $replicationPath)
            $exportedMatches = @($tickRows.match_id | Sort-Object -Unique)
            if ($exportedMatches.Count -eq $Matches -and $replicationRows.Count -gt 0) {
                break
            }
        }
        Start-Sleep -Milliseconds 100
    }
    $tickRows = @(Import-Csv -LiteralPath $tickPath)
    $replicationRows = @(Import-Csv -LiteralPath $replicationPath)
    if (@($tickRows.match_id | Sort-Object -Unique).Count -ne $Matches -or
        $replicationRows.Count -eq 0) {
        throw 'Network load server metrics가 모두 flush되지 않았습니다.'
    }
    if ($clientRows.Count -ne $Matches * 24) {
        throw "Network load client row 수가 예상과 다릅니다: $($clientRows.Count)"
    }
    foreach ($result in $matchResults) {
        $matchTickRowsForResult = @(
            $tickRows | Where-Object { [uint64]$_.match_id -eq $result.match_id }
        )
        $matchReplicationRowsForResult = @(
            $replicationRows |
                Where-Object { [uint64]$_.match_id -eq $result.match_id }
        )
        if ($matchTickRowsForResult.Count -ne [uint32]$result.finished_tick -or
            $matchReplicationRowsForResult.Count -ne [Math]::Floor(
                [uint32]$result.finished_tick / 2.0)) {
            throw "Network load raw sample 수가 result tick과 다릅니다. match=$($result.match_id)"
        }
        $tickP95 = [uint64](Get-DxaNearestRankP95 -Values @(
            $matchTickRowsForResult | ForEach-Object { [double]$_.duration_ns }
        ))
        $replicationP95 = [uint64](Get-DxaNearestRankP95 -Values @(
            $matchReplicationRowsForResult |
                ForEach-Object { [double]$_.encode_duration_ns }
        ))
        $matchPayloadBytes = [uint64](($matchReplicationRowsForResult |
            Measure-Object -Property payload_bytes -Sum).Sum)
        if ([uint64]$matchTickRowsForResult[0].tick_p95_ns -ne $tickP95 -or
            [uint64]$matchReplicationRowsForResult[0].replication_p95_ns -ne
                $replicationP95 -or
            [uint64]$matchTickRowsForResult[0].payload_bytes -ne
                $matchPayloadBytes) {
            throw "Network load server aggregate가 raw CSV와 다릅니다. match=$($result.match_id)"
        }
    }

    Stop-DxaProcessTreeIfRunning -Process $gameProcess
    $gameProcess = $null
    Stop-DxaProcessTreeIfRunning -Process $lobbyProcess
    $lobbyProcess = $null

    $clientsPath = Join-Path $outputDirectory 'clients.csv'
    $clientCsv = @($clientRows | ConvertTo-Csv -NoTypeInformation) -join "`n"
    Write-DxaNetworkLoadUtf8 -Path $clientsPath -Contents ($clientCsv + "`n")
    Write-DxaNetworkLoadUtf8 `
        -Path (Join-Path $outputDirectory 'command.txt') `
        -Contents (($commandLines -join "`n") + "`n")

    $allLogText = @(
        Get-ChildItem -LiteralPath $outputDirectory -Filter '*.log' -File |
            ForEach-Object { Get-Content -Raw -LiteralPath $_.FullName }
    ) -join "`n"
    $secretLeakCount = 0
    foreach ($pattern in Get-DeterministicSecretPatterns) {
        if ($allLogText.IndexOf($pattern, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
            ++$secretLeakCount
        }
    }

    $tickDurations = @($tickRows | ForEach-Object { [double]$_.duration_ns })
    $replicationDurations = @($replicationRows | ForEach-Object { [double]$_.encode_duration_ns })
    $matchTickRows = @($tickRows | Group-Object match_id | ForEach-Object { $_.Group[0] })
    $totalServerTcpBytes = [uint64](($matchTickRows | Measure-Object -Property tcp_bytes -Sum).Sum)
    $totalServerUdpBytes = [uint64](($matchTickRows | Measure-Object -Property udp_bytes -Sum).Sum)
    $totalPayloadBytes = [uint64](($replicationRows | Measure-Object -Property payload_bytes -Sum).Sum)
    $schedulerOverruns = [uint64](($matchTickRows |
        Measure-Object -Property scheduler_overruns -Sum).Sum)
    $keyframeCount = @($replicationRows | Where-Object { $_.keyframe -eq '1' }).Count
    $fallbackKeyframeCount = @(
        $replicationRows | Where-Object { $_.fallback_keyframe -eq '1' }
    ).Count
    $discardedSnapshots = [uint64](($clientRows |
        Measure-Object -Property discarded_snapshots -Sum).Sum)
    $keyframeRequests = [uint64](($clientRows |
        Measure-Object -Property keyframe_requests -Sum).Sum)
    $totalClientReceivedBytes = [uint64](($clientRows | ForEach-Object {
        [uint64]$_.tcp_received_bytes + [uint64]$_.udp_received_bytes
    } | Measure-Object -Sum).Sum)
    $totalClientSeconds = [double](($matchResults | ForEach-Object {
        ([double]$_.finished_tick / 30.0) * 24.0
    } | Measure-Object -Sum).Sum)
    $averageClientBytesPerSecond = if ($totalClientSeconds -gt 0.0) {
        $totalClientReceivedBytes / $totalClientSeconds
    }
    else { 0.0 }
    $tickP95Ms = (Get-DxaNearestRankP95 -Values $tickDurations) / 1000000.0
    $replicationP95Ms = (Get-DxaNearestRankP95 -Values $replicationDurations) / 1000000.0

    $finishedAt = [DateTimeOffset]::UtcNow
    $environment = [ordered]@{
        schema_version = 1
        run_id = $runId
        started_at = $startedAt.ToString('o')
        finished_at = $finishedAt.ToString('o')
        elapsed_seconds = [Math]::Round(($finishedAt - $startedAt).TotalSeconds, 3)
        git = [ordered]@{
            commit_sha = $guard.git.commit_sha
            short_sha = $guard.git.short_sha
            branch = $guard.git.branch
            clean_before_run = $true
        }
        build = [ordered]@{
            preset = $presetSegment
            configuration = $configuration
        }
        operating_system = (Get-CimInstance Win32_OperatingSystem |
            Select-Object Caption, Version, BuildNumber)
        processor = (Get-CimInstance Win32_Processor |
            Select-Object -First 1 Name, NumberOfCores, NumberOfLogicalProcessors)
        process_topology = [ordered]@{
            lobby_servers = 1
            game_servers = 1
            dx11_clients_per_match = 1
            bot_processes_per_match = 1
            bot_sessions_per_match = 23
        }
        selected_adapter = $selectedAdapter
    }
    Write-DxaNetworkLoadUtf8 `
        -Path (Join-Path $outputDirectory 'environment.json') `
        -Contents ($environment | ConvertTo-Json -Depth 6)

    $summary = [ordered]@{
        schema_version = 1
        run_id = $runId
        commit_sha = $CommitSha
        replication_mode = $ReplicationMode
        impairment_enabled = $false
        match_count = $Matches
        seeds = $Seeds
        participant_count = 24
        bot_session_count = 23
        neutral_ai_count = 100
        results = $matchResults
        raw = [ordered]@{
            tick_sample_count = $tickRows.Count
            replication_sample_count = $replicationRows.Count
            client_row_count = $clientRows.Count
            server_tcp_bytes = $totalServerTcpBytes
            server_udp_bytes = $totalServerUdpBytes
            payload_bytes = $totalPayloadBytes
            client_received_bytes = $totalClientReceivedBytes
            scheduler_overruns = $schedulerOverruns
            keyframe_count = $keyframeCount
            fallback_keyframe_count = $fallbackKeyframeCount
            discarded_snapshots = $discardedSnapshots
            keyframe_requests = $keyframeRequests
        }
        metrics = [ordered]@{
            server_tick_p95_ms = [Math]::Round($tickP95Ms, 6)
            replication_encode_p95_ms = [Math]::Round($replicationP95Ms, 6)
            average_client_received_bytes_per_second = [Math]::Round(
                $averageClientBytesPerSecond,
                3)
        }
        targets = [ordered]@{
            server_tick_p95_limit_ms = 33.3
            average_client_received_limit_bytes_per_second = 65536
            server_tick_pass = $tickP95Ms -le 33.3
            client_receive_pass = $averageClientBytesPerSecond -le 65536.0
        }
        secret_leak_count = $secretLeakCount
    }
    Write-DxaNetworkLoadUtf8 `
        -Path (Join-Path $outputDirectory 'summary.json') `
        -Contents ($summary | ConvertTo-Json -Depth 8)

    $resultLines = @(
        '# Network load result',
        '',
        "- commit: $CommitSha",
        "- mode: $ReplicationMode",
        "- matches: $Matches",
        '- participants: 24',
        '- bot sessions: 23',
        '- neutral AI: 100',
        "- server tick P95 ms: $([Math]::Round($tickP95Ms, 6))",
        "- replication encode P95 ms: $([Math]::Round($replicationP95Ms, 6))",
        "- average client received bytes per second: $([Math]::Round($averageClientBytesPerSecond, 3))",
        "- secret leak count: $secretLeakCount",
        '',
        '이 파일은 raw CSV에서 계산한 실제 결과다. 목표 미달 수치도 그대로 유지한다.'
    )
    Write-DxaNetworkLoadUtf8 `
        -Path (Join-Path $outputDirectory 'RESULT.md') `
        -Contents (($resultLines -join "`n") + "`n")

    if ($secretLeakCount -ne 0) {
        throw "Network load log에서 deterministic secret pattern을 발견했습니다: $secretLeakCount"
    }
    $finalHead = (@(& git -C $repositoryRoot rev-parse HEAD) -join '').Trim()
    if ($LASTEXITCODE -ne 0 -or $finalHead -ne $CommitSha) {
        throw 'Network load 실행 중 Git HEAD가 변경됐습니다.'
    }

    Write-Output $outputDirectory
}
finally {
    Stop-DxaProcessTreeIfRunning -Process $botProcess
    Stop-DxaProcessTreeIfRunning -Process $clientProcess
    Stop-DxaProcessTreeIfRunning -Process $gameProcess
    Stop-DxaProcessTreeIfRunning -Process $lobbyProcess
}
