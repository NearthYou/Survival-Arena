import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { access, mkdtemp, mkdir, readFile, readdir, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import test from 'node:test';

const repositoryRoot = path.resolve(import.meta.dirname, '..');
const generatorScriptPath = path.join(repositoryRoot, 'scripts/portfolio/generate-class-diagrams.ps1');
const powershellExecutable = path.join(
    process.env.SystemRoot ?? 'C:\\Windows',
    'System32/WindowsPowerShell/v1.0/powershell.exe'
);
const basisCommitSha = '884e5e70d68d9fcf9dfe5638d97e06623da154c2';
const basisTreeSha = 'a3d167d7ddb3fadfe5ce9a2dfea6f5a58b170890';
const clangUmlVersion = '0.6.3';
const diagramNames = ['engine', 'network'];
const expectedNames = {
    engine: [
        (names) => names.has('EngineApp'),
        (names) => [...names].some((name) => name.includes('RuntimeScene')),
        (names) => [...names].some((name) => name.endsWith('Renderer')),
        (names) => [...names].some((name) => name.startsWith('Resource'))
    ],
    network: [
        'LobbyService',
        'WorkerRegistry',
        'GameServer',
        'AuthoritativeMatch',
        'GameSession',
        'SnapshotReplicator',
        'SnapshotReassembler'
    ].map((requiredName) => (names) => names.has(requiredName))
};

function flattenElements(elements) {
    const flattened = [];
    for (const element of elements ?? []) {
        if (Array.isArray(element?.elements)) {
            flattened.push(...flattenElements(element.elements));
        } else {
            flattened.push(element);
        }
    }
    return flattened;
}

function normalizeRepositoryPath(filePath) {
    return String(filePath).replaceAll('\\', '/').replace(/^\.\//u, '');
}

function escapeHtmlForExpectation(value) {
    return String(value)
        .replaceAll('&', '&amp;')
        .replaceAll('<', '&lt;')
        .replaceAll('>', '&gt;')
        .replaceAll('"', '&quot;')
        .replaceAll("'", '&#39;');
}

function normalizedTextSha256(content) {
    return createHash('sha256').update(String(content).replaceAll('\r\n', '\n')).digest('hex');
}

function sha256(content) {
    return createHash('sha256').update(content).digest('hex');
}

function compareOrdinal(left, right) {
    return left < right ? -1 : (left > right ? 1 : 0);
}

function selectedTranslationUnit(relativePath) {
    return /^engine\/src\/.+\.cpp$/u.test(relativePath)
        || /^apps\/(?:game_client|game_server|lobby_server)\/src\/.+\.cpp$/u.test(relativePath)
        || relativePath === 'tests/engine_resource_pool_test.cpp';
}

function relationshipMidpoint(html, source, destination) {
    const match = new RegExp(
        `data-source="${source}" data-destination="${destination}"[\\s\\S]*?<path d="M ([^ ]+) ([^ ]+) L ([^ ]+) ([^"]+)"`,
        'u'
    ).exec(html);
    assert.ok(match, `relationship path is missing: ${source} -> ${destination}`);
    return {
        x: (Number(match[1]) + Number(match[3])) / 2,
        y: (Number(match[2]) + Number(match[4])) / 2
    };
}

function extractGroupRectangles(markup, groupClass) {
    const pattern = new RegExp(
        `<g class="${groupClass}"[\\s\\S]*?<rect x="([^"]+)" y="([^"]+)" width="([^"]+)" height="([^"]+)"`,
        'gu'
    );
    return [...markup.matchAll(pattern)].map((match) => ({
        x: Number(match[1]),
        y: Number(match[2]),
        width: Number(match[3]),
        height: Number(match[4])
    }));
}

function rectanglesOverlap(left, right) {
    return left.x < right.x + right.width
        && left.x + left.width > right.x
        && left.y < right.y + right.height
        && left.y + left.height > right.y;
}

async function loadRawClassDiagram(diagramName) {
    const relativePath = `docs/diagrams/class/${diagramName}.json`;
    return JSON.parse(await readFile(path.join(repositoryRoot, relativePath), 'utf8'));
}

async function validateClassFixture(document) {
    const renderer = await import('../scripts/portfolio/render-diagrams.mjs');
    return renderer.validateClassDiagram(document, {
        root: repositoryRoot,
        verifyBasisCommit: false
    });
}

function quotePowerShell(value) {
    return `'${String(value).replaceAll("'", "''")}'`;
}

function runPowerShell(command) {
    return spawnSync(
        powershellExecutable,
        ['-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-Command', command],
        { cwd: repositoryRoot, encoding: 'utf8' }
    );
}

function normalizeCmakeCacheSnapshot(cacheText) {
    const encoded = Buffer.from(cacheText, 'utf8').toString('base64');
    const result = runPowerShell([
        "$ErrorActionPreference = 'Stop'",
        `. ${quotePowerShell(generatorScriptPath)}`,
        `$cacheText = [System.Text.Encoding]::UTF8.GetString([Convert]::FromBase64String(${quotePowerShell(encoded)}))`,
        '$normalized = Convert-CMakeCacheToSnapshotText -Value $cacheText -RootReplacements @()',
        '$bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($normalized)',
        '[Console]::Out.Write([Convert]::ToBase64String($bytes))'
    ].join('; '));
    assert.equal(result.status, 0, result.stderr || result.stdout);
    return Buffer.from(result.stdout.trim(), 'base64').toString('utf8');
}

function runFixtureGit(root, argumentsList) {
    const result = spawnSync('git', argumentsList, { cwd: root, encoding: 'utf8' });
    assert.equal(result.status, 0, result.stderr || result.stdout);
    return result.stdout.trim();
}

async function createBasisInputFixture() {
    const root = await mkdtemp(path.join(tmpdir(), 'dxa-class-basis-'));
    const files = [
        'CMakeLists.txt',
        'CMakePresets.json',
        'vcpkg.json',
        'cmake/CompilerWarnings.cmake',
        'engine/CMakeLists.txt',
        'engine/src/windows/EngineApp.cpp',
        'apps/game_common/CMakeLists.txt',
        'apps/game_client/CMakeLists.txt',
        'apps/game_client/src/GameSession.cpp',
        'apps/game_server/CMakeLists.txt',
        'apps/game_server/src/GameServer.cpp',
        'apps/lobby_server/CMakeLists.txt',
        'apps/lobby_server/src/LobbyService.cpp',
        'protocol/CMakeLists.txt',
        'protocol/src/Protocol.cpp',
        'simulation/CMakeLists.txt',
        'simulation/src/Simulation.cpp',
        'tests/CMakeLists.txt',
        'tests/engine_resource_pool_test.cpp'
    ];
    for (const relativePath of files) {
        const filePath = path.join(root, relativePath);
        await mkdir(path.dirname(filePath), { recursive: true });
        await writeFile(filePath, `fixture input: ${relativePath}\n`, 'utf8');
    }
    runFixtureGit(root, ['init']);
    runFixtureGit(root, ['config', 'user.email', 'fixture@example.com']);
    runFixtureGit(root, ['config', 'user.name', 'Fixture']);
    runFixtureGit(root, ['config', 'core.autocrlf', 'false']);
    runFixtureGit(root, ['add', '--', ...files]);
    runFixtureGit(root, ['commit', '-m', 'basis fixture']);
    return { root, basisSha: runFixtureGit(root, ['rev-parse', 'HEAD']) };
}

async function addCompileDatabaseFixture(fixture) {
    const buildDirectory = path.join(fixture.root, 'out/build/portfolio-clang-uml');
    const vcpkgDirectory = path.join(buildDirectory, 'vcpkg_installed');
    const compiler = 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe';
    const ninja = 'C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe';
    const cmake = 'C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe';
    const toolchain = 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake';
    const selectedSources = [
        'engine/src/windows/EngineApp.cpp',
        'apps/game_client/src/GameSession.cpp',
        'apps/game_server/src/GameServer.cpp',
        'apps/lobby_server/src/LobbyService.cpp',
        'tests/engine_resource_pool_test.cpp'
    ];
    await mkdir(path.join(vcpkgDirectory, 'x64-windows/include'), { recursive: true });
    await writeFile(path.join(buildDirectory, 'build.ninja'), '# generated by fixture\n', 'utf8');
    await writeFile(
        path.join(buildDirectory, 'CMakeCache.txt'),
        [
            `CMAKE_CXX_COMPILER:FILEPATH=${compiler}`,
            `CMAKE_MAKE_PROGRAM:FILEPATH=${ninja}`,
            `CMAKE_COMMAND:INTERNAL=${cmake}`,
            `CMAKE_TOOLCHAIN_FILE:FILEPATH=${toolchain}`,
            `VCPKG_INSTALLED_DIR:PATH=${vcpkgDirectory.replaceAll('\\', '/')}`,
            'CMAKE_GENERATOR:INTERNAL=Ninja',
            `CMAKE_HOME_DIRECTORY:INTERNAL=${fixture.root.replaceAll('\\', '/')}`,
            ''
        ].join('\n'),
        'utf8'
    );
    const entries = selectedSources.map((relativePath) => {
        const sourcePath = path.join(fixture.root, relativePath);
        return {
            directory: buildDirectory.replaceAll('\\', '/'),
            file: sourcePath.replaceAll('\\', '/'),
            command: `"${compiler}" /nologo /TP -external:I"${vcpkgDirectory.replaceAll('\\', '/')}/x64-windows/include" /c "${sourcePath.replaceAll('\\', '/')}"`
        };
    });
    await writeFile(
        path.join(buildDirectory, 'compile_commands.json'),
        `${JSON.stringify(entries, null, 2)}\n`,
        'utf8'
    );
    fixture.buildDirectory = buildDirectory;
    fixture.compileEntries = entries;
    return fixture;
}

function validateFixtureBasisInputs(fixture) {
    return runPowerShell([
        "$ErrorActionPreference = 'Stop'",
        `. ${quotePowerShell(generatorScriptPath)}`,
        `Assert-RepositoryBasisInputs -RootPath ${quotePowerShell(fixture.root)} -BasisSha ${quotePowerShell(fixture.basisSha)}`,
        "Write-Output 'BASIS_INPUTS_VALID'"
    ].join('; '));
}

function validateFixtureCompileDatabase(fixture) {
    return runPowerShell([
        "$ErrorActionPreference = 'Stop'",
        `. ${quotePowerShell(generatorScriptPath)}`,
        `Assert-CompileDatabase -DatabaseDirectory ${quotePowerShell(fixture.buildDirectory)} -RootPath ${quotePowerShell(fixture.root)} -BasisSha ${quotePowerShell(fixture.basisSha)}`,
        "Write-Output 'COMPILE_DATABASE_VALID'"
    ].join('; '));
}

test('generator can be imported for fail-closed provenance contract tests without running generation', () => {
    const missingRoot = path.join(repositoryRoot, 'out', 'missing-provenance-fixture');
    const result = runPowerShell([
        `. ${quotePowerShell(generatorScriptPath)} -RepositoryRoot ${quotePowerShell(missingRoot)}`,
        "if (-not (Get-Command Assert-RepositoryBasisInputs -ErrorAction SilentlyContinue)) { exit 23 }",
        "Write-Output 'PROVENANCE_FUNCTIONS_IMPORTED'"
    ].join('; '));

    assert.equal(result.status, 0, result.stderr || result.stdout);
    assert.match(result.stdout, /PROVENANCE_FUNCTIONS_IMPORTED/u);
});

test('generation snapshot omits machine host CMake cache entries before hashing', () => {
    const cacheText = [
        '//Name of the computer/site where compile is being run',
        'SITE:STRING=LEAKED_HOST_VALUE',
        '//ADVANCED property for variable: SITE',
        'SITE-ADVANCED:INTERNAL=1',
        'COMPUTERNAME:INTERNAL=LEAKED_HOST_VALUE',
        'HOSTNAME:INTERNAL=LEAKED_HOST_VALUE',
        'CMAKE_GENERATOR:INTERNAL=Ninja',
        ''
    ].join('\r\n');

    const normalized = normalizeCmakeCacheSnapshot(cacheText);

    assert.equal(normalized.includes('\r'), false);
    assert.match(normalized, /^CMAKE_GENERATOR:INTERNAL=Ninja$/mu);
    assert.doesNotMatch(
        normalized,
        /^(?:SITE|COMPUTERNAME|HOSTNAME|HOST)(?:-ADVANCED)?:/imu
    );
    assert.doesNotMatch(normalized, /LEAKED_HOST_VALUE/iu);
});

test('basis provenance rejects an untracked source inside a consumed scope', async () => {
    const fixture = await createBasisInputFixture();
    try {
        await writeFile(path.join(fixture.root, 'engine/src/untracked.cpp'), 'int untracked;\n', 'utf8');

        const result = validateFixtureBasisInputs(fixture);

        assert.notEqual(result.status, 0, 'untracked consumed source was accepted');
        assert.match(result.stderr, /untracked/iu);
    } finally {
        await rm(fixture.root, { recursive: true, force: true });
    }
});

for (const relativePath of [
    'tests/engine_resource_pool_test.cpp',
    'CMakeLists.txt',
    'engine/CMakeLists.txt'
]) {
    test(`basis provenance rejects changed consumed input ${relativePath}`, async () => {
        const fixture = await createBasisInputFixture();
        try {
            const filePath = path.join(fixture.root, relativePath);
            const original = await readFile(filePath, 'utf8');
            await writeFile(filePath, `${original}changed after basis\n`, 'utf8');

            const result = validateFixtureBasisInputs(fixture);

            assert.notEqual(result.status, 0, `${relativePath} change was accepted`);
            assert.match(result.stderr, /basis/iu);
        } finally {
            await rm(fixture.root, { recursive: true, force: true });
        }
    });
}

test('compile database provenance accepts a fresh owned Ninja and MSVC fixture', async () => {
    const fixture = await addCompileDatabaseFixture(await createBasisInputFixture());
    try {
        const result = validateFixtureCompileDatabase(fixture);

        assert.equal(result.status, 0, result.stderr || result.stdout);
        assert.match(result.stdout, /COMPILE_DATABASE_VALID/u);
    } finally {
        await rm(fixture.root, { recursive: true, force: true });
    }
});

test('compile database provenance rejects a tampered compiler command', async () => {
    const fixture = await addCompileDatabaseFixture(await createBasisInputFixture());
    try {
        fixture.compileEntries[0].command = fixture.compileEntries[0].command.replace('cl.exe', 'clang++.exe');
        await writeFile(
            path.join(fixture.buildDirectory, 'compile_commands.json'),
            `${JSON.stringify(fixture.compileEntries, null, 2)}\n`,
            'utf8'
        );
        const cachePath = path.join(fixture.buildDirectory, 'CMakeCache.txt');
        const cache = await readFile(cachePath, 'utf8');
        await writeFile(cachePath, cache, 'utf8');

        const result = validateFixtureCompileDatabase(fixture);

        assert.notEqual(result.status, 0, 'tampered compiler command was accepted');
        assert.match(result.stderr, /MSVC/iu);
    } finally {
        await rm(fixture.root, { recursive: true, force: true });
    }
});

test('compile database provenance rejects a different executable also named cl.exe', async () => {
    const fixture = await addCompileDatabaseFixture(await createBasisInputFixture());
    try {
        const fakeCompiler = path.join(fixture.buildDirectory, 'tools/cl.exe');
        await mkdir(path.dirname(fakeCompiler), { recursive: true });
        await writeFile(fakeCompiler, 'not the cache compiler\n', 'utf8');
        const commandCompiler = /^"([^"]+)"/u.exec(fixture.compileEntries[0].command)?.[1];
        fixture.compileEntries[0].command = fixture.compileEntries[0].command.replace(
            commandCompiler,
            fakeCompiler.replaceAll('\\', '/')
        );
        await writeFile(
            path.join(fixture.buildDirectory, 'compile_commands.json'),
            `${JSON.stringify(fixture.compileEntries, null, 2)}\n`,
            'utf8'
        );

        const result = validateFixtureCompileDatabase(fixture);

        assert.notEqual(result.status, 0, 'different cl.exe was accepted');
        assert.match(result.stderr, /cache compiler/iu);
    } finally {
        await rm(fixture.root, { recursive: true, force: true });
    }
});

