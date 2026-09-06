[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot,
      [Parameter(Mandatory)][string]$SourceFile,
      [Parameter(Mandatory)][string]$CMakeExecutable,
      [switch]$ApplyPatch)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = [Text.Encoding]::GetEncoding(949).GetString([IO.File]::ReadAllBytes($SourceFile))
if ($ApplyPatch) {
    . (Join-Path $RepositoryRoot 'scripts/reference_game_patches.ps1')
    $source = Add-ReferencePickingMatrixSync $source
}
$methods = [regex]::Matches($source, '(?ms)^void SceneObjectManager::UpdateQuadTree\(\)\s*\{.*?^\}')
if ($methods.Count -ne 1) { throw 'Unexpected source spatial-index update method' }
$fixture = @'
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <cmath>
#include <iostream>
#include <cstdlib>
using namespace std;
struct Vec3 {
    float x = 0;
    static const Vec3 Zero;
    static float Distance(Vec3 a, Vec3 b) { return abs(a.x - b.x); }
};
const Vec3 Vec3::Zero{};
struct Transform {
    Vec3 position{};
    Vec3 GetPosition() { return position; }
    Vec3 GetLocalRotation() { return {}; }
};
struct Camera {
    Transform transform;
    float viewPosition = 0;
    void UpdateMatrix() { viewPosition = transform.position.x; }
};
shared_ptr<Camera> mainCamera = make_shared<Camera>();
struct GameObject {
    Transform transform;
    bool isCamera = false;
    Transform* GetTransform() { return isCamera ? &mainCamera->transform : &transform; }
    shared_ptr<Camera> GetCamera() { return mainCamera; }
    void* GetCollider() { return isCamera ? nullptr : this; }
    bool GetActive() { return true; }
};
struct Viewport { float GetWidth() { return 1366; } float GetHeight() { return 768; } };
struct Graphics { Viewport GetViewport() { return {}; } } graphics;
#define GRAPHICS (&graphics)
struct QuadTree {
    float indexedScreenX = -999;
    int builds = 0;
    QuadTree(float, float) {}
    bool IsObjectVisible(shared_ptr<GameObject>, shared_ptr<Camera>) { return true; }
    void Clear() { indexedScreenX = -999; }
    void Insert(shared_ptr<GameObject> object) { indexedScreenX = object->GetTransform()->position.x - mainCamera->viewPosition; }
    void Build() { ++builds; }
};
struct SceneObjectManager {
    unique_ptr<QuadTree> m_quadTree;
    bool m_quadTreeDirty = true;
    vector<shared_ptr<GameObject>> m_gameObjects;
    shared_ptr<GameObject> cameraObject = make_shared<GameObject>();
    SceneObjectManager() { cameraObject->isCamera = true; }
    shared_ptr<GameObject> GetMainCamera() { return cameraObject; }
    void UpdateQuadTree();
};
'@
$checks = @'
void require(bool value, const char* message) { if (!value) { cerr << message << endl; exit(1); } }
int main() {
    SceneObjectManager manager;
    auto target = make_shared<GameObject>();
    target->transform.position.x = 30;
    manager.m_gameObjects.push_back(target);
    // The camera-follow Update has moved the transform, but Camera::LateUpdate
    // has not synchronized the view matrix yet: the real engine's call order.
    mainCamera->transform.position.x = 10;
    manager.UpdateQuadTree();
    require(manager.m_quadTree->indexedScreenX == 20, "First-frame picking cached the previous camera matrix");
    mainCamera->UpdateMatrix();
    manager.UpdateQuadTree();
    require(manager.m_quadTree->indexedScreenX == 20, "Stationary picking retained stale screen bounds");
    require(manager.m_quadTree->builds == 1, "Stationary scene must not rebuild every frame");
    mainCamera->transform.position.x = 20;
    manager.UpdateQuadTree();
    require(manager.m_quadTree->indexedScreenX == 10, "Moved camera picking is one frame behind");
    require(manager.m_quadTree->builds == 2, "Moved camera must rebuild the spatial index");
}
'@
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('rpm-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    [IO.File]::WriteAllText((Join-Path $temporary 'main.cpp'), $fixture + "`n" + $methods[0].Value + "`n" + $checks)
    [IO.File]::WriteAllText((Join-Path $temporary 'CMakeLists.txt'), "cmake_minimum_required(VERSION 3.25)`nproject(PickingMatrix LANGUAGES CXX)`nadd_executable(picking_matrix main.cpp)`ntarget_compile_options(picking_matrix PRIVATE /utf-8)`n")
    $build = Join-Path $temporary 'build'
    & $CMakeExecutable -S $temporary -B $build -G 'Visual Studio 17 2022' -A x64 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Picking matrix fixture configuration failed' }
    & $CMakeExecutable --build $build --config Debug --target picking_matrix | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Picking matrix fixture compilation failed' }
    & (Join-Path $build 'Debug/picking_matrix.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Picking matrix synchronization failed' }
    Write-Output 'PASS: actual source index update uses current camera matrices on initial and moved frames without rebuilding stationary frames'
} finally {
    $resolved = [IO.Path]::GetFullPath($temporary)
    $prefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $resolved) -notmatch '^rpm-[0-9a-f]{8}$') { throw 'Temporary cleanup boundary mismatch' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
