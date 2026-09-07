[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot,
      [Parameter(Mandatory)][string]$CMakeExecutable)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath($RepositoryRoot)
$out = Join-Path $root ('out/file-io-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
try {
    foreach ($file in @('FileUtils.h','FileUtils.cpp','Define.h')) {
        Copy-Item -LiteralPath (Join-Path $root "game/Engine/$file") -Destination $out
    }
    Copy-Item -LiteralPath (Join-Path $root 'game/Client/GameRunOptions.h') -Destination $out
    Copy-Item -LiteralPath (Join-Path $root 'tests/game_file_io_fixture.cpp') -Destination (Join-Path $out 'main.cpp')
    [IO.File]::WriteAllText((Join-Path $out 'pch.h'), "#pragma once`n#include <windows.h>`n#include <cassert>`n#include <cstdint>`n#include <string>`n#include <stdexcept>`nusing namespace std;`nusing uint8=uint8_t;`nusing uint32=uint32_t;`n")
    [IO.File]::WriteAllText((Join-Path $out 'CMakeLists.txt'), "cmake_minimum_required(VERSION 3.25)`nproject(FileIO LANGUAGES CXX)`nadd_executable(file_io main.cpp FileUtils.cpp)`ntarget_compile_features(file_io PRIVATE cxx_std_17)`ntarget_compile_definitions(file_io PRIVATE UNICODE _UNICODE)`n")
    & $CMakeExecutable -S $out -B (Join-Path $out 'build') -G 'Visual Studio 17 2022' -A x64 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'File I/O fixture configure failed' }
    $buildOutput = & $CMakeExecutable --build (Join-Path $out 'build') --config Release 2>&1
    if ($LASTEXITCODE -ne 0) { throw "File I/O fixture build failed: $buildOutput" }
    Push-Location -LiteralPath $out
    try {
        & (Join-Path $out 'build/Release/file_io.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Release file I/O contract failed' }
    } finally { Pop-Location }
    'PASS: actual Release file I/O and run options'
} finally {
    $allowed = (Join-Path $root 'out') + [IO.Path]::DirectorySeparatorChar
    if (-not $out.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $out) -notmatch '^file-io-[0-9a-f]{32}$') { throw 'Fixture cleanup boundary mismatch' }
    Remove-Item -LiteralPath $out -Recurse -Force
}
