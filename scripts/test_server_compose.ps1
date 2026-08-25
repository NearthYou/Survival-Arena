[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path $PSScriptRoot -Parent),
    [string]$DockerExecutable = 'docker',
    [string]$ImageName = 'dxa-server:compose-smoke',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-AvailableTcpPort {
    $listener = [Net.Sockets.TcpListener]::new(
        [Net.IPAddress]::Loopback,
        0)
    try {
        $listener.Start()
        return [int]$listener.LocalEndpoint.Port
    }
    finally {
        $listener.Stop()
    }
}

function Get-AvailableUdpPort {
    $client = [Net.Sockets.UdpClient]::new(0)
    try {
        return [int]$client.Client.LocalEndPoint.Port
    }
    finally {
        $client.Dispose()
    }
}

function Get-UniquePort {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [Collections.Generic.HashSet[int]]$UsedPorts,

        [Parameter(Mandatory)]
        [ValidateSet('tcp', 'udp')]
        [string]$Protocol
    )

    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        $port = if ($Protocol -eq 'tcp') {
            Get-AvailableTcpPort
        }
        else {
            Get-AvailableUdpPort
        }
        if ($port -ge 1024 -and $UsedPorts.Add($port)) {
            return $port
        }
    }
    throw "중복되지 않는 $Protocol port를 찾지 못했습니다."
}

function Assert-LogPattern {
    param(
        [Parameter(Mandatory)]
        [string]$Contents,

        [Parameter(Mandatory)]
        [string]$Pattern,

        [Parameter(Mandatory)]
        [string]$Label
    )

    if ($Contents -notmatch $Pattern) {
        throw "$Label log에서 pattern을 찾지 못했습니다: $Pattern"
    }
}

$repository = [IO.Path]::GetFullPath($RepositoryRoot)
$composeFile = Join-Path $repository 'deploy/compose.yaml'
$environmentFile = Join-Path $repository 'deploy/.env.example'
if (-not (Test-Path -LiteralPath $composeFile -PathType Leaf)) {
    throw "Compose 파일을 찾지 못했습니다: $composeFile"
}
if (-not (Test-Path -LiteralPath $environmentFile -PathType Leaf)) {
    throw "Compose 환경 파일을 찾지 못했습니다: $environmentFile"
}

& $DockerExecutable compose version
if ($LASTEXITCODE -ne 0) {
    throw 'Docker Compose를 실행할 수 없습니다.'
}

$projectName = 'dxa-smoke-{0}-{1}' -f `
    $PID, `
    ([Guid]::NewGuid().ToString('N').Substring(0, 8))
if ($projectName -notmatch '^dxa-smoke-[0-9]+-[0-9a-f]{8}$') {
    throw "안전하지 않은 Compose project 이름입니다: $projectName"
}

$usedPorts = [Collections.Generic.HashSet[int]]::new()
$lobbyPort = Get-UniquePort $usedPorts 'tcp'
$game1TcpPort = Get-UniquePort $usedPorts 'tcp'
$game1UdpPort = Get-UniquePort $usedPorts 'udp'
$game2TcpPort = Get-UniquePort $usedPorts 'tcp'
$game2UdpPort = Get-UniquePort $usedPorts 'udp'

$status = @(git -C $repository status --porcelain -uall)
if ($LASTEXITCODE -ne 0) {
    throw 'Git 상태를 읽지 못했습니다.'
}
$head = (git -C $repository rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw 'Git HEAD를 읽지 못했습니다.'
}
$imageRevision = if ($status.Count -eq 0) { $head } else { 'working-tree' }

$environment = [ordered]@{
    DXA_SERVER_IMAGE = $ImageName
    DXA_IMAGE_REVISION = $imageRevision
    DXA_PUBLIC_HOST = '127.0.0.1'
    DXA_LOBBY_PORT = [string]$lobbyPort
    DXA_GAME1_TCP_PORT = [string]$game1TcpPort
    DXA_GAME1_UDP_PORT = [string]$game1UdpPort
    DXA_GAME2_TCP_PORT = [string]$game2TcpPort
    DXA_GAME2_UDP_PORT = [string]$game2UdpPort
}
$originalEnvironment = @{}
foreach ($entry in $environment.GetEnumerator()) {
    $originalEnvironment[$entry.Key] = [Environment]::GetEnvironmentVariable(
        $entry.Key,
        [EnvironmentVariableTarget]::Process)
    [Environment]::SetEnvironmentVariable(
        $entry.Key,
        $entry.Value,
        [EnvironmentVariableTarget]::Process)
}

$composeArguments = @(
    'compose',
    '--project-name', $projectName,
    '--file', $composeFile,
    '--env-file', $environmentFile)
