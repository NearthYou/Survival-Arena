param(
    [Parameter(Mandatory)]
    [string]$DockerExecutable,

    [Parameter(Mandatory)]
    [string]$ComposeFile,

    [Parameter(Mandatory)]
    [string]$EnvironmentFile
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-Equal {
    param(
        [Parameter(Mandatory)]
        [object]$Actual,

        [Parameter(Mandatory)]
        [object]$Expected,

        [Parameter(Mandatory)]
        [string]$Label
    )

    if ([string]$Actual -ne [string]$Expected) {
        throw "$Label 값이 다릅니다. 예상: $Expected, 실제: $Actual"
    }
}

function Assert-Contains {
    param(
        [Parameter(Mandatory)]
        [object[]]$Values,

        [Parameter(Mandatory)]
        [object]$Expected,

        [Parameter(Mandatory)]
        [string]$Label
    )

    if ($Values -notcontains $Expected) {
        throw "$Label 목록에 필요한 값이 없습니다: $Expected"
    }
}

function Get-OptionValue {
    param(
        [Parameter(Mandatory)]
        [object[]]$Command,

        [Parameter(Mandatory)]
        [string]$Option
    )

    $index = [Array]::IndexOf($Command, $Option)
    if ($index -lt 0 -or $index + 1 -ge $Command.Count) {
        throw "Compose command에 option이 없습니다: $Option"
    }
    return [string]$Command[$index + 1]
}

function Get-PublishedPorts {
    param(
        [Parameter(Mandatory)]
        [object]$Service
    )

    return @($Service.ports | ForEach-Object {
        '{0}:{1}/{2}' -f $_.published, $_.target, $_.protocol
    } | Sort-Object)
}

function Assert-CommonSecurity {
    param(
        [Parameter(Mandatory)]
        [object]$Service,

        [Parameter(Mandatory)]
        [string]$Name
    )

    Assert-Equal $Service.init $true "$Name init"
    Assert-Equal $Service.read_only $true "$Name read_only"
    Assert-Equal $Service.restart 'unless-stopped' "$Name restart"
    Assert-Contains @($Service.cap_drop) 'ALL' "$Name cap_drop"
    Assert-Contains `
        @($Service.security_opt) `
        'no-new-privileges:true' `
        "$Name security_opt"
    Assert-Contains `
        @($Service.networks.PSObject.Properties.Name) `
        'control' `
        "$Name networks"
    Assert-Contains `
        @($Service.networks.PSObject.Properties.Name) `
        'edge' `
        "$Name networks"
    Assert-Equal $Service.healthcheck.test[0] 'CMD' "$Name healthcheck"
}

if (-not (Test-Path -LiteralPath $ComposeFile -PathType Leaf)) {
    throw "Compose 파일을 찾지 못했습니다: $ComposeFile"
}
if (-not (Test-Path -LiteralPath $EnvironmentFile -PathType Leaf)) {
    throw "Compose 환경 파일을 찾지 못했습니다: $EnvironmentFile"
}

$configOutput = @(& $DockerExecutable `
    compose `
    --file $ComposeFile `
    --env-file $EnvironmentFile `
    config `
    --format json)
if ($LASTEXITCODE -ne 0) {
    throw "docker compose config가 실패했습니다: $LASTEXITCODE"
}
$config = ($configOutput -join [Environment]::NewLine) | ConvertFrom-Json

$serviceNames = @($config.services.PSObject.Properties.Name | Sort-Object)
Assert-Equal $serviceNames.Count 3 'service count'
Assert-Equal $serviceNames[0] 'game-server-1' 'service 1'
Assert-Equal $serviceNames[1] 'game-server-2' 'service 2'
Assert-Equal $serviceNames[2] 'lobby-server' 'service 3'
Assert-Equal $config.networks.control.internal $true 'control network internal'

$lobby = $config.services.'lobby-server'
$game1 = $config.services.'game-server-1'
$game2 = $config.services.'game-server-2'

Assert-CommonSecurity $lobby 'lobby-server'
Assert-CommonSecurity $game1 'game-server-1'
Assert-CommonSecurity $game2 'game-server-2'

Assert-Equal `
    ((Get-PublishedPorts $lobby) -join ',') `
    '7000:7000/tcp' `
    'lobby published ports'
Assert-Equal `
    ((Get-PublishedPorts $game1) -join ',') `
    '7100:7100/tcp,7101:7101/udp' `
    'game-server-1 published ports'
Assert-Equal `
    ((Get-PublishedPorts $game2) -join ',') `
    '7200:7200/tcp,7201:7201/udp' `
    'game-server-2 published ports'

foreach ($case in @(
        [pscustomobject]@{
            service = $game1
            name = 'game-server-1'
            worker = '1'
            tcp = '7100'
            udp = '7101'
        },
        [pscustomobject]@{
            service = $game2
            name = 'game-server-2'
            worker = '2'
            tcp = '7200'
            udp = '7201'
        })) {
    $command = @($case.service.command)
    Assert-Equal `
        (Get-OptionValue $command '--lobby-control-host') `
        'lobby-server' `
        "$($case.name) control host"
    Assert-Equal `
        (Get-OptionValue $command '--worker-id') `
        $case.worker `
        "$($case.name) worker id"
    Assert-Equal `
        (Get-OptionValue $command '--advertise-host') `
        '127.0.0.1' `
        "$($case.name) advertise host"
    Assert-Equal `
        (Get-OptionValue $command '--game-tcp-port') `
        $case.tcp `
        "$($case.name) game TCP port"
    Assert-Equal `
        (Get-OptionValue $command '--game-udp-port') `
        $case.udp `
        "$($case.name) game UDP port"
    Assert-Equal `
        (Get-OptionValue $command '--replication-mode') `
        'interest-delta' `
        "$($case.name) replication mode"
}
