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
    $source = Add-ReferenceWalkablePathSmoothing $source
}
$names = @('GetNearestPointOnNavMesh','IsOnNavMesh','SmoothPath','ProjectPointOnTriangle','IsPointInTriangle','HasLineOfSight','IsLineOnNavMesh','GetDistance')
$methods = foreach ($name in $names) {
    $match = [regex]::Matches($source, "(?ms)^(Vec3|bool|void|float) NavMesh::$name\([^\r\n]*\)\s*\{.*?^\}")
    if ($match.Count -ne 1) { throw "Unexpected source navigation method: $name" }
    $match[0].Value
}
$fixture = @'
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <vector>
using namespace std;
struct Vec3 {
    float x=0, y=0, z=0;
    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec3 operator*(float b) const { return {x*b,y*b,z*b}; }
    float Dot(Vec3 b) const { return x*b.x+y*b.y+z*b.z; }
    float Length() const { return sqrt(Dot(*this)); }
    void Normalize() { const auto length=Length(); x/=length; y/=length; z/=length; }
    static float Distance(Vec3 a, Vec3 b) { return (a-b).Length(); }
};
Vec3 operator*(float value,Vec3 point) { return point*value; }
struct NavMeshTriangle { Vec3 vertices[3]; Vec3 normal{0,1,0}; };
// All synthetic triangles lie inside the original grid's origin cell.
struct Grid {
    unordered_map<unsigned long long, vector<int>> grid;
    unsigned long long GetKey(const Vec3&) const { return 0; }
};
struct NavMesh {
    vector<NavMeshTriangle> m_triangles;
    Grid m_spatialGrid;
    Vec3 GetNearestPointOnNavMesh(const Vec3&);
    bool IsOnNavMesh(const Vec3&,float);
    void SmoothPath(const vector<Vec3>&,vector<Vec3>&);
    Vec3 ProjectPointOnTriangle(const Vec3&,const NavMeshTriangle&);
    bool IsPointInTriangle(const Vec3&,const NavMeshTriangle&);
    bool HasLineOfSight(const Vec3&,const Vec3&);
    bool IsLineOnNavMesh(const Vec3&,const Vec3&,float stepSize=0.5f);
    float GetDistance(const Vec3&,const Vec3&);
};
'@
$checks = @'
void require(bool value, const char* message) { if (!value) { cerr << message << endl; exit(1); } }
void rectangle(NavMesh& nav,float left,float right,float bottom,float top) {
    const Vec3 a(left,0,bottom),b(right,0,bottom),c(right,0,top),d(left,0,top);
    nav.m_triangles.push_back({{a,b,c}});
    nav.m_triangles.push_back({{a,c,d}});
    nav.m_spatialGrid.grid[0].push_back(static_cast<int>(nav.m_triangles.size())-2);
    nav.m_spatialGrid.grid[0].push_back(static_cast<int>(nav.m_triangles.size())-1);
}
int main() {
    NavMesh corner;
    rectangle(corner,-2,0,0,4);
    rectangle(corner,-2,4,-2,0);
    const Vec3 start(-0.1f,0,0.1f), bend(-0.2f,0,-0.2f), end(2,0,-1);
    require(!corner.IsLineOnNavMesh(start,end), "Path smoothing accepted a shortcut outside the walkable corner");
    require(corner.IsLineOnNavMesh(start,bend) && corner.IsLineOnNavMesh(bend,end), "Valid corner detour was rejected");
    vector<Vec3> path;
    corner.SmoothPath({start,bend,end},path);
    require(any_of(path.begin(),path.end(),[](Vec3 p){ return p.x<0 && p.z<0; }), "Smoothing removed the necessary corner waypoint");
    for (size_t i=1;i<path.size();++i) require(corner.IsLineOnNavMesh(path[i-1],path[i]), "Smoothed path contains an unwalkable segment");
    require(corner.IsLineOnNavMesh(Vec3(-1,0,3),Vec3(-1,0,-1)), "Connected triangle boundaries interrupted a valid route");
    require(!corner.IsLineOnNavMesh(Vec3(-1,0,3),Vec3(3,0,3)), "Endpoint outside the navmesh was accepted");
    NavMesh gap;
    rectangle(gap,-2,-0.03f,-1,1);
    rectangle(gap,0.03f,2,-1,1);
    require(!gap.IsLineOnNavMesh(Vec3(-1.1f,0,0),Vec3(1.1f,0,0)), "A narrow hole fell between line samples");
    require(!gap.HasLineOfSight(Vec3(-0.04f,0,0),Vec3(0.04f,0,0)), "Short distance bypassed a real navigation hole");
    NavMesh straight;
    rectangle(straight,-2,2,-2,2);
    require(straight.IsLineOnNavMesh(Vec3(-1,0,-1),Vec3(1,0,1)), "Valid straight route was rejected");
    require(straight.IsLineOnNavMesh(Vec3(0,0,0),Vec3(0,0,0)), "Stationary walkable point was rejected");
    require(!straight.IsLineOnNavMesh(Vec3(4,0,4),Vec3(4,0,4)), "Stationary point outside the navmesh was accepted");
    for (auto& triangle:straight.m_triangles) swap(triangle.vertices[0],triangle.vertices[2]);
    require(straight.IsLineOnNavMesh(Vec3(-1,0,-1),Vec3(1,0,1)), "Triangle winding changed walkability");
}
'@
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('rnp-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    [IO.File]::WriteAllText((Join-Path $temporary 'main.cpp'), $fixture + "`n" + ($methods -join "`n") + "`n" + $checks, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $temporary 'CMakeLists.txt'), "cmake_minimum_required(VERSION 3.25)`nproject(ReferencePath LANGUAGES CXX)`nadd_executable(walkable_path main.cpp)`ntarget_compile_options(walkable_path PRIVATE /utf-8)`n")
    $build = Join-Path $temporary 'build'
    & $CMakeExecutable -S $temporary -B $build -G 'Visual Studio 17 2022' -A x64 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Navigation fixture configuration failed' }
    $compileOutput = & $CMakeExecutable --build $build --config Debug --target walkable_path 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Navigation fixture compilation failed: $($compileOutput -join [Environment]::NewLine)" }
    & (Join-Path $build 'Debug/walkable_path.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Walkable path smoothing failed' }
    Write-Output 'PASS: actual source smoothing retains walkable corner routes and rejects narrow holes, outside endpoints and disconnected segments'
} finally {
    $resolved = [IO.Path]::GetFullPath($temporary)
    $prefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $resolved) -notmatch '^rnp-[0-9a-f]{8}$') { throw 'Temporary cleanup boundary mismatch' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