$started = $false

try {
    & $DockerExecutable @composeArguments config --quiet
    if ($LASTEXITCODE -ne 0) {
        throw 'Docker Compose config 검증이 실패했습니다.'
    }

    if ($SkipBuild) {
        & $DockerExecutable image inspect $ImageName | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Smoke image를 찾지 못했습니다: $ImageName"
        }
    }
    else {
        & $DockerExecutable @composeArguments build
        if ($LASTEXITCODE -ne 0) {
            throw 'Docker Compose image build가 실패했습니다.'
        }
    }

    $started = $true
    & $DockerExecutable `
        @composeArguments `
        up `
        --detach `
        --no-build `
        --wait `
        --wait-timeout 120
    if ($LASTEXITCODE -ne 0) {
        throw 'Docker Compose service 시작이 실패했습니다.'
    }

    $psOutput = @(& $DockerExecutable `
        @composeArguments `
        ps `
        --format json)
    if ($LASTEXITCODE -ne 0) {
        throw 'Docker Compose service 상태 조회가 실패했습니다.'
    }
    $containers = @($psOutput |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        ForEach-Object { $_ | ConvertFrom-Json })
    if ($containers.Count -ne 3) {
        throw "Compose container 수가 다릅니다: $($containers.Count)"
    }
    foreach ($container in $containers) {
        if ($container.State -ne 'running' -or $container.Health -ne 'healthy') {
            throw "Container가 healthy running 상태가 아닙니다: $($container.Service) state=$($container.State) health=$($container.Health)"
        }
    }

    $lobbyLog = (@(& $DockerExecutable `
        @composeArguments `
        logs `
        --no-color `
        lobby-server) -join [Environment]::NewLine)
    $game1Log = (@(& $DockerExecutable `
        @composeArguments `
        logs `
        --no-color `
        game-server-1) -join [Environment]::NewLine)
    $game2Log = (@(& $DockerExecutable `
        @composeArguments `
        logs `
        --no-color `
        game-server-2) -join [Environment]::NewLine)

    Assert-LogPattern $lobbyLog 'lobby_server_listening' 'lobby-server'
    $workerConnections = [regex]::Matches(
        $lobbyLog,
        'worker_control_connection_open').Count
    if ($workerConnections -lt 2) {
        throw "worker control 연결이 두 개보다 적습니다: $workerConnections"
    }
    Assert-LogPattern $game1Log 'game_server_listening worker=1' 'game-server-1'
    Assert-LogPattern $game1Log 'game_server_registered worker=1' 'game-server-1'
    Assert-LogPattern $game2Log 'game_server_listening worker=2' 'game-server-2'
    Assert-LogPattern $game2Log 'game_server_registered worker=2' 'game-server-2'

    $revision = (@(& $DockerExecutable `
        image `
        inspect `
        $ImageName `
        --format '{{index .Config.Labels "org.opencontainers.image.revision"}}') `
        -join '').Trim()
    if ($LASTEXITCODE -ne 0 -or $revision -ne $imageRevision) {
        throw "Image revision label이 다릅니다. 예상: $imageRevision, 실제: $revision"
    }

    Write-Output "compose_smoke project=$projectName image=$ImageName revision=$imageRevision"
    Write-Output "compose_smoke ports lobby=$lobbyPort game1_tcp=$game1TcpPort game1_udp=$game1UdpPort game2_tcp=$game2TcpPort game2_udp=$game2UdpPort"
    Write-Output "compose_smoke containers=3 healthy=3 worker_connections=$workerConnections registered_workers=1,2"
}
catch {
    if ($started) {
        & $DockerExecutable @composeArguments ps
        & $DockerExecutable @composeArguments logs --no-color
    }
    throw
}
finally {
    if ($started) {
        & $DockerExecutable `
            @composeArguments `
            down `
            --remove-orphans `
            --timeout 15 | Out-Null
    }

    foreach ($entry in $originalEnvironment.GetEnumerator()) {
        [Environment]::SetEnvironmentVariable(
            $entry.Key,
            $entry.Value,
            [EnvironmentVariableTarget]::Process)
    }

    $remainingContainers = @(& $DockerExecutable ps `
        --all `
        --quiet `
        --filter "label=com.docker.compose.project=$projectName")
    $remainingNetworks = @(& $DockerExecutable network ls `
        --quiet `
        --filter "label=com.docker.compose.project=$projectName")
    if ($remainingContainers.Count -ne 0 -or $remainingNetworks.Count -ne 0) {
        throw "Compose smoke 정리 뒤 resource가 남았습니다: containers=$($remainingContainers.Count), networks=$($remainingNetworks.Count)"
    }
}
