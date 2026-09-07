[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot,
      [Parameter(Mandatory)][string]$CMakeExecutable)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$header = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'scripts/reference_oracle/ReferenceFrameCapture.hpp') -Raw
$pending = $header.IndexOf('ReferencePendingFrame()')
if ($pending -lt 0) { throw 'Deferred capture queue missing' }
$queue = $header.Substring($header.LastIndexOf('inline', $pending))
$fixture = @'
#include <string>
#include <fstream>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <sstream>
using HRESULT = long;
std::vector<std::wstring> written;
HRESULT ReferenceWriteFrame(const wchar_t* path) { written.emplace_back(path); return 0; }
'@
$checks = @'
void require(bool value, const char* message) { if (!value) { std::cerr << message << std::endl; std::exit(1); } }
int main() {
    ReferenceFlushFrame();
    require(written.empty(), "An empty queue wrote a capture");
    std::wstring callerPath = L"expected-frame.bmp";
    ReferenceQueueFrame(callerPath.c_str());
    callerPath[0] = L'x';
    ReferenceFlushFrame();
    require(written.size() == 1 && written[0] == L"expected-frame.bmp", "Queued capture borrowed mutable filename storage");
    ReferenceFlushFrame();
    require(written.size() == 1, "A capture request was not consumed exactly once");
    {
        const std::wstring temporary = L"temporary-frame.bmp";
        ReferenceQueueFrame(temporary.c_str());
    }
    ReferenceFlushFrame();
    require(written.size() == 2 && written[1] == L"temporary-frame.bmp", "Queued filename did not survive the caller scope");
    ReferenceQueueFrame(L"superseded.bmp");
    ReferenceQueueFrame(L"latest.bmp");
    ReferenceFlushFrame();
    require(written.size() == 3 && written[2] == L"latest.bmp", "Single-frame queue did not retain its latest request");
    ReferenceQueueFrame(nullptr);
    ReferenceFlushFrame();
    require(written.size() == 3, "Null request produced a capture");
}
'@
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('rfc-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    [IO.File]::WriteAllText((Join-Path $temporary 'main.cpp'), $fixture + "`n" + $queue + "`n" + $checks)
    [IO.File]::WriteAllText((Join-Path $temporary 'CMakeLists.txt'), "cmake_minimum_required(VERSION 3.25)`nproject(ReferenceCapture LANGUAGES CXX)`nadd_executable(capture_queue main.cpp)`n")
    $build = Join-Path $temporary 'build'
    & $CMakeExecutable -S $temporary -B $build -G 'Visual Studio 17 2022' -A x64 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Capture fixture configuration failed' }
    & $CMakeExecutable --build $build --config Debug --target capture_queue | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Capture fixture compilation failed' }
    Push-Location -LiteralPath $temporary
    try {
        & (Join-Path $build 'Debug/capture_queue.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Deferred capture filename ownership failed' }
    } finally { Pop-Location }
    Write-Output 'PASS: deferred capture owns mutable and temporary filenames and consumes each request once'
} finally {
    $resolved = [IO.Path]::GetFullPath($temporary)
    $prefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Leaf $resolved) -notmatch '^rfc-[0-9a-f]{8}$') { throw 'Temporary cleanup boundary mismatch' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
