[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($RepositoryRoot)
$output = Join-Path $root ('out/probe-runner-test-' + [Guid]::NewGuid().ToString('N'))
$binaries = Join-Path $output 'runtime/Binaries'
New-Item -ItemType Directory -Path $binaries -Force | Out-Null
try {
    [IO.File]::WriteAllText((Join-Path $binaries 'bianca-skills.json'), '{"passed":true}')
    $message = ''
    try {
        & (Join-Path $root 'scripts/build_game_probe.ps1') -Scenario bianca -Character Bianca `
            -SdkRoot 'Z:/missing-sdk' -AssetRoot 'Z:/missing-assets' -BuildRoot $output -Run
    } catch { $message = $_.Exception.Message }
    if (-not $message.Contains('Existing probe result')) {
        throw "Runner must reject stale evidence before building or launching: $message"
    }
    if (Test-Path -LiteralPath (Join-Path $output 'include')) { throw 'Stale evidence rejection mutated the build' }
    'PASS: stale probe evidence is rejected before any build or launch'
} finally {
    $allowed = (Join-Path $root 'out') + [IO.Path]::DirectorySeparatorChar
    if (-not $output.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $output) -notmatch '^probe-runner-test-[0-9a-f]{32}$') { throw 'Probe fixture cleanup boundary mismatch' }
    Remove-Item -LiteralPath $output -Recurse -Force
}