test('compile database provenance rejects a non-Ninja CMake cache', async () => {
    const fixture = await addCompileDatabaseFixture(await createBasisInputFixture());
    try {
        const cachePath = path.join(fixture.buildDirectory, 'CMakeCache.txt');
        const cache = await readFile(cachePath, 'utf8');
        await writeFile(
            cachePath,
            cache.replace('CMAKE_GENERATOR:INTERNAL=Ninja', 'CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022'),
            'utf8'
        );

        const result = validateFixtureCompileDatabase(fixture);

        assert.notEqual(result.status, 0, 'non-Ninja cache was accepted');
        assert.match(result.stderr, /Ninja/iu);
    } finally {
        await rm(fixture.root, { recursive: true, force: true });
    }
});

test('compile database provenance rejects a mixed vcpkg dependency root', async () => {
    const fixture = await addCompileDatabaseFixture(await createBasisInputFixture());
    try {
        fixture.compileEntries[0].command = fixture.compileEntries[0].command.replace(
            fixture.buildDirectory.replaceAll('\\', '/'),
            'C:/foreign-checkout/out/build'
        );
        await writeFile(
            path.join(fixture.buildDirectory, 'compile_commands.json'),
            `${JSON.stringify(fixture.compileEntries, null, 2)}\n`,
            'utf8'
        );

        const result = validateFixtureCompileDatabase(fixture);

        assert.notEqual(result.status, 0, 'mixed dependency root was accepted');
        assert.match(result.stderr, /mixed dependency root/iu);
    } finally {
        await rm(fixture.root, { recursive: true, force: true });
    }
});

