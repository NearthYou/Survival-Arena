[CmdletBinding()]
param(
    [ValidateSet('windows-game-debug', 'windows-game-release', 'windows-msvc-debug', 'legacy-windows-msvc-debug')]
    [string]$Preset = 'windows-game-debug'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudioRoot = @(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath) -join ''
$visualStudioRoot = $visualStudioRoot.Trim()
$ctest = Join-Path $visualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
$directory = switch ($Preset) {
    'windows-game-debug' { 'game-msvc-debug' }
    'windows-game-release' { 'game-msvc-release' }
    'legacy-windows-msvc-debug' { 'legacy-msvc-debug' }
    default { 'windows-msvc-vs-debug' }
}
$buildDirectory = Join-Path $repositoryRoot "out/build/$directory"

$configuration = if ($Preset -eq 'windows-game-release') { 'Release' } else { 'Debug' }
& $ctest --test-dir $buildDirectory --build-config $configuration --output-on-failure --no-tests=error
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
    'Client.HybridTransparentShaderDeployment',
    'Client.CharacterAssetDeployment',
    'Client.FloorAssetDeployment',
    'Client.TextureAssetDeployment'
)

if ($Preset -eq 'legacy-windows-msvc-debug') {
    $inventory = & $ctest --test-dir $buildDirectory -C Debug --show-only=json-v1 | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the legacy test inventory' }
    foreach ($testName in $requiredClientTests) {
        if ($inventory.tests.name -cnotcontains $testName) { throw "Legacy test missing: $testName" }
    }
}
