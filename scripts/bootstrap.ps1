[CmdletBinding()]
param(
    [ValidateSet('windows-msvc-debug')]
    [string]$Preset = 'windows-msvc-debug'
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer의 vswhere.exe를 찾지 못했습니다.'
}

$visualStudioRoot = @(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath) -join ''
$visualStudioRoot = $visualStudioRoot.Trim()
if ([string]::IsNullOrWhiteSpace($visualStudioRoot)) {
    throw 'Visual Studio 2022 C++ 도구를 찾지 못했습니다.'
}

$cmake = Join-Path $visualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$vcpkgRoot = Join-Path $visualStudioRoot 'VC\vcpkg'
$toolchain = Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'

foreach ($requiredPath in @($cmake, $toolchain)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "필수 도구를 찾지 못했습니다: $requiredPath"
    }
}

& $cmake --preset $Preset "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure가 실패했습니다. 종료 코드: $LASTEXITCODE"
}

Write-Output "구성 완료: $repositoryRoot ($Preset)"