test('compile database provenance rejects an untracked selected translation unit', async () => {
    const fixture = await addCompileDatabaseFixture(await createBasisInputFixture());
    try {
        const injectedPath = path.join(fixture.root, 'engine/src/Injected.cpp');
        await writeFile(injectedPath, 'int injected;\n', 'utf8');
        fixture.compileEntries.push({
            ...fixture.compileEntries[0],
            file: injectedPath.replaceAll('\\', '/'),
            command: fixture.compileEntries[0].command.replace(
                fixture.compileEntries[0].file,
                injectedPath.replaceAll('\\', '/')
            )
        });
        await writeFile(
            path.join(fixture.buildDirectory, 'compile_commands.json'),
            `${JSON.stringify(fixture.compileEntries, null, 2)}\n`,
            'utf8'
        );
        const cachePath = path.join(fixture.buildDirectory, 'CMakeCache.txt');
        const cache = await readFile(cachePath, 'utf8');
        await writeFile(cachePath, cache, 'utf8');

        const result = validateFixtureCompileDatabase(fixture);

        assert.notEqual(result.status, 0, 'untracked selected translation unit was accepted');
        assert.match(result.stderr, /ls-files|tracked|basis/iu);
    } finally {
        await rm(fixture.root, { recursive: true, force: true });
    }
});

