[CmdletBinding()]
param(
    [string]$RepositoryRoot,
    [string]$CompileDatabaseDirectory,
    [string]$ClangUmlExecutable = 'C:\Program Files\clang-uml\bin\clang-uml.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$BasisCommitSha = '884e5e70d68d9fcf9dfe5638d97e06623da154c2'
$BasisTreeSha = 'a3d167d7ddb3fadfe5ce9a2dfea6f5a58b170890'
$RequiredToolVersion = '0.6.3'
$DiagramNames = @('engine', 'network')
$BasisInputPaths = @(
    'CMakeLists.txt',
    'CMakePresets.json',
    'vcpkg.json',
    'cmake',
    'engine',
    'apps',
    'protocol',
    'simulation',
    'tests/CMakeLists.txt',
    'tests/engine_resource_pool_test.cpp'
)
$RequiredClassNames = @{
    engine = @('EngineApp')
    network = @(
        'LobbyService',
        'WorkerRegistry',
        'GameServer',
        'AuthoritativeMatch',
        'GameSession',
        'SnapshotReplicator',
        'SnapshotReassembler'
    )
}

function Get-FullPath([string]$PathValue, [string]$BasePath)
{
    try
    {
        if ([System.IO.Path]::IsPathRooted($PathValue))
        {
            return [System.IO.Path]::GetFullPath($PathValue)
        }
        return [System.IO.Path]::GetFullPath((Join-Path $BasePath $PathValue))
    }
    catch
    {
        throw "Invalid path '$PathValue' relative to '$BasePath': $($_.Exception.Message)"
    }
}

function Get-Sha256Hex([string]$FilePath)
{
    $stream = [System.IO.File]::OpenRead($FilePath)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try
    {
        return ([System.BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
    }
    finally
    {
        $algorithm.Dispose()
        $stream.Dispose()
    }
}

function Get-Utf8StringSha256([string]$Value)
{
    $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($Value)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try
    {
        return ([System.BitConverter]::ToString($algorithm.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally
    {
        $algorithm.Dispose()
    }
}

function Get-NormalizedTextSha256([string]$FilePath)
{
    $content = (Get-Content -LiteralPath $FilePath -Raw).Replace("`r`n", "`n")
    return Get-Utf8StringSha256 $content
}

function Get-NormalizedManifestPath(
    [string]$AbsolutePath,
    [string]$RootPath)
{
    $absolute = [System.IO.Path]::GetFullPath($AbsolutePath).TrimEnd('\', '/')
    $root = [System.IO.Path]::GetFullPath($RootPath).TrimEnd('\', '/')
    if ($absolute.Equals($root, [System.StringComparison]::OrdinalIgnoreCase))
    {
        return '.'
    }
    if (Test-PathWithin $absolute $root)
    {
        return $absolute.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
    }
    return $absolute.Replace('\', '/')
}

function Get-WindowsFileId([string]$FilePath)
{
    $output = @(& fsutil.exe file queryfileid $FilePath 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0)
    {
        throw "File identity cannot be resolved: $FilePath`n$output"
    }
    $match = [regex]::Match($output, '0x[0-9a-fA-F]+')
    if (-not $match.Success)
    {
        throw "File identity output is unrecognized: $FilePath`n$output"
    }
    return $match.Value.ToLowerInvariant()
}

function Get-WindowsVolumeSerialNumber([string]$FilePath)
{
    if (-not ('DxaVolumeIdentity' -as [type]))
    {
        Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

public static class DxaVolumeIdentity
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool GetVolumePathName(
        string fileName,
        StringBuilder volumePathName,
        int bufferLength);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool GetVolumeInformation(
        string rootPathName,
        StringBuilder volumeNameBuffer,
        int volumeNameSize,
        out uint volumeSerialNumber,
        out uint maximumComponentLength,
        out uint fileSystemFlags,
        StringBuilder fileSystemNameBuffer,
        int fileSystemNameSize);

    public static uint SerialNumber(string fileName)
    {
        var root = new StringBuilder(260);
        if (!GetVolumePathName(fileName, root, root.Capacity))
            throw new Win32Exception(Marshal.GetLastWin32Error());
        uint serial;
        uint maximumComponentLength;
        uint flags;
        var volumeName = new StringBuilder(260);
        var fileSystemName = new StringBuilder(260);
        if (!GetVolumeInformation(
                root.ToString(),
                volumeName,
                volumeName.Capacity,
                out serial,
                out maximumComponentLength,
                out flags,
                fileSystemName,
                fileSystemName.Capacity))
            throw new Win32Exception(Marshal.GetLastWin32Error());
        return serial;
    }
}
'@
    }
    $serial = [DxaVolumeIdentity]::SerialNumber($FilePath)
    return ('0x{0:x8}' -f $serial)
}

function Get-WindowsFileIdentity([string]$FilePath)
{
    return [pscustomobject][ordered]@{
        volumeSerialNumber = Get-WindowsVolumeSerialNumber $FilePath
        fileId = Get-WindowsFileId $FilePath
        sha256 = Get-Sha256Hex $FilePath
    }
}

function Test-PathWithin([string]$CandidatePath, [string]$ParentPath)
{
    $candidate = [System.IO.Path]::GetFullPath($CandidatePath).TrimEnd('\', '/')
    $parent = [System.IO.Path]::GetFullPath($ParentPath).TrimEnd('\', '/')
    if ($candidate.Equals($parent, [System.StringComparison]::OrdinalIgnoreCase))
    {
        return $true
    }
    return $candidate.StartsWith(
        "$parent$([System.IO.Path]::DirectorySeparatorChar)",
        [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-RepositoryRelativePath([string]$AbsolutePath, [string]$RootPath)
{
    $absolute = [System.IO.Path]::GetFullPath($AbsolutePath)
    $root = [System.IO.Path]::GetFullPath($RootPath).TrimEnd('\', '/')
    if (-not (Test-PathWithin $absolute $root) -or $absolute.Length -le $root.Length)
    {
        throw "Path is not a repository file: $absolute"
    }
    return $absolute.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
}

function Invoke-Git([string[]]$Arguments, [switch]$Capture)
{
    if ($Capture)
    {
        $output = @(& git -C $script:ResolvedRepositoryRoot @Arguments 2>&1) -join "`n"
        if ($LASTEXITCODE -ne 0)
        {
            throw "git $($Arguments -join ' ') failed:`n$output"
        }
        return $output.Trim()
    }

    & git -C $script:ResolvedRepositoryRoot @Arguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "git $($Arguments -join ' ') failed with exit code $LASTEXITCODE"
    }
}

function Invoke-GitAt(
    [string]$RootPath,
    [string[]]$Arguments,
    [switch]$Capture)
{
    if ($Capture)
    {
        $output = @(& git -C $RootPath @Arguments 2>&1) -join "`n"
        if ($LASTEXITCODE -ne 0)
        {
            throw "git $($Arguments -join ' ') failed:`n$output"
        }
        return $output.Trim()
    }

    & git -C $RootPath @Arguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "git $($Arguments -join ' ') failed with exit code $LASTEXITCODE"
    }
}

function Get-FlattenedElements([object[]]$Elements)
{
    $result = [System.Collections.Generic.List[object]]::new()
    foreach ($element in @($Elements))
    {
        if ($null -ne $element -and $null -ne $element.PSObject.Properties['elements'])
        {
            foreach ($nested in @(Get-FlattenedElements @($element.elements)))
            {
                $result.Add($nested)
            }
        }
        elseif ($null -ne $element)
        {
            $result.Add($element)
        }
    }
    return @($result)
}

function Get-CMakeCacheValue(
    [string]$CacheText,
    [string]$Name)
{
    $match = [regex]::Match(
        $CacheText,
        "(?m)^$([regex]::Escape($Name)):[^=]+=(.+?)\r?$")
    if (-not $match.Success)
    {
        throw "CMake cache does not declare $Name"
    }
    return $match.Groups[1].Value.Trim()
}

function Test-DiagramTranslationUnit([string]$RelativePath)
{
    $normalized = $RelativePath.Replace('\', '/')
    return $normalized -match '^engine/src/.+\.cpp$' -or
        $normalized -match '^apps/(game_client|game_server|lobby_server)/src/.+\.cpp$' -or
        $normalized -eq 'tests/engine_resource_pool_test.cpp'
}

function Assert-CompileDatabase(
    [string]$DatabaseDirectory,
    [string]$RootPath,
    [string]$BasisSha)
{
    $resolvedRoot = (Resolve-Path -LiteralPath $RootPath).Path
    $resolvedDatabaseDirectory = (Resolve-Path -LiteralPath $DatabaseDirectory).Path
    if (-not (Test-PathWithin $resolvedDatabaseDirectory $resolvedRoot))
    {
        throw "Compile database directory must stay inside the repository: $resolvedDatabaseDirectory"
    }

    $databasePath = Join-Path $resolvedDatabaseDirectory 'compile_commands.json'
    $cachePath = Join-Path $resolvedDatabaseDirectory 'CMakeCache.txt'
    $ninjaPath = Join-Path $resolvedDatabaseDirectory 'build.ninja'
    if (-not (Test-Path -LiteralPath $databasePath -PathType Leaf))
    {
        throw "Compilation database is missing: $databasePath"
    }
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf))
    {
        throw "CMake cache is missing beside compilation database: $cachePath"
    }
    if (-not (Test-Path -LiteralPath $ninjaPath -PathType Leaf))
    {
        throw "Ninja build graph is missing beside compilation database: $ninjaPath"
    }

    $cacheText = Get-Content -LiteralPath $cachePath -Raw
    $cacheSourceRoot = Get-FullPath (Get-CMakeCacheValue $cacheText 'CMAKE_HOME_DIRECTORY') $resolvedDatabaseDirectory
    if (-not $cacheSourceRoot.Equals(
            $resolvedRoot,
            [System.StringComparison]::OrdinalIgnoreCase))
    {
        throw "Compilation database source root mismatch: $cacheSourceRoot"
    }
    $generator = Get-CMakeCacheValue $cacheText 'CMAKE_GENERATOR'
    if ($generator -ne 'Ninja')
    {
        throw "CMake generator must be Ninja, found: $generator"
    }
    $compilerPath = Get-FullPath (Get-CMakeCacheValue $cacheText 'CMAKE_CXX_COMPILER') $resolvedDatabaseDirectory
    if ([System.IO.Path]::GetFileName($compilerPath) -ne 'cl.exe' -or
        -not (Test-Path -LiteralPath $compilerPath -PathType Leaf))
    {
        throw "CMake CXX compiler must be an existing MSVC cl.exe: $compilerPath"
    }
    $compilerPath = (Resolve-Path -LiteralPath $compilerPath).Path
    $cacheCompilerIdentity = Get-WindowsFileIdentity $compilerPath
    $makeProgram = Get-FullPath (Get-CMakeCacheValue $cacheText 'CMAKE_MAKE_PROGRAM') $resolvedDatabaseDirectory
    if ([System.IO.Path]::GetFileName($makeProgram) -ne 'ninja.exe' -or
        -not (Test-Path -LiteralPath $makeProgram -PathType Leaf))
    {
        throw "CMake make program must be an existing ninja.exe: $makeProgram"
    }
    $makeProgram = (Resolve-Path -LiteralPath $makeProgram).Path
    $cmakeCommand = Get-FullPath (Get-CMakeCacheValue $cacheText 'CMAKE_COMMAND') $resolvedDatabaseDirectory
    if ([System.IO.Path]::GetFileName($cmakeCommand) -ne 'cmake.exe' -or
        -not (Test-Path -LiteralPath $cmakeCommand -PathType Leaf))
    {
        throw "CMAKE_COMMAND must be an existing cmake.exe: $cmakeCommand"
    }
    $cmakeCommand = (Resolve-Path -LiteralPath $cmakeCommand).Path
    $toolchain = Get-FullPath (Get-CMakeCacheValue $cacheText 'CMAKE_TOOLCHAIN_FILE') $resolvedDatabaseDirectory
    if ([System.IO.Path]::GetFileName($toolchain) -ne 'vcpkg.cmake' -or
        -not (Test-Path -LiteralPath $toolchain -PathType Leaf))
    {
        throw "CMAKE_TOOLCHAIN_FILE must be an existing vcpkg.cmake: $toolchain"
    }
    $toolchain = (Resolve-Path -LiteralPath $toolchain).Path
    $vcpkgInstalled = Get-FullPath (Get-CMakeCacheValue $cacheText 'VCPKG_INSTALLED_DIR') $resolvedDatabaseDirectory
    if (-not (Test-PathWithin $vcpkgInstalled $resolvedDatabaseDirectory) -or
        -not (Test-Path -LiteralPath $vcpkgInstalled -PathType Container))
    {
        throw "VCPKG_INSTALLED_DIR must be an owned directory inside the build root: $vcpkgInstalled"
    }

    $inputFiles = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
    foreach ($relativePath in $BasisInputPaths)
    {
        $inputPath = Join-Path $resolvedRoot $relativePath
        if (Test-Path -LiteralPath $inputPath -PathType Leaf)
        {
            $inputFiles.Add((Get-Item -LiteralPath $inputPath))
        }
        elseif (Test-Path -LiteralPath $inputPath -PathType Container)
        {
            foreach ($file in @(Get-ChildItem -LiteralPath $inputPath -File -Recurse))
            {
                $inputFiles.Add($file)
            }
        }
    }
    $latestInputWrite = ($inputFiles | Measure-Object -Property LastWriteTimeUtc -Maximum).Maximum
    if ($null -eq $latestInputWrite -or
        (Get-Item -LiteralPath $cachePath).LastWriteTimeUtc -lt $latestInputWrite -or
        (Get-Item -LiteralPath $databasePath).LastWriteTimeUtc -lt $latestInputWrite)
    {
        throw 'CMake cache or compile_commands.json is stale relative to consumed build inputs'
    }

    $parsedDatabase = Get-Content -LiteralPath $databasePath -Raw | ConvertFrom-Json
    if ($parsedDatabase -is [System.Array])
    {
        [object[]]$entries = $parsedDatabase
    }
    else
    {
        [object[]]$entries = @($parsedDatabase)
    }
    if ($entries.Count -eq 0)
    {
        throw 'Compilation database must contain at least one translation unit'
    }

    $projectEntryCount = 0
    $usedSources = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    $normalizedVcpkgRoot = $vcpkgInstalled.Replace('\', '/').ToLowerInvariant().TrimEnd('/')
    foreach ($entry in $entries)
    {
        if ([string]::IsNullOrWhiteSpace([string]$entry.directory) -or
            [string]::IsNullOrWhiteSpace([string]$entry.file))
        {
            throw 'Compilation database entries require directory and file'
        }

        $entryDirectory = Get-FullPath ([string]$entry.directory) $resolvedDatabaseDirectory
        if (-not $entryDirectory.Equals(
                $resolvedDatabaseDirectory,
                [System.StringComparison]::OrdinalIgnoreCase))
        {
            throw "Compilation command directory must equal the owned build directory: $entryDirectory"
        }

        $sourcePath = Get-FullPath ([string]$entry.file) $entryDirectory
        if (-not (Test-PathWithin $sourcePath $resolvedRoot))
        {
            throw "Compilation database points outside this source root: $sourcePath"
        }
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf))
        {
            throw "Compilation database source file is missing: $sourcePath"
        }
        $projectEntryCount += 1

        $relativeSource = Get-RepositoryRelativePath $sourcePath $resolvedRoot
        if (-not (Test-DiagramTranslationUnit $relativeSource))
        {
            continue
        }
        if (-not $usedSources.Add($relativeSource))
        {
            throw "Compilation database contains a duplicate selected translation unit: $relativeSource"
        }
        $null = Invoke-GitAt $resolvedRoot @('ls-files', '--error-unmatch', '--', $relativeSource)
        Invoke-GitAt $resolvedRoot @('cat-file', '-e', "${BasisSha}:$relativeSource")

        $command = [string]$entry.command
        if ([string]::IsNullOrWhiteSpace($command) -or
            $command -notmatch '(?i)^(?:"[^"]*[\\/]cl\.exe"|[^\s"]*[\\/]cl\.exe)\s' -or
            $command -notmatch '(?i)(?:^|\s)/TP(?:\s|$)' -or
            $command.Replace('\', '/').ToLowerInvariant() -notlike "*$($relativeSource.ToLowerInvariant())*")
        {
            throw "Selected translation unit does not have a recognizable MSVC C++ command: $relativeSource"
        }
        $compilerMatch = [regex]::Match($command, '^\s*(?:"([^"]+)"|(\S+))')
        $commandCompilerValue = if ($compilerMatch.Groups[1].Success)
        {
            $compilerMatch.Groups[1].Value
        }
        else
        {
            $compilerMatch.Groups[2].Value
        }
        if (-not [System.IO.Path]::IsPathRooted($commandCompilerValue) -or
            -not (Test-Path -LiteralPath $commandCompilerValue -PathType Leaf))
        {
            throw "Compilation command compiler is unresolved or relative for ${relativeSource}: $commandCompilerValue"
        }
        $commandCompiler = (Resolve-Path -LiteralPath $commandCompilerValue).Path
        $commandCompilerIdentity = Get-WindowsFileIdentity $commandCompiler
        if ($commandCompilerIdentity.volumeSerialNumber -ne $cacheCompilerIdentity.volumeSerialNumber -or
            $commandCompilerIdentity.fileId -ne $cacheCompilerIdentity.fileId -or
            $commandCompilerIdentity.sha256 -ne $cacheCompilerIdentity.sha256)
        {
            throw "Compilation command compiler does not match the normalized CMake cache compiler for ${relativeSource}: $commandCompiler"
        }
        foreach ($dependencyMatch in [regex]::Matches(
                $command.Replace('\', '/'),
                '(?i)[A-Z]:/[^\s"]*vcpkg_installed[^\s"]*'))
        {
            $dependencyPath = $dependencyMatch.Value.ToLowerInvariant()
            if (-not $dependencyPath.StartsWith($normalizedVcpkgRoot))
            {
                throw "Compilation command uses a mixed dependency root for ${relativeSource}: $($dependencyMatch.Value)"
            }
        }
    }

    $expectedOutput = Invoke-GitAt $resolvedRoot @(
        'ls-tree',
        '-r',
        '--name-only',
        $BasisSha,
        '--',
        'engine/src',
        'apps/game_client/src',
        'apps/game_server/src',
        'apps/lobby_server/src',
        'tests/engine_resource_pool_test.cpp'
    ) -Capture
    $expectedSources = @($expectedOutput -split "`r?`n" | Where-Object {
            -not [string]::IsNullOrWhiteSpace($_) -and (Test-DiagramTranslationUnit $_)
        })
    $missingSources = @($expectedSources | Where-Object { -not $usedSources.Contains($_) })
    $unexpectedSources = @($usedSources | Where-Object { $expectedSources -notcontains $_ })
    if ($missingSources.Count -gt 0 -or $unexpectedSources.Count -gt 0)
    {
        throw "Compilation database selected source set does not match basis. Missing: $($missingSources -join ', '); unexpected: $($unexpectedSources -join ', ')"
    }

    [string[]]$selectedPaths = @($usedSources)
    [System.Array]::Sort($selectedPaths, [System.StringComparer]::Ordinal)
    $cmakeVersionOutput = @(& $cmakeCommand --version 2>&1) -join "`n"
    if ($cmakeVersionOutput -notmatch '(?m)^cmake version [0-9]+\.[0-9]+\.[^\r\n]+$')
    {
        throw "CMake version cannot be identified: $cmakeVersionOutput"
    }
    $cmakeVersion = [regex]::Match($cmakeVersionOutput, '(?m)^cmake version [^\r\n]+$').Value
    $ninjaVersion = (@(& $makeProgram --version 2>&1) -join "`n").Trim()
    if ($ninjaVersion -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$')
    {
        throw "Ninja version cannot be identified: $ninjaVersion"
    }
    $previousErrorActionPreference = $ErrorActionPreference
    try
    {
        $ErrorActionPreference = 'Continue'
        $msvcOutput = @(& $compilerPath 2>&1) -join "`n"
    }
    finally
    {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $msvcMatch = [regex]::Match($msvcOutput, '\b19\.[0-9]+\.[0-9]+(?:\.[0-9]+)?\b')
    if (-not $msvcMatch.Success)
    {
        throw "MSVC version cannot be identified: $msvcOutput"
    }

    Write-Host "validated compile database: $projectEntryCount translation units, $($usedSources.Count) selected"
    return [pscustomobject][ordered]@{
        DatabasePath = $databasePath
        CachePath = $cachePath
        TotalTranslationUnits = $projectEntryCount
        SelectedPaths = $selectedPaths
        Generator = $generator
        SourceRoot = $cacheSourceRoot
        BuildDirectory = $resolvedDatabaseDirectory
        Compiler = $compilerPath
        CompilerIdentity = $cacheCompilerIdentity
        MakeProgram = $makeProgram
        CmakeCommand = $cmakeCommand
        Toolchain = $toolchain
        VcpkgInstalled = $vcpkgInstalled
        CmakeVersion = $cmakeVersion
        NinjaVersion = $ninjaVersion
        MsvcVersion = $msvcMatch.Value
    }
}

function Assert-RawClassDiagram([string]$DiagramName, [string]$JsonPath)
{
    $document = Get-Content -LiteralPath $JsonPath -Raw | ConvertFrom-Json
    if ($document.name -ne $DiagramName -or $document.diagram_type -ne 'class')
    {
        throw "$DiagramName output is not a raw clang-uml class document"
    }
    if ($document.metadata.clang_uml_version -ne $RequiredToolVersion)
    {
        throw "$DiagramName clang-uml metadata version mismatch"
    }
    if ([string]::IsNullOrWhiteSpace([string]$document.metadata.llvm_version))
    {
        throw "$DiagramName LLVM metadata is missing"
    }
    if (-not ([string]$document.title).Contains($BasisCommitSha))
    {
        throw "$DiagramName title does not contain the canonical basis SHA"
    }
    foreach ($forbiddenField in @('schemaVersion', 'basisCommitSha', 'nodes', 'edges'))
    {
        if ($null -ne $document.PSObject.Properties[$forbiddenField])
        {
            throw "$DiagramName contains transformed field '$forbiddenField'; raw clang-uml JSON is required"
        }
    }

    $elements = @(Get-FlattenedElements @($document.elements))
    $classes = @($elements | Where-Object { $_.type -eq 'class' })
    $relationships = @($document.relationships)
    if ($classes.Count -eq 0)
    {
        throw "$DiagramName contains no AST classes"
    }
    if ($relationships.Count -eq 0)
    {
        throw "$DiagramName contains no AST relationships"
    }

    $names = @($classes | ForEach-Object { [string]$_.name })
    foreach ($requiredName in $RequiredClassNames[$DiagramName])
    {
        if ($names -notcontains $requiredName)
        {
            throw "$DiagramName is missing required class: $requiredName"
        }
    }
    if ($DiagramName -eq 'engine')
    {
        if (@($names | Where-Object { $_ -like '*RuntimeScene*' }).Count -eq 0)
        {
            throw 'engine is missing the RuntimeScene boundary'
        }
        if (@($names | Where-Object { $_ -like '*Renderer' }).Count -eq 0)
        {
            throw 'engine is missing a renderer boundary'
        }
        if (@($names | Where-Object { $_ -like 'Resource*' }).Count -eq 0)
        {
            throw 'engine is missing a resource boundary'
        }
    }

    foreach ($classElement in $classes)
    {
        $sourceValue = [string]$classElement.source_location.file
        if ([string]::IsNullOrWhiteSpace($sourceValue) -or [System.IO.Path]::IsPathRooted($sourceValue))
        {
            throw "$DiagramName class '$($classElement.name)' requires a repository-relative source path"
        }
        $sourcePath = Get-FullPath $sourceValue $script:ResolvedRepositoryRoot
        if (-not (Test-PathWithin $sourcePath $script:ResolvedRepositoryRoot))
        {
            throw "$DiagramName class source escapes repository root: $sourceValue"
        }
        $relativePath = Get-RepositoryRelativePath $sourcePath $script:ResolvedRepositoryRoot
        if ($relativePath -match '^(tests?|third_party)/')
        {
            throw "$DiagramName contains a test or third-party class: $relativePath"
        }
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf))
        {
            throw "$DiagramName class source is absent from current checkout: $relativePath"
        }
        Invoke-Git @('cat-file', '-e', "${BasisCommitSha}:$relativePath")
    }

    return [pscustomobject]@{
        Name = $DiagramName
        ClassCount = $classes.Count
        RelationshipCount = $relationships.Count
    }
}

function Assert-RepositoryBasisInputs(
    [string]$RootPath,
    [string]$BasisSha)
{
    $resolvedRoot = (Resolve-Path -LiteralPath $RootPath).Path
    foreach ($relativePath in $BasisInputPaths)
    {
        $currentPath = Join-Path $resolvedRoot $relativePath
        if (-not (Test-Path -LiteralPath $currentPath))
        {
            throw "Required basis input is missing from current checkout: $relativePath"
        }
        Invoke-GitAt $resolvedRoot @('cat-file', '-e', "${BasisSha}:$relativePath")
        $tracked = Invoke-GitAt $resolvedRoot @('ls-files', '--', $relativePath) -Capture
        if ([string]::IsNullOrWhiteSpace($tracked))
        {
            throw "Required basis input is not tracked: $relativePath"
        }
    }

    $changedArguments = @(
        'diff',
        '--name-only',
        $BasisSha,
        '--'
    ) + $BasisInputPaths
    $changed = Invoke-GitAt $resolvedRoot $changedArguments -Capture
    if (-not [string]::IsNullOrWhiteSpace($changed))
    {
        throw "Consumed repository input differs from basis ${BasisSha}:`n$changed"
    }

    $untrackedArguments = @(
        'ls-files',
        '--others',
        '--exclude-standard',
        '--'
    ) + $BasisInputPaths
    $untracked = Invoke-GitAt $resolvedRoot $untrackedArguments -Capture
    if (-not [string]::IsNullOrWhiteSpace($untracked))
    {
        throw "Untracked file exists inside a consumed source scope:`n$untracked"
    }
}

function Assert-VcpkgInstalledCompleteness(
    [string]$VcpkgInstalledRoot,
    [string]$StatusPath,
    [string[]]$MetadataPaths)
{
    $statusText = (Get-Content -LiteralPath $StatusPath -Raw).Replace("`r`n", "`n")
    $installedPackages = @{}
    foreach ($paragraph in @([regex]::Split($statusText.Trim(), "`n`n+")))
    {
        $fields = @{}
        foreach ($line in @($paragraph -split "`n"))
        {
            $match = [regex]::Match($line, '^([^:]+): (.*)$')
            if ($match.Success)
            {
                $fields[$match.Groups[1].Value] = $match.Groups[2].Value
            }
        }
        if ($fields.ContainsKey('Status') -and
            $fields.ContainsKey('Architecture') -and
            $fields.ContainsKey('Abi') -and
            $fields['Status'] -eq 'install ok installed' -and
            $fields['Architecture'] -eq 'x64-windows' -and
            ([string]$fields['Abi']) -match '^[0-9a-f]{64}$')
        {
            $packageName = [string]$fields['Package']
            if ([string]::IsNullOrWhiteSpace($packageName) -or
                $installedPackages.ContainsKey($packageName))
            {
                throw "Installed vcpkg status contains a duplicate or empty base package: $packageName"
            }
            $installedPackages[$packageName] = [string]$fields['Abi']
        }
    }
    if ($installedPackages.Count -ne 71)
    {
        throw "Installed vcpkg status must contain exactly 71 base packages, found $($installedPackages.Count)"
    }

    $listPaths = @($MetadataPaths | Where-Object {
            $_.StartsWith('vcpkg/info/', [System.StringComparison]::Ordinal) -and
            $_.EndsWith('.list', [System.StringComparison]::Ordinal)
        })
    $abiPaths = @($MetadataPaths | Where-Object {
            $_.StartsWith('x64-windows/share/', [System.StringComparison]::Ordinal) -and
            $_.EndsWith('/vcpkg_abi_info.txt', [System.StringComparison]::Ordinal)
        })
    if ($listPaths.Count -ne 71 -or $abiPaths.Count -ne 71)
    {
        throw "Installed vcpkg metadata must contain 71 list and 71 ABI files; found $($listPaths.Count) and $($abiPaths.Count)"
    }

    $matchedLists = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $matchedAbis = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($packageName in @($installedPackages.Keys | Sort-Object))
    {
        $prefix = "vcpkg/info/${packageName}_"
        $lists = @($listPaths | Where-Object {
                $_.StartsWith($prefix, [System.StringComparison]::Ordinal) -and
                $_.EndsWith('_x64-windows.list', [System.StringComparison]::Ordinal)
            })
        $abiPath = "x64-windows/share/${packageName}/vcpkg_abi_info.txt"
        $abis = @($abiPaths | Where-Object { $_ -eq $abiPath })
        if ($lists.Count -ne 1)
        {
            throw "Installed vcpkg package must have exactly one list file: $packageName"
        }
        if ($abis.Count -ne 1)
        {
            throw "Installed vcpkg package must have exactly one ABI metadata file: $packageName"
        }
        [void]$matchedLists.Add($lists[0])
        [void]$matchedAbis.Add($abiPath)
        $listContent = (Get-Content -LiteralPath (Join-Path $VcpkgInstalledRoot $lists[0]) -Raw).Replace("`r`n", "`n")
        if (-not $listContent.Contains("${abiPath}`n"))
        {
            throw "Installed vcpkg list omits its ABI metadata path: $packageName"
        }
        $abiHash = Get-Sha256Hex (Join-Path $VcpkgInstalledRoot $abiPath)
        if ($abiHash -ne $installedPackages[$packageName])
        {
            throw "Installed vcpkg ABI metadata does not match status: $packageName"
        }
    }
    if ($matchedLists.Count -ne $listPaths.Count -or $matchedAbis.Count -ne $abiPaths.Count)
    {
        throw 'Installed vcpkg metadata contains a list or ABI file not represented by status'
    }
}

function Get-VcpkgInstalledProvenance([string]$VcpkgInstalledRoot)
{
    $statusPath = Join-Path $VcpkgInstalledRoot 'vcpkg\status'
    $infoDirectory = Join-Path $VcpkgInstalledRoot 'vcpkg\info'
    $shareDirectory = Join-Path $VcpkgInstalledRoot 'x64-windows\share'
    if (-not (Test-Path -LiteralPath $statusPath -PathType Leaf))
    {
        throw "Installed vcpkg status is missing: $statusPath"
    }
    if (-not (Test-Path -LiteralPath $infoDirectory -PathType Container))
    {
        throw "Installed vcpkg info directory is missing: $infoDirectory"
    }
    if (-not (Test-Path -LiteralPath $shareDirectory -PathType Container))
    {
        throw "Installed vcpkg share directory is missing: $shareDirectory"
    }

    $metadataPaths = [System.Collections.Generic.List[string]]::new()
    foreach ($file in @(Get-ChildItem -LiteralPath $infoDirectory -Filter '*.list' -File))
    {
        $metadataPaths.Add((Get-RepositoryRelativePath $file.FullName $VcpkgInstalledRoot))
    }
    foreach ($file in @(Get-ChildItem -LiteralPath $shareDirectory -Filter 'vcpkg_abi_info.txt' -File -Recurse))
    {
        $metadataPaths.Add((Get-RepositoryRelativePath $file.FullName $VcpkgInstalledRoot))
    }
    [string[]]$sortedPaths = @($metadataPaths)
    [System.Array]::Sort($sortedPaths, [System.StringComparer]::Ordinal)
    if ($sortedPaths.Count -eq 0)
    {
        throw 'Installed vcpkg package metadata set must not be empty'
    }
    Assert-VcpkgInstalledCompleteness `
        -VcpkgInstalledRoot $VcpkgInstalledRoot `
        -StatusPath $statusPath `
        -MetadataPaths $sortedPaths
    $entries = @($sortedPaths | ForEach-Object {
            [pscustomobject][ordered]@{
                path = $_
                sha256 = Get-Sha256Hex (Join-Path $VcpkgInstalledRoot $_)
            }
        })
    $setMaterial = ($entries | ForEach-Object { "$($_.path)`0$($_.sha256)`n" }) -join ''
    return [pscustomobject][ordered]@{
        status = [pscustomobject][ordered]@{
            path = 'vcpkg/status'
            sha256 = Get-Sha256Hex $statusPath
        }
        metadataFileCount = $entries.Count
        metadataSetSha256 = Get-Utf8StringSha256 $setMaterial
        metadataFiles = $entries
    }
}

function Replace-OrdinalIgnoreCase(
    [string]$Value,
    [string]$Search,
    [string]$Replacement)
{
    if ([string]::IsNullOrEmpty($Search))
    {
        return $Value
    }
    $replacementValue = $Replacement
    return [regex]::Replace(
        $Value,
        [regex]::Escape($Search),
        { param($match) $replacementValue },
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
}

function Convert-ToSnapshotText(
    [string]$Value,
    [object[]]$RootReplacements)
{
    $normalized = $Value.Replace("`r`n", "`n").Replace('\', '/')
    foreach ($replacement in @($RootReplacements | Sort-Object { $_.source.Length } -Descending))
    {
        $source = ([string]$replacement.source).Replace('\', '/')
        $normalized = Replace-OrdinalIgnoreCase $normalized $source ([string]$replacement.token)
    }
    return $normalized
}

function Remove-HostSpecificCMakeCacheEntries([string]$Value)
{
    $lines = [regex]::Split($Value, "`n")
    $filtered = @($lines | Where-Object {
            $_ -notmatch '^(?i:(?:SITE|COMPUTERNAME|HOSTNAME|HOST)(?:-ADVANCED)?):[^=]*='
        })
    return $filtered -join "`n"
}

function Convert-CMakeCacheToSnapshotText(
    [string]$Value,
    [object[]]$RootReplacements)
{
    $normalized = Convert-ToSnapshotText $Value $RootReplacements
    return Remove-HostSpecificCMakeCacheEntries $normalized
}

function Convert-CompileCommandToSnapshotText(
    [string]$Command,
    [object[]]$RootReplacements)
{
    $compilerMatch = [regex]::Match(
        $Command,
        '^\s*(?:"[^"]+"|\S+)(?=\s|$)')
    if (-not $compilerMatch.Success)
    {
        throw "Compilation command does not have a parseable first executable: $Command"
    }

    $compilerReplacements = @($RootReplacements | Where-Object {
            ([string]$_.token) -ceq '${MSVC_COMPILER}'
        })
    if ($compilerReplacements.Count -ne 1)
    {
        throw 'Compile snapshot normalization requires exactly one compiler replacement.'
    }
    $compilerReplacement = $compilerReplacements[0]
    $argumentReplacements = @($RootReplacements | Where-Object {
            -not [object]::ReferenceEquals($_, $compilerReplacement)
        })
    $arguments = $Command.Substring($compilerMatch.Length)
    $normalizedArguments = Convert-ToSnapshotText $arguments $argumentReplacements
    return '${MSVC_COMPILER}' + $normalizedArguments
}

function Assert-PrivacySafeSnapshotText(
    [string]$Value,
    [string]$Label)
{
    foreach ($pattern in @(
            '(?i)[A-Z]:/Users/',
            '(?i)AppData|/Temp/|\.worktrees|siwon',
            '(?i)"(?:generatedAt|timestamp|createdAt)"\s*:',
            '(?i)(?:^|[\r\n]|\\n)\s*(?:SITE|COMPUTERNAME|HOSTNAME|HOST)(?:(?::[^=\\\r\n]*)?=|:\s*)',
            '(?i)"(?:site|computerName|hostName|hostname|host)"\s*:',
            '(?i)(?:/D|-D)(?:SITE|COMPUTERNAME|HOSTNAME|HOST)='))
    {
        if ($Value -match $pattern)
        {
            throw "Privacy-sensitive generation data remains in ${Label}: $pattern"
        }
    }
}

function Write-DeterministicJson(
    [object]$Document,
    [string]$FilePath)
{
    $json = ($Document | ConvertTo-Json -Depth 30).Replace("`r`n", "`n") + "`n"
    Assert-PrivacySafeSnapshotText $json $FilePath
    [System.IO.File]::WriteAllText(
        $FilePath,
        $json,
        [System.Text.UTF8Encoding]::new($false))
}

function New-GenerationEvidenceSnapshots(
    [string]$RootPath,
    [string]$ClangFullVersion,
    [pscustomobject]$CompileProvenance,
    [string]$OutputDirectory)
{
    $evidenceDirectory = Join-Path $OutputDirectory 'evidence'
    New-Item -ItemType Directory -Path $evidenceDirectory -Force | Out-Null
    $cacheText = Get-Content -LiteralPath $CompileProvenance.CachePath -Raw
    $rootReplacements = @(
        [pscustomobject]@{ source = $CompileProvenance.VcpkgInstalled; token = '${VCPKG_INSTALLED_ROOT}' },
        [pscustomobject]@{ source = $CompileProvenance.BuildDirectory; token = '${BUILD_ROOT}' },
        [pscustomobject]@{ source = $RootPath; token = '${REPOSITORY_ROOT}' },
        [pscustomobject]@{ source = $CompileProvenance.Compiler; token = '${MSVC_COMPILER}' },
        [pscustomobject]@{ source = $CompileProvenance.MakeProgram; token = '${NINJA}' },
        [pscustomobject]@{ source = $CompileProvenance.CmakeCommand; token = '${CMAKE}' },
        [pscustomobject]@{ source = $CompileProvenance.Toolchain; token = '${VCPKG_TOOLCHAIN}' }
    )
    foreach ($powerShellMatch in [regex]::Matches(
            $cacheText,
            '(?m)^(?:DXA_POWERSHELL_EXECUTABLE|Z_VCPKG_PWSH_PATH|Z_VCPKG_POWERSHELL_PATH):[^=]+=(.+?)\r?$'))
    {
        $powerShellPath = $powerShellMatch.Groups[1].Value.Trim()
        if (-not [string]::IsNullOrWhiteSpace($powerShellPath))
        {
            $rootReplacements += [pscustomobject]@{
                source = $powerShellPath
                token = '${POWERSHELL}'
            }
        }
    }

    $parsedDatabase = Get-Content -LiteralPath $CompileProvenance.DatabasePath -Raw | ConvertFrom-Json
    [object[]]$entries = if ($parsedDatabase -is [System.Array]) { $parsedDatabase } else { @($parsedDatabase) }
    $snapshotEntriesByFile = @{}
    foreach ($entry in $entries)
    {
            $sourcePath = Get-FullPath ([string]$entry.file) ([string]$entry.directory)
            $relativeSource = Get-RepositoryRelativePath $sourcePath $RootPath
            $command = [string]$entry.command
            $normalizedCommand = Convert-CompileCommandToSnapshotText `
                -Command $command `
                -RootReplacements $rootReplacements
            $snapshotEntriesByFile[$relativeSource] = [pscustomobject][ordered]@{
                file = $relativeSource
                directory = '${BUILD_ROOT}'
                command = $normalizedCommand
            }
    }
    [string[]]$snapshotPaths = @($snapshotEntriesByFile.Keys)
    [System.Array]::Sort($snapshotPaths, [System.StringComparer]::Ordinal)
    $snapshotEntries = @($snapshotPaths | ForEach-Object { $snapshotEntriesByFile[$_] })
    $compileSnapshot = [pscustomobject][ordered]@{
        schemaVersion = 1
        normalization = [pscustomobject][ordered]@{
            encoding = 'UTF-8'
            lineEndings = 'LF'
            pathSeparator = '/'
            commandCompilerToken = '${MSVC_COMPILER}'
        }
        entries = $snapshotEntries
    }
    $compileSnapshotPath = Join-Path $evidenceDirectory 'compile-commands.json'
    Write-DeterministicJson $compileSnapshot $compileSnapshotPath

    $normalizedCache = Convert-CMakeCacheToSnapshotText $cacheText $rootReplacements
    Assert-PrivacySafeSnapshotText $normalizedCache 'cmake-cache.json'
    $cacheSnapshot = [pscustomobject][ordered]@{
        schemaVersion = 1
        normalization = [pscustomobject][ordered]@{
            encoding = 'UTF-8'
            lineEndings = 'LF'
            pathSeparator = '/'
            hostIdentifiers = 'omitted'
        }
        format = 'cmake-cache'
        content = $normalizedCache
    }
    $cacheSnapshotPath = Join-Path $evidenceDirectory 'cmake-cache.json'
    Write-DeterministicJson $cacheSnapshot $cacheSnapshotPath

    $normalizedClangVersion = $ClangFullVersion.Replace("`r`n", "`n").TrimEnd()
    $llvmIdentity = [regex]::Match(
        $normalizedClangVersion,
        '(?m)^Using LLVM/Clang libraries version:\s*(.+)$').Groups[1].Value.Trim()
    $toolSnapshot = [pscustomobject][ordered]@{
        schemaVersion = 1
        normalization = [pscustomobject][ordered]@{
            encoding = 'UTF-8'
            lineEndings = 'LF'
            pathSeparator = '/'
            rootTokens = [pscustomobject][ordered]@{
                '${REPOSITORY_ROOT}' = '.'
                '${BUILD_ROOT}' = 'out/build/portfolio-clang-uml'
                '${VCPKG_INSTALLED_ROOT}' = 'out/build/portfolio-clang-uml/vcpkg_installed'
                '${MSVC_COMPILER}' = Get-NormalizedManifestPath $CompileProvenance.Compiler $RootPath
                '${CMAKE}' = Get-NormalizedManifestPath $CompileProvenance.CmakeCommand $RootPath
                '${NINJA}' = Get-NormalizedManifestPath $CompileProvenance.MakeProgram $RootPath
                '${VCPKG_TOOLCHAIN}' = Get-NormalizedManifestPath $CompileProvenance.Toolchain $RootPath
                '${POWERSHELL}' = 'external/powershell'
            }
        }
        clangUml = [pscustomobject][ordered]@{
            version = $RequiredToolVersion
            fullVersion = $normalizedClangVersion
            llvmIdentity = $llvmIdentity
        }
        cmakeVersion = $CompileProvenance.CmakeVersion
        ninjaVersion = $CompileProvenance.NinjaVersion
        msvcVersion = $CompileProvenance.MsvcVersion
        compilerIdentity = $CompileProvenance.CompilerIdentity
        sourceDigests = [pscustomobject][ordered]@{
            compileCommandsRawSha256 = Get-Sha256Hex $CompileProvenance.DatabasePath
            cmakeCacheRawSha256 = Get-Sha256Hex $CompileProvenance.CachePath
            vcpkgStatusRawSha256 = Get-Sha256Hex (Join-Path $CompileProvenance.VcpkgInstalled 'vcpkg\status')
        }
    }
    $toolSnapshotPath = Join-Path $evidenceDirectory 'tool-identities.json'
    Write-DeterministicJson $toolSnapshot $toolSnapshotPath

    $statusSource = Join-Path $CompileProvenance.VcpkgInstalled 'vcpkg\status'
    $statusText = (Get-Content -LiteralPath $statusSource -Raw).Replace("`r`n", "`n")
    Assert-PrivacySafeSnapshotText $statusText 'vcpkg-status.json'
    $statusSnapshot = [pscustomobject][ordered]@{
        schemaVersion = 1
        normalization = [pscustomobject][ordered]@{
            encoding = 'UTF-8'
            lineEndings = 'LF'
        }
        format = 'vcpkg-status'
        content = $statusText
    }
    $statusSnapshotPath = Join-Path $evidenceDirectory 'vcpkg-status.json'
    Write-DeterministicJson $statusSnapshot $statusSnapshotPath

    $installed = Get-VcpkgInstalledProvenance $CompileProvenance.VcpkgInstalled
    $metadataEntries = @($installed.metadataFiles | ForEach-Object {
            $sourcePath = Join-Path $CompileProvenance.VcpkgInstalled $_.path
            $content = (Get-Content -LiteralPath $sourcePath -Raw).Replace("`r`n", "`n")
            Assert-PrivacySafeSnapshotText $content $_.path
            [pscustomobject][ordered]@{
                path = $_.path
                sha256 = Get-Utf8StringSha256 $content
                content = $content
            }
        })
    $metadataSnapshot = [pscustomobject][ordered]@{
        schemaVersion = 1
        normalization = [pscustomobject][ordered]@{
            encoding = 'UTF-8'
            lineEndings = 'LF'
        }
        files = $metadataEntries
    }
    $metadataSnapshotPath = Join-Path $evidenceDirectory 'vcpkg-metadata.json'
    Write-DeterministicJson $metadataSnapshot $metadataSnapshotPath

    return [pscustomobject][ordered]@{
        Directory = $evidenceDirectory
        Files = @(
            $cacheSnapshotPath,
            $compileSnapshotPath,
            $toolSnapshotPath,
            $metadataSnapshotPath,
            $statusSnapshotPath
        )
    }
}

function New-ClassGenerationManifest(
    [string]$RootPath,
    [string]$ConfigPath,
    [string]$GeneratorPath,
    [string]$ClangFullVersion,
    [pscustomobject]$CompileProvenance,
    [string]$OutputDirectory,
    [object[]]$DiagramResults,
    [pscustomobject]$SnapshotProvenance)
{
    $normalizedClangVersion = $ClangFullVersion.Replace("`r`n", "`n").TrimEnd()
    $llvmMatch = [regex]::Match(
        $normalizedClangVersion,
        '(?m)^Using LLVM/Clang libraries version:\s*(.+)$')
    if (-not $llvmMatch.Success)
    {
        throw 'clang-uml full version does not contain the LLVM identity'
    }
    $selectedSetMaterial = "$($CompileProvenance.SelectedPaths -join "`n")`n"
    $vcpkgManifestPath = Join-Path $RootPath 'vcpkg.json'
    if (-not (Test-Path -LiteralPath $vcpkgManifestPath -PathType Leaf))
    {
        throw 'vcpkg.json is missing from the generation source root'
    }
    $vcpkgConfigurationPath = Join-Path $RootPath 'vcpkg-configuration.json'
    $vcpkgConfiguration = if (Test-Path -LiteralPath $vcpkgConfigurationPath -PathType Leaf)
    {
        [pscustomobject][ordered]@{
            path = 'vcpkg-configuration.json'
            sha256 = Get-Sha256Hex $vcpkgConfigurationPath
        }
    }
    else
    {
        $null
    }
    $installed = Get-VcpkgInstalledProvenance $CompileProvenance.VcpkgInstalled

    $diagrams = [ordered]@{}
    foreach ($diagramName in $DiagramNames)
    {
        $result = @($DiagramResults | Where-Object { $_.Name -eq $diagramName })[0]
        if ($null -eq $result)
        {
            throw "Diagram result is missing while building manifest: $diagramName"
        }
        $jsonPath = Join-Path $OutputDirectory "$diagramName.json"
        $diagrams[$diagramName] = [pscustomobject][ordered]@{
            path = "docs/diagrams/class/$diagramName.json"
            sha256 = Get-Sha256Hex $jsonPath
            classCount = $result.ClassCount
            relationshipCount = $result.RelationshipCount
        }
    }

    $snapshotEntries = @($SnapshotProvenance.Files | ForEach-Object {
            [pscustomobject][ordered]@{
                path = "docs/diagrams/class/evidence/$([System.IO.Path]::GetFileName($_))"
                sha256 = Get-Sha256Hex $_
            }
        })

    return [pscustomobject][ordered]@{
        schemaVersion = 2
        basis = [pscustomobject][ordered]@{
            commitSha = $BasisCommitSha
            treeSha = $BasisTreeSha
        }
        tooling = [pscustomobject][ordered]@{
            clangUml = [pscustomobject][ordered]@{
                version = $RequiredToolVersion
                fullVersion = $normalizedClangVersion
                llvmIdentity = $llvmMatch.Groups[1].Value.Trim()
            }
            config = [pscustomobject][ordered]@{
                path = '.clang-uml'
                sha256 = Get-NormalizedTextSha256 $ConfigPath
            }
            generator = [pscustomobject][ordered]@{
                path = 'scripts/portfolio/generate-class-diagrams.ps1'
                sha256 = Get-NormalizedTextSha256 $GeneratorPath
            }
            cmakeVersion = $CompileProvenance.CmakeVersion
            ninjaVersion = $CompileProvenance.NinjaVersion
            msvcVersion = $CompileProvenance.MsvcVersion
        }
        compilation = [pscustomobject][ordered]@{
            compileCommands = [pscustomobject][ordered]@{
                path = 'out/build/portfolio-clang-uml/compile_commands.json'
                sha256 = Get-Sha256Hex $CompileProvenance.DatabasePath
                totalTranslationUnits = $CompileProvenance.TotalTranslationUnits
                selectedTranslationUnits = $CompileProvenance.SelectedPaths.Count
                selectedPathsSha256 = Get-Utf8StringSha256 $selectedSetMaterial
                selectedPaths = $CompileProvenance.SelectedPaths
            }
            cmakeCache = [pscustomobject][ordered]@{
                path = 'out/build/portfolio-clang-uml/CMakeCache.txt'
                sha256 = Get-Sha256Hex $CompileProvenance.CachePath
                generator = $CompileProvenance.Generator
                homeDirectory = Get-NormalizedManifestPath $CompileProvenance.SourceRoot $RootPath
                buildDirectory = Get-NormalizedManifestPath $CompileProvenance.BuildDirectory $RootPath
                compiler = Get-NormalizedManifestPath $CompileProvenance.Compiler $RootPath
                compilerIdentity = $CompileProvenance.CompilerIdentity
                makeProgram = Get-NormalizedManifestPath $CompileProvenance.MakeProgram $RootPath
                toolchain = Get-NormalizedManifestPath $CompileProvenance.Toolchain $RootPath
                vcpkgInstalled = Get-NormalizedManifestPath $CompileProvenance.VcpkgInstalled $RootPath
            }
        }
        dependencies = [pscustomobject][ordered]@{
            vcpkgManifest = [pscustomobject][ordered]@{
                path = 'vcpkg.json'
                sha256 = Get-Sha256Hex $vcpkgManifestPath
            }
            vcpkgConfiguration = $vcpkgConfiguration
            installed = $installed
        }
        snapshots = $snapshotEntries
        diagrams = [pscustomobject]$diagrams
    }
}

function Write-And-ValidateClassGenerationManifest(
    [pscustomobject]$Manifest,
    [string]$ManifestPath,
    [string]$RootPath,
    [string]$ConfigPath,
    [string]$GeneratorPath,
    [pscustomobject]$CompileProvenance,
    [string]$OutputDirectory,
    [pscustomobject]$SnapshotProvenance)
{
    $json = ($Manifest | ConvertTo-Json -Depth 20).Replace("`r`n", "`n") + "`n"
    [System.IO.File]::WriteAllText(
        $ManifestPath,
        $json,
        [System.Text.UTF8Encoding]::new($false))
    $document = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    if ($document.schemaVersion -ne 2 -or
        $document.basis.commitSha -ne $BasisCommitSha -or
        $document.basis.treeSha -ne $BasisTreeSha -or
        $document.tooling.config.sha256 -ne (Get-NormalizedTextSha256 $ConfigPath) -or
        $document.tooling.generator.sha256 -ne (Get-NormalizedTextSha256 $GeneratorPath) -or
        $document.compilation.compileCommands.sha256 -ne (Get-Sha256Hex $CompileProvenance.DatabasePath) -or
        $document.compilation.cmakeCache.sha256 -ne (Get-Sha256Hex $CompileProvenance.CachePath))
    {
        throw 'Generated class manifest failed its input provenance validation'
    }
    $expectedSnapshotNames = @(
        'cmake-cache.json',
        'compile-commands.json',
        'tool-identities.json',
        'vcpkg-metadata.json',
        'vcpkg-status.json'
    )
    if (@($document.snapshots).Count -ne $expectedSnapshotNames.Count)
    {
        throw 'Generated class manifest does not enumerate the exact evidence snapshot set'
    }
    foreach ($index in 0..($expectedSnapshotNames.Count - 1))
    {
        $snapshot = @($document.snapshots)[$index]
        $expectedPath = "docs/diagrams/class/evidence/$($expectedSnapshotNames[$index])"
        $sourcePath = Join-Path $SnapshotProvenance.Directory $expectedSnapshotNames[$index]
        if ($snapshot.path -ne $expectedPath -or
            $snapshot.sha256 -ne (Get-Sha256Hex $sourcePath))
        {
            throw "Generated class manifest snapshot mismatch: $expectedPath"
        }
        Assert-PrivacySafeSnapshotText (Get-Content -LiteralPath $sourcePath -Raw) $expectedPath
    }
    foreach ($diagramName in $DiagramNames)
    {
        $jsonPath = Join-Path $OutputDirectory "$diagramName.json"
        if ($document.diagrams.$diagramName.sha256 -ne (Get-Sha256Hex $jsonPath))
        {
            throw "Generated class manifest failed raw JSON validation: $diagramName"
        }
    }
}

function Invoke-ClassDiagramGeneration
{
if ([string]::IsNullOrWhiteSpace($RepositoryRoot))
{
    $RepositoryRoot = Join-Path $PSScriptRoot '..\..'
}
$script:ResolvedRepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path

$gitRoot = Invoke-Git @('rev-parse', '--show-toplevel') -Capture
$resolvedGitRoot = [System.IO.Path]::GetFullPath($gitRoot)
if (-not $resolvedGitRoot.Equals(
        $script:ResolvedRepositoryRoot,
        [System.StringComparison]::OrdinalIgnoreCase))
{
    throw "Repository root mismatch: expected $script:ResolvedRepositoryRoot, git reported $resolvedGitRoot"
}

$basisType = Invoke-Git @('cat-file', '-t', $BasisCommitSha) -Capture
if ($basisType -ne 'commit')
{
    throw "Canonical basis object is not a commit: $BasisCommitSha"
}
$basisTree = Invoke-Git @('show', '-s', '--format=%T', $BasisCommitSha) -Capture
if ($basisTree -ne $BasisTreeSha)
{
    throw "Canonical basis tree mismatch: $basisTree"
}
Invoke-Git @('merge-base', '--is-ancestor', $BasisCommitSha, 'HEAD')
Assert-RepositoryBasisInputs $script:ResolvedRepositoryRoot $BasisCommitSha

if (-not (Test-Path -LiteralPath $ClangUmlExecutable -PathType Leaf))
{
    throw "clang-uml executable is missing: $ClangUmlExecutable"
}
$versionOutput = @(& $ClangUmlExecutable --version 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0)
{
    throw "clang-uml --version failed:`n$versionOutput"
}
if ($versionOutput -notmatch "(?m)^clang-uml $([regex]::Escape($RequiredToolVersion))$")
{
    throw "clang-uml version must be ${RequiredToolVersion}:`n$versionOutput"
}
Write-Host "validated clang-uml $RequiredToolVersion"

if ([string]::IsNullOrWhiteSpace($CompileDatabaseDirectory))
{
    $CompileDatabaseDirectory = Join-Path $script:ResolvedRepositoryRoot 'out\build\portfolio-clang-uml'
}
$resolvedCompileDatabaseDirectory = [System.IO.Path]::GetFullPath($CompileDatabaseDirectory)
if (-not (Test-PathWithin $resolvedCompileDatabaseDirectory $script:ResolvedRepositoryRoot))
{
    throw "Compile database directory must stay inside the repository: $resolvedCompileDatabaseDirectory"
}
$compileProvenance = Assert-CompileDatabase $resolvedCompileDatabaseDirectory $script:ResolvedRepositoryRoot $BasisCommitSha

$configPath = Join-Path $script:ResolvedRepositoryRoot '.clang-uml'
if (-not (Test-Path -LiteralPath $configPath -PathType Leaf))
{
    throw "clang-uml config is missing: $configPath"
}

$temporaryBase = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\', '/')
$temporaryOutput = Join-Path $temporaryBase "dxa-clang-uml-$PID-$([guid]::NewGuid().ToString('N'))"
$temporaryOutput = [System.IO.Path]::GetFullPath($temporaryOutput)
if (-not (Test-PathWithin $temporaryOutput $temporaryBase) -or
    -not ([System.IO.Path]::GetFileName($temporaryOutput)).StartsWith('dxa-clang-uml-'))
{
    throw "Unsafe temporary output path: $temporaryOutput"
}

$results = @()
try
{
    New-Item -ItemType Directory -Path $temporaryOutput | Out-Null
    & $ClangUmlExecutable `
        -c $configPath `
        -d $resolvedCompileDatabaseDirectory `
        -n engine network `
        -g json `
        -o $temporaryOutput `
        --quiet
    if ($LASTEXITCODE -ne 0)
    {
        throw "clang-uml generation failed with exit code $LASTEXITCODE"
    }

    foreach ($diagramName in $DiagramNames)
    {
        $jsonPath = Join-Path $temporaryOutput "$diagramName.json"
        if (-not (Test-Path -LiteralPath $jsonPath -PathType Leaf))
        {
            throw "clang-uml did not generate $diagramName.json"
        }
        $results += Assert-RawClassDiagram $diagramName $jsonPath
    }

    $snapshotProvenance = New-GenerationEvidenceSnapshots `
        -RootPath $script:ResolvedRepositoryRoot `
        -ClangFullVersion $versionOutput `
        -CompileProvenance $compileProvenance `
        -OutputDirectory $temporaryOutput
    $manifestPath = Join-Path $temporaryOutput 'manifest.json'
    $manifest = New-ClassGenerationManifest `
        -RootPath $script:ResolvedRepositoryRoot `
        -ConfigPath $configPath `
        -GeneratorPath $PSCommandPath `
        -ClangFullVersion $versionOutput `
        -CompileProvenance $compileProvenance `
        -OutputDirectory $temporaryOutput `
        -DiagramResults $results `
        -SnapshotProvenance $snapshotProvenance
    Write-And-ValidateClassGenerationManifest `
        -Manifest $manifest `
        -ManifestPath $manifestPath `
        -RootPath $script:ResolvedRepositoryRoot `
        -ConfigPath $configPath `
        -GeneratorPath $PSCommandPath `
        -CompileProvenance $compileProvenance `
        -OutputDirectory $temporaryOutput `
        -SnapshotProvenance $snapshotProvenance

    $destinationDirectory = Join-Path $script:ResolvedRepositoryRoot 'docs\diagrams\class'
    New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
    foreach ($diagramName in $DiagramNames)
    {
        Move-Item `
            -LiteralPath (Join-Path $temporaryOutput "$diagramName.json") `
            -Destination (Join-Path $destinationDirectory "$diagramName.json") `
            -Force
    }
    $destinationEvidenceDirectory = Join-Path $destinationDirectory 'evidence'
    if (Test-Path -LiteralPath $destinationEvidenceDirectory)
    {
        $resolvedEvidence = [System.IO.Path]::GetFullPath($destinationEvidenceDirectory)
        $resolvedClassDirectory = [System.IO.Path]::GetFullPath($destinationDirectory).TrimEnd('\', '/')
        if (-not $resolvedEvidence.StartsWith(
                "$resolvedClassDirectory$([System.IO.Path]::DirectorySeparatorChar)",
                [System.StringComparison]::OrdinalIgnoreCase))
        {
            throw "Unsafe evidence publish target: $resolvedEvidence"
        }
        Remove-Item -LiteralPath $destinationEvidenceDirectory -Recurse -Force
    }
    Move-Item `
        -LiteralPath $snapshotProvenance.Directory `
        -Destination $destinationEvidenceDirectory
    Move-Item `
        -LiteralPath $manifestPath `
        -Destination (Join-Path $destinationDirectory 'manifest.json') `
        -Force
}
finally
{
    if ((Test-Path -LiteralPath $temporaryOutput) -and
        (Test-PathWithin $temporaryOutput $temporaryBase) -and
        ([System.IO.Path]::GetFileName($temporaryOutput)).StartsWith('dxa-clang-uml-'))
    {
        Remove-Item -LiteralPath $temporaryOutput -Recurse -Force
    }
}

foreach ($result in $results)
{
    Write-Host "generated $($result.Name): $($result.ClassCount) classes, $($result.RelationshipCount) relationships"
}
}

if ($MyInvocation.InvocationName -ne '.')
{
    Invoke-ClassDiagramGeneration
}
