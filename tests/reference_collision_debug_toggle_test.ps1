[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot,
      [Parameter(Mandatory)][string]$CMakeExecutable)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$patcher = Join-Path $RepositoryRoot 'scripts/reference_game_patches.ps1'
if (-not (Test-Path -LiteralPath $patcher)) { throw 'Collision debug toggle patch is unavailable' }
. $patcher
$fixture = @'
#include <vector>
#include <stdexcept>
constexpr int VK_F3 = 0x72;
enum class KEY_TYPE { Other = 0 };
struct Input {
    int downKey = 0;
    bool GetButtonDown(KEY_TYPE key) { return downKey == static_cast<int>(key); }
} input;
#define INPUT (&input)
struct Collider {
    bool visible = true;
    const bool active;
    void SetVisible(bool value) { visible = value; }
};
struct Object {
    Collider* collider;
    Collider* GetCollider() const { return collider; }
};
struct Scene {
    int lateUpdates = 0;
    void LateUpdate() { ++lateUpdates; }
};
struct LumiaIsland : Scene {
    using Super = Scene;
    std::vector<Object*> objects;
    const std::vector<Object*>& GetObjects() const { return objects; }
    void LateUpdate();
};
void LumiaIsland::LateUpdate()
{
    Super::LateUpdate();
}
void require(bool value) { if (!value) throw std::runtime_error("collision visibility contract failed"); }
int main() {
    Collider player{true, true}, dormant{true, false}, spawned{true, true};
    Object first{&player}, second{&dormant}, decorative{nullptr}, dynamic{&spawned};
    LumiaIsland scene;
    scene.objects = {&first, &second, &decorative};
    scene.LateUpdate();
    require(!player.visible && !dormant.visible && player.active && !dormant.active);
    input.downKey = VK_F3;
    scene.LateUpdate();
    require(player.visible && dormant.visible);
    input.downKey = 0;
    scene.LateUpdate();
    require(player.visible && dormant.visible);
    scene.objects.push_back(&dynamic);
    scene.LateUpdate();
    require(spawned.visible);
    input.downKey = VK_F3;
    scene.LateUpdate();
    require(!player.visible && !dormant.visible && !spawned.visible);
    input.downKey = 'Q';
    scene.LateUpdate();
    require(!player.visible && player.active && !dormant.active && spawned.active);
    require(scene.lateUpdates == 6);
}
'@
$patched = Add-ReferenceCollisionDebugToggle $fixture
$message = ''
try { Add-ReferenceCollisionDebugToggle 'unrecognized scene implementation' | Out-Null } catch { $message = $_.Exception.Message }
if (-not $message.Contains('Unexpected collision debug hook site')) { throw 'Unknown source layout was accepted' }
$message = ''
try { Add-ReferenceCollisionDebugToggle $patched | Out-Null } catch { $message = $_.Exception.Message }
if (-not $message.Contains('already present')) { throw 'Duplicate toggle hook was accepted' }
$outputRoot = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'out'))
# Keep MSBuild's nested file-tracker paths below its legacy path limit.
$temporary = Join-Path $outputRoot ('rct-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $temporary -Force | Out-Null
try {
    [IO.File]::WriteAllText((Join-Path $temporary 'main.cpp'), $patched)
    [IO.File]::WriteAllText((Join-Path $temporary 'CMakeLists.txt'), "cmake_minimum_required(VERSION 3.25)`nproject(CollisionVisibility LANGUAGES CXX)`nadd_executable(collision_visibility main.cpp)`ntarget_compile_features(collision_visibility PRIVATE cxx_std_17)`n")
    $build = Join-Path $temporary 'build'
    & $CMakeExecutable -S $temporary -B $build -G 'Visual Studio 17 2022' -A x64 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Collision visibility fixture configuration failed' }
    & $CMakeExecutable --build $build --config Debug --target collision_visibility | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Collision visibility fixture compilation failed' }
    & (Join-Path $build 'Debug/collision_visibility.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Collision visibility behavior failed' }
    Write-Output 'PASS: hidden by default, F3 toggles on/off, other keys and held frames preserve state, late colliders inherit visibility, collision activation is untouched'
} finally {
    $resolved = [IO.Path]::GetFullPath($temporary)
    if (-not $resolved.StartsWith($outputRoot.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $resolved) -notmatch '^rct-[0-9a-f]{8}$') { throw 'Temporary cleanup boundary mismatch' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
