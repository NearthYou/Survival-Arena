[CmdletBinding()]
param([Parameter(Mandatory)][string]$Executable, [switch]$ValidateOnly,
      [ValidateSet('1366x768','1920x1080')][string]$Resolution = '1366x768')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$executablePath = (Resolve-Path -LiteralPath $Executable).Path
if ([IO.Path]::GetFileName($executablePath) -cne 'dxa_game.exe') {
    throw 'play_game requires the repository-built dxa_game.exe'
}
$validation = Start-Process -FilePath $executablePath -ArgumentList '--validate-runtime' -Wait -PassThru -WindowStyle Hidden
if ($validation.ExitCode -ne 0) { throw "Game runtime verification failed ($($validation.ExitCode))" }
Write-Output "Game runtime verified: $executablePath"
if (-not $ValidateOnly) {
    # The user requested an interactive game window.
    $arguments = @{}
    if ($Resolution -eq '1920x1080') { $arguments.ArgumentList = '--resolution=1920x1080' }
    $process = Start-Process @arguments -FilePath $executablePath -WorkingDirectory ([IO.Path]::GetTempPath()) -WindowStyle Normal -PassThru
    [pscustomobject]@{ process_id = $process.Id; executable = $executablePath; manual_approval = 'pending' }
}
