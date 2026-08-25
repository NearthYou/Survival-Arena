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
    $p95 = Get-DxaNearestRankP95 -Values ([double[]](1..20))
    if ($p95 -ne 19.0) {
        throw "Network load nearest-rank P95가 잘못됐습니다: $p95"
    }
    $quoted = ConvertTo-DxaProcessArgumentString -Arguments @(
        'alpha',
        'path with space',
        'quote"value')
    if ($quoted -ne '"alpha" "path with space" "quote\"value"') {
        throw "Network load process argument quoting이 잘못됐습니다: $quoted"
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
    $server = Start-Process `
        -FilePath $serverExecutable `
        -ArgumentList $serverArguments `
        -RedirectStandardOutput $serverStdout `
        -RedirectStandardError $serverStderr `
        -WindowStyle Hidden `
        -PassThru
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
            $replicationHeader -notmatch '^match_id,sample_index,encode_duration_ns') {
            throw 'Game server metrics CSV header가 예상과 다릅니다.'
        }
    }
    finally {
        $server.Refresh()
        if (-not $server.HasExited) {
            Stop-Process -Id $server.Id -Force
            $server.WaitForExit(5000) | Out-Null
        }
    }

    $duplicate = Start-Process `
        -FilePath $serverExecutable `
        -ArgumentList $serverArguments `
        -RedirectStandardOutput (Join-Path $resolvedTemporaryRoot 'duplicate.stdout.log') `
        -RedirectStandardError (Join-Path $resolvedTemporaryRoot 'duplicate.stderr.log') `
        -WindowStyle Hidden `
        -PassThru
    if (-not $duplicate.WaitForExit(5000)) {
        Stop-Process -Id $duplicate.Id -Force
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
