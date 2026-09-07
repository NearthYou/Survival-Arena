[CmdletBinding()]
param(
    [ValidateSet('windows-game-debug', 'windows-game-release', 'windows-msvc-debug', 'windows-msvc-release', 'legacy-windows-msvc-debug')]
    [string]$Preset = 'windows-game-debug'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudioRoot = @(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath) -join ''
$visualStudioRoot = $visualStudioRoot.Trim()
$cmake = Join-Path $visualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$configuration = if ($Preset -in @('windows-msvc-release','windows-game-release')) { 'Release' } else { 'Debug' }

Push-Location $repositoryRoot
try {
    & $cmake --build --preset $Preset --config $configuration
    if ($LASTEXITCODE -ne 0) {
        throw "빌드가 실패했습니다. 종료 코드: $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}
