param(
    [Parameter(Mandatory)]
    [string]$CMakeExecutable,

    [Parameter(Mandatory)]
    [string]$BuildDirectory,

    [Parameter(Mandatory)]
    [string]$Configuration
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$installRoot = Join-Path `
    $temporaryRoot `
    ('dxa-server-install-' + [Guid]::NewGuid().ToString('N'))

try {
    & $CMakeExecutable `
        --install $BuildDirectory `
        --config $Configuration `
        --prefix $installRoot `
        --component Server
    if ($LASTEXITCODE -ne 0) {
        throw "Server install 명령이 실패했습니다: $LASTEXITCODE"
    }

    $extension = if ($env:OS -eq 'Windows_NT') { '.exe' } else { '' }
    foreach ($name in @('dxa_lobby_server', 'dxa_game_server')) {
        $executable = Join-Path $installRoot "bin/$name$extension"
        if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
            throw "설치된 server executable을 찾지 못했습니다: $executable"
        }
    }
}
finally {
    if (Test-Path -LiteralPath $installRoot) {
        $resolvedInstallRoot = [IO.Path]::GetFullPath($installRoot)
        if (-not $resolvedInstallRoot.StartsWith(
                $temporaryRoot,
                [StringComparison]::OrdinalIgnoreCase) -or
            -not ([IO.Path]::GetFileName($resolvedInstallRoot)).StartsWith(
                'dxa-server-install-',
                [StringComparison]::Ordinal)) {
            throw "안전하지 않은 임시 install 경로입니다: $resolvedInstallRoot"
        }
        Remove-Item -LiteralPath $resolvedInstallRoot -Recurse -Force
    }
}
