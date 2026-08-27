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
    $makeProgram = Get-FullPath (Get-CMakeCacheValue $cacheText 'CMAKE_MAKE_PROGRAM') $resolvedDatabaseDirectory
    if ([System.IO.Path]::GetFileName($makeProgram) -ne 'ninja.exe' -or
        -not (Test-Path -LiteralPath $makeProgram -PathType Leaf))
    {
        throw "CMake make program must be an existing ninja.exe: $makeProgram"
    }
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

    Write-Host "validated compile database: $projectEntryCount translation units, $($usedSources.Count) selected"
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
Assert-CompileDatabase $resolvedCompileDatabaseDirectory $script:ResolvedRepositoryRoot $BasisCommitSha

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

    $destinationDirectory = Join-Path $script:ResolvedRepositoryRoot 'docs\diagrams\class'
    New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
    foreach ($diagramName in $DiagramNames)
    {
        Move-Item `
            -LiteralPath (Join-Path $temporaryOutput "$diagramName.json") `
            -Destination (Join-Path $destinationDirectory "$diagramName.json") `
            -Force
    }
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