test('compile database provenance rejects a selected translation-unit set mismatch', async () => {
    const fixture = await addCompileDatabaseFixture(await createBasisInputFixture());
    try {
        fixture.compileEntries = fixture.compileEntries.slice(0, -1);
        await writeFile(
            path.join(fixture.buildDirectory, 'compile_commands.json'),
            `${JSON.stringify(fixture.compileEntries, null, 2)}\n`,
            'utf8'
        );

        const result = validateFixtureCompileDatabase(fixture);

        assert.notEqual(result.status, 0, 'selected translation-unit set mismatch was accepted');
        assert.match(result.stderr, /selected source set does not match basis/iu);
    } finally {
        await rm(fixture.root, { recursive: true, force: true });
    }
});

test('.clang-uml defines the engine and network AST scopes with metadata', async () => {
    const config = await readFile(path.join(repositoryRoot, '.clang-uml'), 'utf8');

    assert.match(config, /^diagrams:\s*$/mu);
    for (const diagramName of diagramNames) {
        assert.match(config, new RegExp(`^  ${diagramName}:\\s*$`, 'mu'));
    }
    assert.match(config, /^generate_metadata:\s*true\s*$/mu);
    assert.match(config, /^remove_compile_flags:\s*\r?\n\s+- ['"]?\/WX['"]?\s*$/mu);
    assert.doesNotMatch(config, /(?:^|[/\\])(?:tests?|third_party)(?:[/\\]|$)/imu);
});

test('committed generation manifest deterministically binds AST generation inputs and outputs', async () => {
    const manifestPath = path.join(repositoryRoot, 'docs/diagrams/class/manifest.json');
    const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
    const [config, generator, vcpkgManifest] = await Promise.all([
        readFile(path.join(repositoryRoot, '.clang-uml'), 'utf8'),
        readFile(generatorScriptPath, 'utf8'),
        readFile(path.join(repositoryRoot, 'vcpkg.json'))
    ]);

    assert.equal(manifest.schemaVersion, 2);
    assert.deepEqual(manifest.basis, {
        commitSha: basisCommitSha,
        treeSha: basisTreeSha
    });
    assert.equal(manifest.tooling.clangUml.version, clangUmlVersion);
    assert.match(manifest.tooling.clangUml.fullVersion, /^clang-uml 0\.6\.3\n/iu);
    assert.match(manifest.tooling.clangUml.llvmIdentity, /^clang version 22\.1\.8\b/u);
    assert.deepEqual(manifest.tooling.config, {
        path: '.clang-uml',
        sha256: normalizedTextSha256(config)
    });
    assert.deepEqual(manifest.tooling.generator, {
        path: 'scripts/portfolio/generate-class-diagrams.ps1',
        sha256: normalizedTextSha256(generator)
    });
    assert.match(manifest.tooling.cmakeVersion, /^cmake version 3\.31\.6/iu);
    assert.equal(manifest.tooling.ninjaVersion, '1.12.1');
    assert.match(manifest.tooling.msvcVersion, /^19\.44\.35228/u);

    const compile = manifest.compilation.compileCommands;
    assert.equal(compile.path, 'out/build/portfolio-clang-uml/compile_commands.json');
    assert.match(compile.sha256, /^[0-9a-f]{64}$/u);
    assert.equal(compile.totalTranslationUnits, 172);
    assert.equal(compile.selectedTranslationUnits, 44);
    assert.equal(compile.selectedPaths.length, 44);
    assert.deepEqual(compile.selectedPaths, [...compile.selectedPaths].sort(compareOrdinal));
    assert.equal(new Set(compile.selectedPaths).size, compile.selectedPaths.length);
    assert.equal(compile.selectedPathsSha256, sha256(`${compile.selectedPaths.join('\n')}\n`));

    const cache = manifest.compilation.cmakeCache;
    assert.equal(cache.path, 'out/build/portfolio-clang-uml/CMakeCache.txt');
    assert.match(cache.sha256, /^[0-9a-f]{64}$/u);
    assert.equal(cache.generator, 'Ninja');
    assert.equal(cache.homeDirectory, '.');
    assert.equal(cache.buildDirectory, 'out/build/portfolio-clang-uml');
    assert.match(cache.compiler, /\/cl\.exe$/iu);
    assert.match(cache.compilerIdentity.volumeSerialNumber, /^0x[0-9a-f]{8}$/u);
    assert.match(cache.compilerIdentity.fileId, /^0x[0-9a-f]{32}$/u);
    assert.match(cache.compilerIdentity.sha256, /^[0-9a-f]{64}$/u);
    assert.match(cache.makeProgram, /\/ninja\.exe$/iu);
    assert.match(cache.toolchain, /\/vcpkg\.cmake$/iu);
    assert.equal(cache.vcpkgInstalled, 'out/build/portfolio-clang-uml/vcpkg_installed');

    assert.deepEqual(manifest.dependencies.vcpkgManifest, {
        path: 'vcpkg.json',
        sha256: sha256(vcpkgManifest)
    });
    assert.equal(manifest.dependencies.vcpkgConfiguration, null);
    assert.equal(manifest.dependencies.installed.status.path, 'vcpkg/status');
    assert.match(manifest.dependencies.installed.status.sha256, /^[0-9a-f]{64}$/u);
    const metadataFiles = manifest.dependencies.installed.metadataFiles;
    assert.equal(manifest.dependencies.installed.metadataFileCount, metadataFiles.length);
    assert.ok(metadataFiles.length > 0);
    assert.deepEqual(metadataFiles, [...metadataFiles].sort((left, right) => compareOrdinal(left.path, right.path)));
    assert.ok(metadataFiles.every((entry) => (
        /^(?:vcpkg\/info\/.+\.list|x64-windows\/share\/.+\/vcpkg_abi_info\.txt)$/u.test(entry.path)
        && /^[0-9a-f]{64}$/u.test(entry.sha256)
    )));
    assert.equal(
        manifest.dependencies.installed.metadataSetSha256,
        sha256(metadataFiles.map((entry) => `${entry.path}\0${entry.sha256}\n`).join(''))
    );

    for (const diagramName of diagramNames) {
        const rawPath = path.join(repositoryRoot, `docs/diagrams/class/${diagramName}.json`);
        const rawBytes = await readFile(rawPath);
        const raw = JSON.parse(rawBytes.toString('utf8'));
        assert.deepEqual(manifest.diagrams[diagramName], {
            path: `docs/diagrams/class/${diagramName}.json`,
            sha256: sha256(rawBytes),
            classCount: raw.elements.length,
            relationshipCount: raw.relationships.length
        });
    }
});

test('committed generation evidence snapshot is complete, enumerated and privacy-safe', async () => {
    const evidenceDirectory = path.join(repositoryRoot, 'docs/diagrams/class/evidence');
    const expectedFiles = [
        'cmake-cache.json',
        'compile-commands.json',
        'tool-identities.json',
        'vcpkg-metadata.json',
        'vcpkg-status.json'
    ];
    const actualFiles = (await readdir(evidenceDirectory)).sort(compareOrdinal);
    assert.deepEqual(actualFiles, expectedFiles);

    const manifest = JSON.parse(await readFile(
        path.join(repositoryRoot, 'docs/diagrams/class/manifest.json'),
        'utf8'
    ));
    assert.deepEqual(
        manifest.snapshots.map((entry) => entry.path),
        expectedFiles.map((file) => `docs/diagrams/class/evidence/${file}`)
    );
    for (const snapshot of manifest.snapshots) {
        const bytes = await readFile(path.join(repositoryRoot, snapshot.path));
        assert.equal(snapshot.sha256, sha256(bytes), snapshot.path);
        const text = bytes.toString('utf8');
        assert.doesNotMatch(text, /C:[/\\]Users[/\\]/iu, snapshot.path);
        assert.doesNotMatch(text, /AppData|[/\\]Temp[/\\]|\.worktrees|siwon/iu, snapshot.path);
        assert.doesNotMatch(text, /"(?:generatedAt|timestamp|createdAt)"\s*:/iu, snapshot.path);
        assert.doesNotMatch(
            text,
            /(?:^|\\n|\n)\s*(?:SITE|COMPUTERNAME|HOSTNAME|HOST)(?:(?::[^=\\\r\n]*)?=|:\s*)|"(?:site|computerName|hostName|hostname|host)"\s*:|(?:\/D|-D)(?:SITE|COMPUTERNAME|HOSTNAME|HOST)=/iu,
            snapshot.path
        );
    }

    const [cacheSnapshot, compileSnapshot, toolSnapshot, metadataSnapshot, statusSnapshot] = await Promise.all([
        readFile(path.join(evidenceDirectory, 'cmake-cache.json'), 'utf8').then(JSON.parse),
        readFile(path.join(evidenceDirectory, 'compile-commands.json'), 'utf8').then(JSON.parse),
        readFile(path.join(evidenceDirectory, 'tool-identities.json'), 'utf8').then(JSON.parse),
        readFile(path.join(evidenceDirectory, 'vcpkg-metadata.json'), 'utf8').then(JSON.parse),
        readFile(path.join(evidenceDirectory, 'vcpkg-status.json'), 'utf8').then(JSON.parse)
    ]);
    assert.equal(cacheSnapshot.normalization.hostIdentifiers, 'omitted');
    assert.equal(
        cacheSnapshot.content.split('\n').some((line) => (
            /^(?:SITE|COMPUTERNAME|HOSTNAME|HOST)(?:-ADVANCED)?:/iu.test(line)
        )),
        false
    );
    assert.equal(compileSnapshot.entries.length, manifest.compilation.compileCommands.totalTranslationUnits);
    assert.deepEqual(
        compileSnapshot.entries
            .map((entry) => entry.file)
            .filter((entryPath) => selectedTranslationUnit(entryPath)),
        manifest.compilation.compileCommands.selectedPaths
    );
    assert.deepEqual(toolSnapshot.compilerIdentity, manifest.compilation.cmakeCache.compilerIdentity);
    assert.deepEqual(toolSnapshot.sourceDigests, {
        compileCommandsRawSha256: manifest.compilation.compileCommands.sha256,
        cmakeCacheRawSha256: manifest.compilation.cmakeCache.sha256,
        vcpkgStatusRawSha256: manifest.dependencies.installed.status.sha256
    });
    assert.equal(sha256(statusSnapshot.content), manifest.dependencies.installed.status.sha256);
    assert.deepEqual(
        metadataSnapshot.files.map(({ path: relativePath, sha256: digest }) => ({
            path: relativePath,
            sha256: digest
        })),
        manifest.dependencies.installed.metadataFiles
    );
});

test('class validator rejects hand-authored JSON that only imitates a clang-uml envelope', async () => {
    const document = {
        name: 'engine',
        diagram_type: 'class',
        title: `Engine AST class diagram | code basis ${basisCommitSha}`,
        metadata: {
            clang_uml_version: clangUmlVersion,
            llvm_version: 'not real clang metadata'
        },
        elements: [
            {
                id: '1',
                name: 'FakeEngineA',
                type: 'class',
                members: [],
                methods: [],
                source_location: { file: 'engine/include/dxa/engine/EngineApp.hpp' }
            },
            {
                id: '2',
                name: 'FakeEngineB',
                type: 'class',
                members: [],
                methods: [],
                source_location: { file: 'engine/include/dxa/engine/EngineApp.hpp' }
            }
        ],
        relationships: [{ source: '1', destination: '2', type: 'dependency' }]
    };

    const errors = await validateClassFixture(document);

    assert.ok(errors.some((error) => error.includes('metadata.schema_version')));
    assert.ok(errors.some((error) => error.includes('package_type')));
    assert.ok(errors.some((error) => error.includes('minimum 9')));
    assert.ok(errors.some((error) => error.includes('missing required class: EngineApp')));
});

test('class validator rejects a self relationship before geometry can produce NaN', async () => {
    const document = structuredClone(await loadRawClassDiagram('engine'));
    document.relationships[0].destination = document.relationships[0].source;

    const errors = await validateClassFixture(document);

    assert.ok(errors.some((error) => error.includes('self relationship')));
});

test('class validator rejects a class source outside the diagram-specific prefix', async () => {
    const document = structuredClone(await loadRawClassDiagram('engine'));
    document.elements[0].source_location.file = 'apps/game_server/include/dxa/game_server/GameServer.hpp';

    const errors = await validateClassFixture(document);

    assert.ok(errors.some((error) => error.includes('allowed source prefix')));
});

test('class validator rejects a missing selected translation unit', async () => {
    const document = structuredClone(await loadRawClassDiagram('engine'));
    document.elements[0].source_location.translation_unit = 'engine/src/windows/Missing.cpp';

    const errors = await validateClassFixture(document);

    assert.ok(errors.some((error) => error.includes('translation unit is missing from current checkout')));
});

test('class validator rejects a diagram missing a required class', async () => {
    const document = structuredClone(await loadRawClassDiagram('engine'));
    const removed = document.elements.find((element) => element.name === 'EngineApp');
    document.elements = document.elements.filter((element) => element.id !== removed.id);
    document.relationships = document.relationships.filter((relationship) => (
        relationship.source !== removed.id && relationship.destination !== removed.id
    ));

    const errors = await validateClassFixture(document);

    assert.ok(errors.some((error) => error.includes('minimum 9')));
    assert.ok(errors.some((error) => error.includes('missing required class: EngineApp')));
});

for (const diagramName of diagramNames) {
    test(`${diagramName} is preserved as raw clang-uml AST JSON with source-backed relationships`, async () => {
        const document = await loadRawClassDiagram(diagramName);
        const elements = flattenElements(document.elements);
        const classes = elements.filter((element) => element?.type === 'class');
        const classNames = new Set(classes.map((element) => element.name));

        assert.equal(document.name, diagramName);
        assert.equal(document.diagram_type, 'class');
        assert.equal(document.metadata?.clang_uml_version, clangUmlVersion);
        assert.equal(typeof document.metadata?.llvm_version, 'string');
        assert.equal(Number.isInteger(document.metadata?.schema_version), true);
        assert.match(document.title ?? '', new RegExp(basisCommitSha));

        assert.equal(Object.hasOwn(document, 'schemaVersion'), false);
        assert.equal(Object.hasOwn(document, 'basisCommitSha'), false);
        assert.equal(Object.hasOwn(document, 'nodes'), false);
        assert.equal(Object.hasOwn(document, 'edges'), false);
        assert.ok(classes.length > 0, `${diagramName} must contain at least one AST class`);
        assert.ok(document.relationships.length > 0, `${diagramName} must contain at least one AST relationship`);

        for (const hasRequiredName of expectedNames[diagramName]) {
            assert.equal(hasRequiredName(classNames), true, `${diagramName} required class is missing`);
        }

        for (const classElement of classes) {
            assert.equal(typeof classElement.id, 'string');
            assert.equal(Array.isArray(classElement.members), true);
            assert.equal(Array.isArray(classElement.methods), true);

            const sourcePath = normalizeRepositoryPath(classElement.source_location?.file);
            assert.notEqual(sourcePath, '');
            assert.doesNotMatch(sourcePath, /^(?:tests?|third_party)\//iu);
            await access(path.join(repositoryRoot, sourcePath));

            const basisResult = spawnSync('git', ['cat-file', '-e', `${basisCommitSha}:${sourcePath}`], {
                cwd: repositoryRoot,
                encoding: 'utf8'
            });
            assert.equal(
                basisResult.status,
                0,
                `${diagramName} source is absent from basis commit: ${sourcePath}`
            );
        }
    });
}

test('class diagrams move to verified only with the generation manifest and pinned outputs', async () => {
    const [releaseStatusText, config, generator] = await Promise.all([
        readFile(path.join(repositoryRoot, 'docs/portfolio/release-status.json'), 'utf8'),
        readFile(path.join(repositoryRoot, '.clang-uml'), 'utf8'),
        readFile(generatorScriptPath, 'utf8')
    ]);
    const releaseStatus = JSON.parse(releaseStatusText);
    const item = releaseStatus.items.find((candidate) => candidate.id === 'class-diagrams');
    const expectedEvidence = [
        'docs/diagrams/class/engine.json',
        'docs/diagrams/class/network.json',
        'docs/diagrams/class/engine.html',
        'docs/diagrams/class/network.html',
        'docs/diagrams/class/manifest.json'
    ];
    const manifestBytes = await readFile(path.join(repositoryRoot, expectedEvidence[4]));

    assert.equal(item?.status, 'verified');
    assert.deepEqual(item?.evidence, expectedEvidence);
    assert.equal(item?.proof?.tool, 'clang-uml');
    assert.equal(item?.proof?.toolVersion, clangUmlVersion);
    assert.equal(item?.proof?.basisCommitSha, basisCommitSha);
    assert.deepEqual(item?.proof?.manifest, {
        path: expectedEvidence[4],
        sha256: sha256(manifestBytes)
    });
    assert.deepEqual(item?.proof?.inputs, {
        config: {
            path: '.clang-uml',
            sha256: normalizedTextSha256(config)
        },
        generator: {
            path: 'scripts/portfolio/generate-class-diagrams.ps1',
            sha256: normalizedTextSha256(generator)
        }
    });
    assert.deepEqual(item?.proof?.outputs, {
        engine: {
            json: expectedEvidence[0],
            html: expectedEvidence[2]
        },
        network: {
            json: expectedEvidence[1],
            html: expectedEvidence[3]
        }
    });
});

test('Task 2 renderer validates raw AST and renders deterministic self-contained class HTML', async () => {
    const renderer = await import('../scripts/portfolio/render-diagrams.mjs');

    assert.equal(typeof renderer.validateClassDiagram, 'function');
    assert.equal(typeof renderer.renderClassDiagram, 'function');

    for (const diagramName of diagramNames) {
        const document = await loadRawClassDiagram(diagramName);
        const errors = await renderer.validateClassDiagram(document, { root: repositoryRoot });
        const firstRender = renderer.renderClassDiagram(document);
        const secondRender = renderer.renderClassDiagram(document);
        const svg = firstRender.match(/<svg[\s\S]*?<\/svg>/u)?.[0] ?? '';
        const elements = flattenElements(document.elements).filter((element) => element?.type === 'class');
        const nodeRectangles = extractGroupRectangles(firstRender, 'class-node');
        const relationshipRectangles = extractGroupRectangles(firstRender, 'class-relationship');

        assert.deepEqual(errors, []);
        assert.equal(firstRender, secondRender);
        assert.match(firstRender, /<!doctype html>/iu);
        assert.match(firstRender, /<svg\b/iu);
        assert.match(firstRender, new RegExp(basisCommitSha));
        assert.match(firstRender, /clang-uml 0\.6\.3/u);
        assert.doesNotMatch(firstRender, /https?:\/\//iu);
        assert.doesNotMatch(firstRender, /<script\b/iu);
        assert.doesNotMatch(firstRender, /<link\b[^>]*rel=["']stylesheet["']/iu);
        assert.doesNotMatch(firstRender, /NaN/u);
        if (diagramName === 'engine') {
            assert.match(svg, />ResourceHandle&lt;T&gt;</u);
            assert.match(svg, />ResourcePool&lt;T&gt;</u);
            assert.match(svg, />ResourcePool::Slot</u);
            assert.doesNotMatch(svg, /##/u);
        }
        for (const element of elements) {
            const displayName = escapeHtmlForExpectation(element.display_name);
            assert.match(firstRender, new RegExp(displayName.replace(/[.*+?^${}()|[\]\\]/gu, '\\$&')));
        }
        for (const relationship of document.relationships) {
            assert.match(
                firstRender,
                new RegExp(`data-source="${relationship.source}" data-destination="${relationship.destination}"`)
            );
            const reciprocal = document.relationships.find((candidate) => (
                candidate.source === relationship.destination
                && candidate.destination === relationship.source
            ));
            if (reciprocal && relationship.source.localeCompare(relationship.destination, 'en') < 0) {
                const forwardMidpoint = relationshipMidpoint(
                    firstRender,
                    relationship.source,
                    relationship.destination
                );
                const reverseMidpoint = relationshipMidpoint(
                    firstRender,
                    reciprocal.source,
                    reciprocal.destination
                );
                assert.notDeepEqual(
                    forwardMidpoint,
                    reverseMidpoint,
                    'reciprocal relationships must render on separate tracks'
                );
                assert.ok(
                    Math.hypot(
                        forwardMidpoint.x - reverseMidpoint.x,
                        forwardMidpoint.y - reverseMidpoint.y
                    ) >= 32,
                    'reciprocal relationship labels require a responsive separation margin'
                );
            }
        }
        for (const [relationshipIndex, relationshipRectangle] of relationshipRectangles.entries()) {
            for (const [nodeIndex, nodeRectangle] of nodeRectangles.entries()) {
                assert.equal(
                    rectanglesOverlap(relationshipRectangle, nodeRectangle),
                    false,
                    `${diagramName} relationship label ${relationshipIndex} overlaps class node ${nodeIndex}`
                );
            }
            for (let otherIndex = relationshipIndex + 1; otherIndex < relationshipRectangles.length; otherIndex += 1) {
                assert.equal(
                    rectanglesOverlap(relationshipRectangle, relationshipRectangles[otherIndex]),
                    false,
                    `${diagramName} relationship labels ${relationshipIndex} and ${otherIndex} overlap`
                );
            }
        }
    }
});

test('committed class HTML and combined index exactly match the renderer', async () => {
    const renderer = await import('../scripts/portfolio/render-diagrams.mjs');
    const manualSourceNames = [
        'game-start-sequence',
        'room-lifecycle',
        'snapshot-data-flow',
        'system-architecture'
    ];
    const manualEntries = [];

    for (const sourceName of manualSourceNames) {
        const document = JSON.parse(await readFile(
            path.join(repositoryRoot, `docs/diagrams/source/${sourceName}.json`),
            'utf8'
        ));
        manualEntries.push({
            title: document.title,
            file: `${sourceName}.html`,
            basisCommitSha: document.basisCommitSha
        });
    }

    for (const diagramName of diagramNames) {
        const document = await loadRawClassDiagram(diagramName);
        const committedHtml = await readFile(
            path.join(repositoryRoot, `docs/diagrams/class/${diagramName}.html`),
            'utf8'
        );
        assert.equal(committedHtml, renderer.renderClassDiagram(document));
    }

    const committedIndex = await readFile(path.join(repositoryRoot, 'docs/diagrams/index.html'), 'utf8');
    assert.equal(committedIndex, renderer.renderIndex(manualEntries));
    assert.match(committedIndex, /href="class\/engine\.html"/u);
    assert.match(committedIndex, /href="class\/network\.html"/u);
});

test('renderer check treats manifest JSON as provenance rather than a diagram source', () => {
    const result = spawnSync(
        process.execPath,
        ['scripts/portfolio/render-diagrams.mjs', '--root', '.', '--check'],
        { cwd: repositoryRoot, encoding: 'utf8' }
    );

    assert.equal(result.status, 0, result.stderr || result.stdout);
    assert.match(result.stdout, /validated class\/engine\.json/u);
    assert.match(result.stdout, /validated class\/network\.json/u);
    assert.doesNotMatch(result.stdout, /validated class\/manifest\.json/u);
});
