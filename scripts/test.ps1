[CmdletBinding()]
param(
    [ValidateSet('windows-msvc-debug')]
    [string]$Preset = 'windows-msvc-debug'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudioRoot = @(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath) -join ''
$visualStudioRoot = $visualStudioRoot.Trim()
$ctest = Join-Path $visualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
$buildDirectory = Join-Path $repositoryRoot 'out\build\windows-msvc-vs-debug'

& $ctest --test-dir $buildDirectory --build-config Debug --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "테스트가 실패했습니다. 종료 코드: $LASTEXITCODE"
}

$requiredClientTests = @(
    'Client.WarpSmoke',
    'Client.ShaderDeployment',
    'Client.AssetShaderDeployment',
    'Client.HybridGeometryShaderDeployment',
    'Client.HybridLightingShaderDeployment',
    'Client.HybridShadowShaderDeployment',
    'Client.CharacterAssetDeployment',
    'Client.FloorAssetDeployment',
    'Client.TextureAssetDeployment'
)

foreach ($testName in $requiredClientTests) {
    $escapedName = [regex]::Escape($testName)
    & $ctest `
        --test-dir $buildDirectory `
        --build-config Debug `
        --output-on-failure `
        --no-tests=error `
        -R "^$escapedName$"
    if ($LASTEXITCODE -ne 0) {
        throw "$testName 등록 검증이 실패했습니다. 종료 코드: $LASTEXITCODE"
    }
}
