[CmdletBinding()]
param(
    [ValidateSet(
        'full-state',
        'interest-full',
        'interest-quantized',
        'interest-delta')]
    [string]$ReplicationMode = 'full-state',

    [ValidateRange(1, 3)]
    [int]$Matches = 1,

    [uint32[]]$Seeds = @(20260825),

    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$CommitSha,

    [switch]$Impairment,
    [switch]$Release,

    [ValidateSet(0, 30)]
    [int]$SoakMinutes = 0,

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

if ($Seeds.Count -ne $Matches) {
    throw 'Seeds 수가 실행 mode와 맞지 않습니다.'
}
$matchSeedBase = [uint32]20260824
for ($index = 0; $index -lt $Seeds.Count; ++$index) {
    $expectedSeed = $matchSeedBase -bxor [uint32]($index + 1)
    if ($Seeds[$index] -ne $expectedSeed) {
        throw "현재 lobby seed 순서와 요청 seed가 다릅니다. index=$index expected=$expectedSeed actual=$($Seeds[$index])"
    }
}

$shortSha = if ($CommitSha.Length -ge 8) { $CommitSha.Substring(0, 8) } else { $CommitSha }
$runId = '{0}-{1}-{2}' -f (
    Get-Date -Format 'yyyyMMdd-HHmmss'), $shortSha, $ReplicationMode
if ($Impairment) {
    $runId += '-impairment'
}
if ($SoakMinutes -gt 0) {
    $runId += "-soak$SoakMinutes"
}
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
    -SoakMinutes $SoakMinutes `
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
$impairmentArguments = if ($Impairment) {
    @(
        '--udp-latency-ms', '50',
        '--udp-jitter-ms', '10',
        '--udp-loss-basis-points', '200',
        '--network-seed', '20260825')
}
else {
    @()
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
$actualSeeds = [Collections.Generic.List[uint32]]::new()
$matchDirectories = [Collections.Generic.List[string]]::new()
$totalMeasuredSeconds = 0.0
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
    $gameArguments = @($gameArguments + $impairmentArguments)
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

    $matchIndex = 0
    while ($matchIndex -lt $Matches -or
        ($SoakMinutes -gt 0 -and
         $totalMeasuredSeconds -lt $SoakMinutes * 60.0)) {
        if ($matchIndex -ge 1000) {
            throw 'Network load soak match 수가 안전 한계를 넘었습니다.'
        }
        $ordinal = $matchIndex + 1
        $matchDirectory = Join-Path $outputDirectory ('match-{0:D3}' -f $ordinal)
        New-Item -ItemType Directory -Path $matchDirectory | Out-Null
        $matchDirectories.Add($matchDirectory)
        $matchCommandLines = [Collections.Generic.List[string]]::new()
        $matchWorkingSetRows = [Collections.Generic.List[object]]::new()
        $matchClientRows = [Collections.Generic.List[object]]::new()
        $clientStdout = Join-Path $matchDirectory 'client.stdout.log'
        $clientStderr = Join-Path $matchDirectory 'client.stderr.log'
        $clientArguments = @(
            '--warp',
            '--hidden',
            '--verify-render',
            '--render-path', 'hybrid-deferred',
            '--network-create',
            '--replication-mode', $ReplicationMode,
            '--expected-players', '24',
            '--exit-on-match-result',
            '--lobby-host', '127.0.0.1',
            '--lobby-port', $lobbyPort,
            '--width', '1920',
            '--height', '1080')
        $clientArguments = @($clientArguments + $impairmentArguments)
        $clientCommand = "$clientExecutable $(ConvertTo-DxaProcessArgumentString $clientArguments)"
        $commandLines.Add($clientCommand)
        $matchCommandLines.Add($clientCommand)
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

        $botStdout = Join-Path $matchDirectory 'bot.stdout.log'
        $botStderr = Join-Path $matchDirectory 'bot.stderr.log'
        $botArguments = @(
            '--host', '127.0.0.1',
            '--port', $lobbyPort,
            '--room', $roomId,
            '--count', '23',
            '--play')
        $botArguments = @($botArguments + $impairmentArguments)
        $botCommand = "$botExecutable $(ConvertTo-DxaProcessArgumentString $botArguments)"
        $commandLines.Add($botCommand)
        $matchCommandLines.Add($botCommand)
        $botProcess = Start-DxaLoggedProcess `
            -FilePath $botExecutable `
            -Arguments $botArguments `
            -StdoutPath $botStdout `
            -StderrPath $botStderr

        $workingSetSample = {
            $timestamp = [DateTimeOffset]::UtcNow.ToString('o')
            foreach ($entry in @(
                    [pscustomobject]@{
                        name = 'lobby_server'
                        process = $lobbyProcess
                    },
                    [pscustomobject]@{
                        name = 'game_server'
                        process = $gameProcess
                    },
                    [pscustomobject]@{
                        name = 'dx11_client'
                        process = $clientProcess
                    },
                    [pscustomobject]@{
                        name = 'bot_client'
                        process = $botProcess
                    })) {
                $bytes = Get-DxaProcessTreeWorkingSetBytes `
                    -Process $entry.process
                if ($bytes -gt 0) {
                    $matchWorkingSetRows.Add([pscustomobject]@{
                        timestamp_utc = $timestamp
                        process = $entry.name
                        working_set_bytes = $bytes
                    })
                }
            }
        }
        Wait-DxaNetworkLoadProcesses `
            -Processes @($botProcess, $clientProcess) `
            -TimeoutSeconds 660 `
            -OnSample $workingSetSample `
            -SampleIntervalSeconds 10
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
        $clientMetric = [regex]::Match(
            $clientText,
            'client game metrics measurement_ns=(\d+) tcp_received_bytes=(\d+) udp_received_bytes=(\d+) discarded_snapshots=(\d+) keyframe_requests=(\d+) udp_dropped=(\d+) udp_delayed=(\d+) udp_delivered=(\d+) shaped_queue_overflows=(\d+) protocol_errors=(\d+)')
        if (-not $clientResult.Success -or
            -not $botResult.Success -or
            -not $clientMetric.Success) {
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
        $actualSeed = Get-DxaNetworkLoadMatchSeed `
            -MatchId $matchId `
            -SeedBase $matchSeedBase
        if ($matchIndex -lt $Seeds.Count -and
            $actualSeed -ne $Seeds[$matchIndex]) {
            throw 'Network load 실제 MatchId의 seed가 요청과 다릅니다.'
        }
        $actualSeeds.Add($actualSeed)

        $clientMeasurementSeconds =
            [double]$clientMetric.Groups[1].Value / 1000000000.0
        if ($clientMeasurementSeconds -le 0.0) {
            throw 'Network load DX11 measurement time이 유효하지 않습니다.'
        }
        $dx11Row = [pscustomobject]@{
            match_id = $matchId
            client_kind = 'dx11'
            player_id = [uint32]$clientResult.Groups[2].Value
            snapshots_applied = [uint64]$clientResult.Groups[6].Value
            tcp_received_bytes = [uint64]$clientMetric.Groups[2].Value
            udp_received_bytes = [uint64]$clientMetric.Groups[3].Value
            game_received_bytes = [uint64]$clientMetric.Groups[2].Value +
                [uint64]$clientMetric.Groups[3].Value
            measurement_seconds = $clientMeasurementSeconds
            discarded_snapshots = [uint64]$clientMetric.Groups[4].Value
            keyframe_requests = [uint64]$clientMetric.Groups[5].Value
            udp_dropped = [uint64]$clientMetric.Groups[6].Value
            udp_delayed = [uint64]$clientMetric.Groups[7].Value
            udp_delivered = [uint64]$clientMetric.Groups[8].Value
            shaped_queue_overflows = [uint64]$clientMetric.Groups[9].Value
            protocol_errors = [uint64]$clientMetric.Groups[10].Value
            exit_code = 0
        }
        $clientRows.Add($dx11Row)
        $matchClientRows.Add($dx11Row)
        $botSessions = [regex]::Matches(
            $botText,
            'bot session player=(\d+) match=(\d+) snapshots_applied=(\d+) tcp_received_bytes=(\d+) udp_received_bytes=(\d+) discarded_snapshots=(\d+) keyframe_requests=(\d+) measurement_ns=(\d+) udp_dropped=(\d+) udp_delayed=(\d+) udp_delivered=(\d+) shaped_queue_overflows=(\d+) protocol_errors=(\d+) exit=(\d+)')
        if ($botSessions.Count -ne 23) {
            throw "Network load bot session row 수가 23이 아닙니다: $($botSessions.Count)"
        }
        foreach ($session in $botSessions) {
            if ([uint64]$session.Groups[2].Value -ne $matchId -or
                [int]$session.Groups[14].Value -ne 0 -or
                [uint64]$session.Groups[3].Value -lt 2 -or
                [uint64]$session.Groups[4].Value -eq 0 -or
                [uint64]$session.Groups[5].Value -eq 0 -or
                [uint64]$session.Groups[8].Value -eq 0) {
                throw 'Network load bot session metric이 유효하지 않습니다.'
            }
            $botMeasurementSeconds =
                [double]$session.Groups[8].Value / 1000000000.0
            $botRow = [pscustomobject]@{
                match_id = $matchId
                client_kind = 'bot'
                player_id = [uint32]$session.Groups[1].Value
                snapshots_applied = [uint64]$session.Groups[3].Value
                tcp_received_bytes = [uint64]$session.Groups[4].Value
                udp_received_bytes = [uint64]$session.Groups[5].Value
                game_received_bytes = [uint64]$session.Groups[4].Value +
                    [uint64]$session.Groups[5].Value
                measurement_seconds = $botMeasurementSeconds
                discarded_snapshots = [uint64]$session.Groups[6].Value
                keyframe_requests = [uint64]$session.Groups[7].Value
                udp_dropped = [uint64]$session.Groups[9].Value
                udp_delayed = [uint64]$session.Groups[10].Value
                udp_delivered = [uint64]$session.Groups[11].Value
                shaped_queue_overflows = [uint64]$session.Groups[12].Value
                protocol_errors = [uint64]$session.Groups[13].Value
                exit_code = [int]$session.Groups[14].Value
            }
            $clientRows.Add($botRow)
            $matchClientRows.Add($botRow)
        }
        $matchResults.Add([pscustomobject]@{
            match_id = $matchId
            room_id = $roomId
            seed = $actualSeed
            winner = $clientResult.Groups[3].Value
            reason = [uint32]$clientResult.Groups[5].Value
            finished_tick = [uint32]$clientResult.Groups[4].Value
        })
        $matchMeasuredSeconds = [double](($matchClientRows |
            Measure-Object -Property measurement_seconds -Maximum).Maximum)
        $totalMeasuredSeconds += $matchMeasuredSeconds
        $matchClientCsv = @($matchClientRows | ConvertTo-Csv -NoTypeInformation) -join "`n"
        Write-DxaNetworkLoadUtf8 `
            -Path (Join-Path $matchDirectory 'clients.csv') `
            -Contents ($matchClientCsv + "`n")
        $workingSetText = if ($matchWorkingSetRows.Count -gt 0) {
            (@($matchWorkingSetRows | ConvertTo-Csv -NoTypeInformation) -join "`n") + "`n"
        }
        else {
            "timestamp_utc,process,working_set_bytes`n"
        }
        Write-DxaNetworkLoadUtf8 `
            -Path (Join-Path $matchDirectory 'working-set.csv') `
            -Contents $workingSetText
        $matchCommandText = ConvertTo-DxaEvidenceCommandText `
            -CommandLines $matchCommandLines `
            -RepositoryRoot $repositoryRoot `
            -RunDirectory $outputDirectory
        Write-DxaNetworkLoadUtf8 `
            -Path (Join-Path $matchDirectory 'command.txt') `
            -Contents ($matchCommandText + "`n")

        $matchLogText = @(
            Get-ChildItem -LiteralPath $matchDirectory -Filter '*.log' -File |
                ForEach-Object { Get-Content -Raw -LiteralPath $_.FullName }
        ) -join "`n"
        $matchSecretLeaks = 0
        foreach ($pattern in Get-DeterministicSecretPatterns) {
            if ($matchLogText.IndexOf(
                    $pattern,
                    [StringComparison]::OrdinalIgnoreCase) -ge 0) {
                ++$matchSecretLeaks
            }
        }
        $matchProtocolErrors = [uint64](($matchClientRows |
            Measure-Object -Property protocol_errors -Sum).Sum)
        $matchQueueOverflows = [uint64](($matchClientRows |
            Measure-Object -Property shaped_queue_overflows -Sum).Sum)
        $matchMetadata = [ordered]@{
            schema_version = 2
            commit_sha = $CommitSha
            seed = $actualSeed
            participant_count = 24
            match_id = $matchId
            room_id = $roomId
            winner = $clientResult.Groups[3].Value
            reason = [uint32]$clientResult.Groups[5].Value
            finished_tick = [uint32]$clientResult.Groups[4].Value
            exit_code = 0
            protocol_errors = $matchProtocolErrors
            shaped_queue_overflows = $matchQueueOverflows
            secret_leak_count = $matchSecretLeaks
            measurement_seconds = $matchMeasuredSeconds
        }
        Write-DxaNetworkLoadUtf8 `
            -Path (Join-Path $matchDirectory 'match.json') `
            -Contents ($matchMetadata | ConvertTo-Json -Depth 6)
        $botProcess = $null
        $clientProcess = $null
        ++$matchIndex
    }

    $actualMatchCount = $matchResults.Count
    $tickPath = Join-Path $outputDirectory 'server-ticks.csv'
    $replicationPath = Join-Path $outputDirectory 'replication.csv'
    $metricsDeadline = [DateTimeOffset]::UtcNow.AddSeconds(30)
    while ([DateTimeOffset]::UtcNow -lt $metricsDeadline) {
        if ((Test-Path -LiteralPath $tickPath -PathType Leaf) -and
            (Test-Path -LiteralPath $replicationPath -PathType Leaf)) {
            $tickRows = @(Import-Csv -LiteralPath $tickPath)
            $replicationRows = @(Import-Csv -LiteralPath $replicationPath)
            $exportedMatches = @($tickRows.match_id | Sort-Object -Unique)
            if ($exportedMatches.Count -eq $actualMatchCount -and
                $replicationRows.Count -gt 0) {
                break
            }
        }
        Start-Sleep -Milliseconds 100
    }
    $tickRows = @(Import-Csv -LiteralPath $tickPath)
    $replicationRows = @(Import-Csv -LiteralPath $replicationPath)
    if (@($tickRows.match_id | Sort-Object -Unique).Count -ne
            $actualMatchCount -or
        $replicationRows.Count -eq 0) {
        throw 'Network load server metrics가 모두 flush되지 않았습니다.'
    }
    if ($clientRows.Count -ne $actualMatchCount * 24) {
        throw "Network load client row 수가 예상과 다릅니다: $($clientRows.Count)"
    }
    for ($resultIndex = 0; $resultIndex -lt $matchResults.Count; ++$resultIndex) {
        $result = $matchResults[$resultIndex]
        $matchTickRowsForResult = @(
            $tickRows | Where-Object { [uint64]$_.match_id -eq $result.match_id }
        )
        $matchReplicationRowsForResult = @(
            $replicationRows |
                Where-Object { [uint64]$_.match_id -eq $result.match_id }
        )
        $maximumReplicationRows =
            [Math]::Floor([uint32]$result.finished_tick / 2.0) * 24
        if ($matchTickRowsForResult.Count -ne [uint32]$result.finished_tick -or
            $matchReplicationRowsForResult.Count -le 0 -or
            $matchReplicationRowsForResult.Count -gt $maximumReplicationRows) {
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

        $matchDirectory = $matchDirectories[$resultIndex]
        $matchTickText = @(
            $matchTickRowsForResult | ConvertTo-Csv -NoTypeInformation) -join "`n"
        $matchReplicationText = @(
            $matchReplicationRowsForResult |
                ConvertTo-Csv -NoTypeInformation) -join "`n"
        Write-DxaNetworkLoadUtf8 `
            -Path (Join-Path $matchDirectory 'server-ticks.csv') `
            -Contents ($matchTickText + "`n")
        Write-DxaNetworkLoadUtf8 `
            -Path (Join-Path $matchDirectory 'replication.csv') `
            -Contents ($matchReplicationText + "`n")

        $metadataPath = Join-Path $matchDirectory 'match.json'
        $metadata = Get-Content -Raw -LiteralPath $metadataPath |
            ConvertFrom-Json
        $serverDropped = [uint64]$matchTickRowsForResult[0].udp_dropped
        $serverDelayed = [uint64]$matchTickRowsForResult[0].udp_delayed
        $serverDelivered = [uint64]$matchTickRowsForResult[0].udp_delivered
        $serverOverflows =
            [uint64]$matchTickRowsForResult[0].shaped_queue_overflows
        $metadata.shaped_queue_overflows =
            [uint64]$metadata.shaped_queue_overflows + $serverOverflows
        $metadata | Add-Member `
            -NotePropertyName server_udp_dropped `
            -NotePropertyValue $serverDropped
        $metadata | Add-Member `
            -NotePropertyName server_udp_delayed `
            -NotePropertyValue $serverDelayed
        $metadata | Add-Member `
            -NotePropertyName server_udp_delivered `
            -NotePropertyValue $serverDelivered
        Write-DxaNetworkLoadUtf8 `
            -Path $metadataPath `
            -Contents ($metadata | ConvertTo-Json -Depth 6)
    }

    Stop-DxaProcessTreeIfRunning -Process $gameProcess
    $gameProcess = $null
    Stop-DxaProcessTreeIfRunning -Process $lobbyProcess
    $lobbyProcess = $null

    $clientsPath = Join-Path $outputDirectory 'clients.csv'
    $clientCsv = @($clientRows | ConvertTo-Csv -NoTypeInformation) -join "`n"
    Write-DxaNetworkLoadUtf8 -Path $clientsPath -Contents ($clientCsv + "`n")
    $commandText = ConvertTo-DxaEvidenceCommandText `
        -CommandLines $commandLines `
        -RepositoryRoot $repositoryRoot `
        -RunDirectory $outputDirectory
    Write-DxaNetworkLoadUtf8 `
        -Path (Join-Path $outputDirectory 'command.txt') `
        -Contents ($commandText + "`n")

    $allLogText = @(
        Get-ChildItem `
            -LiteralPath $outputDirectory `
            -Filter '*.log' `
            -File `
            -Recurse |
            ForEach-Object { Get-Content -Raw -LiteralPath $_.FullName }
    ) -join "`n"
    $secretLeakCount = 0
    foreach ($pattern in Get-DeterministicSecretPatterns) {
        if ($allLogText.IndexOf($pattern, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
            ++$secretLeakCount
        }
    }

    $finishedAt = [DateTimeOffset]::UtcNow
    $environment = [ordered]@{
        schema_version = 2
        run_id = $runId
        started_at = $startedAt.ToString('o')
        finished_at = $finishedAt.ToString('o')
        elapsed_seconds = [Math]::Round(($finishedAt - $startedAt).TotalSeconds, 3)
        git = [ordered]@{
            commit_sha = $guard.git.commit_sha
            short_sha = $guard.git.short_sha
            branch = $guard.git.branch
            clean_before_run = $guard.git.clean
            evidence_only_changes_before_run =
                $guard.git.evidence_only_changes
        }
        build = [ordered]@{
            preset = $presetSegment
            configuration = $configuration
        }
        operating_system = (Get-CimInstance Win32_OperatingSystem |
            Select-Object Caption, Version, BuildNumber, TotalVisibleMemorySize)
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
        replication_mode = $ReplicationMode
        match_count = $actualMatchCount
        soak_minutes = $SoakMinutes
        measured_match_seconds = [Math]::Round($totalMeasuredSeconds, 3)
        network = [ordered]@{
            impairment_enabled = [bool]$Impairment
            one_way_latency_ms = if ($Impairment) { 50 } else { 0 }
            jitter_ms = if ($Impairment) { 10 } else { 0 }
            loss_basis_points = if ($Impairment) { 200 } else { 0 }
            seed = if ($Impairment) { 20260825 } else { 0 }
        }
    }
    Write-DxaNetworkLoadUtf8 `
        -Path (Join-Path $outputDirectory 'environment.json') `
        -Contents ($environment | ConvertTo-Json -Depth 6)
    foreach ($matchDirectory in $matchDirectories) {
        Write-DxaNetworkLoadUtf8 `
            -Path (Join-Path $matchDirectory 'environment.json') `
            -Contents ($environment | ConvertTo-Json -Depth 6)
    }

    if ($secretLeakCount -ne 0) {
        throw "Network load log에서 deterministic secret pattern을 발견했습니다: $secretLeakCount"
    }
    $finalHead = (@(& git -C $repositoryRoot rev-parse HEAD) -join '').Trim()
    if ($LASTEXITCODE -ne 0 -or $finalHead -ne $CommitSha) {
        throw 'Network load 실행 중 Git HEAD가 변경됐습니다.'
    }
    $minimumWorkingSetSamples = if ($SoakMinutes -gt 0) { 90 } else { 0 }
    $aggregate = Get-DxaNetworkLoadAggregate `
        -ParentDirectory $outputDirectory `
        -ExpectedCommitSha $CommitSha `
        -ExpectedSeeds $actualSeeds.ToArray() `
        -ExpectedParticipantCount 24 `
        -WorkingSetWindowSeconds 900
    Assert-DxaNetworkLoadAggregateReady `
        -Aggregate $aggregate `
        -MinimumGameServerWorkingSetSamples $minimumWorkingSetSamples
    $summaryPath = Join-Path $outputDirectory 'summary.json'
    $resultPath = Join-Path $outputDirectory 'RESULT.md'
    if ((Test-Path -LiteralPath $summaryPath) -or
        (Test-Path -LiteralPath $resultPath)) {
        throw 'Network load parent output file이 이미 존재합니다.'
    }
    $aggregate | Add-Member -NotePropertyName run_id -NotePropertyValue $runId
    $aggregate | Add-Member `
        -NotePropertyName replication_mode `
        -NotePropertyValue $ReplicationMode
    $aggregate | Add-Member `
        -NotePropertyName impairment_enabled `
        -NotePropertyValue ([bool]$Impairment)
    $aggregate | Add-Member `
        -NotePropertyName soak_minutes `
        -NotePropertyValue $SoakMinutes
    $replicationP95Ms = (Get-DxaNearestRankP95 -Values @(
        $replicationRows | ForEach-Object { [double]$_.encode_duration_ns }
    )) / 1000000.0
    $matchTickRows = @($tickRows |
        Group-Object match_id |
        ForEach-Object { $_.Group[0] })
    $aggregate.raw | Add-Member `
        -NotePropertyName replication_sample_count `
        -NotePropertyValue $replicationRows.Count
    $aggregate.raw | Add-Member `
        -NotePropertyName server_tcp_bytes `
        -NotePropertyValue ([uint64](($matchTickRows |
            Measure-Object -Property tcp_bytes -Sum).Sum))
    $aggregate.raw | Add-Member `
        -NotePropertyName server_udp_bytes `
        -NotePropertyValue ([uint64](($matchTickRows |
            Measure-Object -Property udp_bytes -Sum).Sum))
    $aggregate.raw | Add-Member `
        -NotePropertyName payload_bytes `
        -NotePropertyValue ([uint64](($replicationRows |
            Measure-Object -Property payload_bytes -Sum).Sum))
    $aggregate.raw | Add-Member `
        -NotePropertyName scheduler_overruns `
        -NotePropertyValue ([uint64](($matchTickRows |
            Measure-Object -Property scheduler_overruns -Sum).Sum))
    $aggregate.raw | Add-Member `
        -NotePropertyName keyframe_count `
        -NotePropertyValue @($replicationRows |
            Where-Object { $_.keyframe -eq '1' }).Count
    $aggregate.raw | Add-Member `
        -NotePropertyName fallback_keyframe_count `
        -NotePropertyValue @($replicationRows |
            Where-Object { $_.fallback_keyframe -eq '1' }).Count
    $aggregate.raw | Add-Member `
        -NotePropertyName discarded_snapshots `
        -NotePropertyValue ([uint64](($clientRows |
            Measure-Object -Property discarded_snapshots -Sum).Sum))
    $aggregate.raw | Add-Member `
        -NotePropertyName keyframe_requests `
        -NotePropertyValue ([uint64](($clientRows |
            Measure-Object -Property keyframe_requests -Sum).Sum))
    $aggregate.raw | Add-Member `
        -NotePropertyName udp_datagrams_dropped `
        -NotePropertyValue ([uint64](($clientRows |
            Measure-Object -Property udp_dropped -Sum).Sum) +
            [uint64](($matchTickRows |
                Measure-Object -Property udp_dropped -Sum).Sum))
    $aggregate.raw | Add-Member `
        -NotePropertyName udp_datagrams_delayed `
        -NotePropertyValue ([uint64](($clientRows |
            Measure-Object -Property udp_delayed -Sum).Sum) +
            [uint64](($matchTickRows |
                Measure-Object -Property udp_delayed -Sum).Sum))
    $aggregate.metrics | Add-Member `
        -NotePropertyName replication_encode_p95_ms `
        -NotePropertyValue ([Math]::Round($replicationP95Ms, 6))
    $aggregate | Add-Member -NotePropertyName targets -NotePropertyValue (
        [pscustomobject]@{
            server_tick_p95_limit_ms = 33.3
            participant_average_received_limit_kib_per_second = 64.0
            server_tick_pass =
                $aggregate.metrics.server_tick_p95_ms -le 33.3
            participant_average_receive_pass =
                $aggregate.metrics.participant_average_received_kib_per_second -le 64.0
        })
    $dockerCommand = @(
        'docker run --rm',
        '--mount type=bind,source=<REPOSITORY_ROOT>,target=/workspace',
        '-w /workspace',
        'dxa-linux-asan:local',
        "bash -lc 'ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 timeout 1800s ./out/build/linux-asan/tests/dxa_tests --gtest_filter=GameServerIntegration.PlayCoordinatorReportsEverySession --gtest_repeat=-1'"
    ) -join ' '
    Write-DxaNetworkLoadUtf8 `
        -Path (Join-Path $outputDirectory 'docker-asan-command.txt') `
        -Contents ($dockerCommand + "`n")
    Write-DxaNetworkLoadUtf8 `
        -Path $summaryPath `
        -Contents ($aggregate | ConvertTo-Json -Depth 10)

    $resultLines = @(
        '# Network load result',
        '',
        "- commit: $CommitSha",
        "- mode: $ReplicationMode",
        "- impairment: $([bool]$Impairment)",
        "- matches: $actualMatchCount",
        "- soak minutes: $SoakMinutes",
        '- participants per match: 24',
        "- server tick P95 ms: $($aggregate.metrics.server_tick_p95_ms)",
        "- replication encode P95 ms: $($aggregate.metrics.replication_encode_p95_ms)",
        "- participant average received KiB/s: $($aggregate.metrics.participant_average_received_kib_per_second)",
        "- recipient received KiB/s P95: $($aggregate.metrics.recipient_received_kib_per_second_p95)",
        '',
        'This report is computed from child raw evidence and preserves target misses.'
    )
    Write-DxaNetworkLoadUtf8 `
        -Path $resultPath `
        -Contents (($resultLines -join "`n") + "`n")

    Write-Output $outputDirectory
}
finally {
    Stop-DxaProcessTreeIfRunning -Process $botProcess
    Stop-DxaProcessTreeIfRunning -Process $clientProcess
    Stop-DxaProcessTreeIfRunning -Process $gameProcess
    Stop-DxaProcessTreeIfRunning -Process $lobbyProcess
}
