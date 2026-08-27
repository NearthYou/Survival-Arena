import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdtemp, mkdir, readFile, readdir, rm, symlink, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { validateEvidence } from '../scripts/portfolio/evidence.mjs';
import {
    loadReleaseStatus,
    validateMarkdownLinks,
    validateReleaseStatus
} from '../scripts/portfolio/verify-all.mjs';

const basisCommitSha = '884e5e70d68d9fcf9dfe5638d97e06623da154c2';
const repositoryRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const generatorScriptPath = path.join(repositoryRoot, 'scripts/portfolio/generate-class-diagrams.ps1');
const powershellExecutable = path.join(
    process.env.SystemRoot ?? 'C:\\Windows',
    'System32/WindowsPowerShell/v1.0/powershell.exe'
);
const classEvidenceSnapshotPaths = [
    'docs/diagrams/class/evidence/cmake-cache.json',
    'docs/diagrams/class/evidence/compile-commands.json',
    'docs/diagrams/class/evidence/tool-identities.json',
    'docs/diagrams/class/evidence/vcpkg-metadata.json',
    'docs/diagrams/class/evidence/vcpkg-status.json'
];
const requiredRuntimeLfsPaths = [
    'assets/runtime/characters/cyber-runner.dxam',
    'assets/runtime/environment/colormap.dds',
    'assets/runtime/environment/prototype-floor.dxam'
];
const requiredCaseHeadings = [
    '## 상황',
    '## 재현',
    '## 관찰',
    '## 가설과 비교한 대안',
    '## 선택',
    '## 구현',
    '## 검증',
    '## 남은 한계'
];

function createDocument() {
    return {
        schemaVersion: 1,
        basisCommitSha,
        cases: [{
            id: 'sample',
            devlog: 'docs/devlog/sample.md',
            adr: 'docs/adr/sample.md',
            evidence: ['docs/benchmarks/sample/RESULT.md'],
            metrics: [{
                name: 'sample_count',
                value: 600,
                unit: 'frames',
                sourceText: '600/600'
            }],
            limits: ['synthetic fixture'],
            caseDocument: 'docs/portfolio/cases/sample.md'
        }]
    };
}

function createReleaseDocument() {
    return {
        schemaVersion: 1,
        codeBasisCommitSha: basisCommitSha,
        items: [
            { id: 'historical-24-player-metrics', label: '24-player metrics', status: 'verified', evidence: ['evidence.md'] },
            { id: 'historical-30-minute-soak', label: '30-minute soak', status: 'verified', evidence: ['evidence.md'] },
            { id: 'licenses-assets-manifest', label: 'licenses and assets', status: 'verified', evidence: ['evidence.md'] },
            { id: 'lfs-object-availability', label: 'LFS object availability', status: 'partial', evidence: ['evidence.md'] },
            { id: 'current-head-builds', label: 'current HEAD builds', status: 'partial', evidence: ['evidence.md'] },
            { id: 'warp-rtx-visual-artifacts', label: 'WARP and RTX visuals', status: 'partial', evidence: ['evidence.md'] },
            { id: 'architecture-diagrams', label: 'architecture diagrams', status: 'verified', evidence: ['evidence.md'] },
            { id: 'class-diagrams', label: 'class diagrams', status: 'missing', evidence: [] },
            { id: 'portfolio-pdf', label: 'portfolio PDF', status: 'missing', evidence: [] },
            { id: 'demo-video', label: 'demo video', status: 'missing', evidence: [] },
            {
                id: 'aws-external-test',
                label: 'AWS external test',
                status: 'blocked',
                evidence: ['evidence.md'],
                resourceState: { created: false, cleanupVerified: false }
            },
            { id: 'repository-visibility', label: 'repository visibility', status: 'blocked', evidence: [] },
            { id: 'v0.1.0', label: 'v0.1.0', status: 'missing', evidence: [] }
        ]
    };
}

async function withReleaseFixture(callback) {
    const root = await mkdtemp(path.join(tmpdir(), 'dxa-portfolio-release-'));
    try {
        await writeFile(path.join(root, 'evidence.md'), '# evidence\n', 'utf8');
        await callback(createReleaseDocument(), root);
    } finally {
        await rm(root, { recursive: true, force: true });
    }
}

function runFixtureGit(root, argumentsList, options = {}) {
    const result = spawnSync('git', argumentsList, { cwd: root, encoding: 'utf8' });
    if (result.status !== 0 && !options.allowFailure) {
        throw new Error(`git ${argumentsList.join(' ')} failed: ${result.stderr || result.stdout}`);
    }
    return result;
}

function sha256(content) {
    return createHash('sha256').update(content).digest('hex');
}

function normalizedTextSha256(content) {
    return sha256(String(content).replaceAll('\r\n', '\n'));
}

function quotePowerShell(value) {
    return `'${String(value).replaceAll("'", "''")}'`;
}

const compileReplacementFixture = Object.freeze({
    repository: 'C:/fixture/repository',
    build: 'C:/fixture/repository/out/build/portfolio-clang-uml',
    vcpkg: 'C:/fixture/repository/out/build/portfolio-clang-uml/vcpkg_installed',
    compiler: 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe'
});

function runCompileCommandThroughGenerator(command, options = {}) {
    const compilerPath = options.compilerPath ?? compileReplacementFixture.compiler;
    const replacementDefinitions = [
        {
            source: compileReplacementFixture.vcpkg,
            token: '${VCPKG_INSTALLED_ROOT}'
        },
        {
            source: compileReplacementFixture.build,
            token: '${BUILD_ROOT}'
        },
        {
            source: compileReplacementFixture.repository,
            token: '${REPOSITORY_ROOT}'
        },
        { source: compilerPath, token: '${MSVC_COMPILER}' },
        ...(options.additionalReplacements ?? [])
    ];
    const encoded = Buffer.from(command, 'utf8').toString('base64');
    const encodedReplacements = Buffer.from(
        JSON.stringify(replacementDefinitions),
        'utf8'
    ).toString('base64');
    const script = [
        "$ErrorActionPreference = 'Stop'",
        `. ${quotePowerShell(generatorScriptPath)}`,
        `$command = [System.Text.Encoding]::UTF8.GetString([Convert]::FromBase64String(${quotePowerShell(encoded)}))`,
        `$replacementJson = [System.Text.Encoding]::UTF8.GetString([Convert]::FromBase64String(${quotePowerShell(encodedReplacements)}))`,
        '$replacementDefinitions = $replacementJson | ConvertFrom-Json',
        '$rootReplacements = @($replacementDefinitions | ForEach-Object { [pscustomobject]@{ source = [string]$_.source; token = [string]$_.token } })',
        '$normalized = Convert-CompileCommandToSnapshotText -Command $command -RootReplacements $rootReplacements',
        '$bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($normalized)',
        '[Console]::Out.Write([Convert]::ToBase64String($bytes))'
    ].join('; ');
    return spawnSync(
        powershellExecutable,
        ['-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-Command', script],
        { cwd: repositoryRoot, encoding: 'utf8' }
    );
}

function canonicalizeCompileCommandThroughGenerator(command, options = {}) {
    const result = runCompileCommandThroughGenerator(command, options);
    assert.equal(result.status, 0, result.stderr || result.stdout);
    return Buffer.from(result.stdout.trim(), 'base64').toString('utf8');
}

function setVerifiedClassProof(document, inputs, manifestProof) {
    const paths = [
        'docs/diagrams/class/engine.json',
        'docs/diagrams/class/network.json',
        'docs/diagrams/class/engine.html',
        'docs/diagrams/class/network.html',
        ...(manifestProof ? ['docs/diagrams/class/manifest.json'] : [])
    ];
    const item = document.items.find((candidate) => candidate.id === 'class-diagrams');
    item.status = 'verified';
    item.evidence = paths;
    item.proof = {
        tool: 'clang-uml',
        toolVersion: '0.6.3',
        basisCommitSha,
        inputs,
        ...(manifestProof ? { manifest: manifestProof } : {}),
        outputs: {
            engine: { json: paths[0], html: paths[2] },
            network: { json: paths[1], html: paths[3] }
        }
    };
    return { item, paths };
}

async function createValidClassProofFixture(document, root) {
    const configPath = '.clang-uml';
    const generatorPath = 'scripts/portfolio/generate-class-diagrams.ps1';
    const [config, generator, vcpkgManifest] = await Promise.all([
        readFile(path.join(repositoryRoot, configPath), 'utf8'),
        readFile(path.join(repositoryRoot, generatorPath), 'utf8'),
        readFile(path.join(repositoryRoot, 'vcpkg.json'))
    ]);
    for (const [relativePath, content] of [[configPath, config], [generatorPath, generator]]) {
        const target = path.join(root, relativePath);
        await mkdir(path.dirname(target), { recursive: true });
        await writeFile(target, content, 'utf8');
    }
    await writeFile(path.join(root, 'vcpkg.json'), vcpkgManifest);

    for (const diagramName of ['engine', 'network']) {
        const jsonPath = `docs/diagrams/class/${diagramName}.json`;
        const htmlPath = `docs/diagrams/class/${diagramName}.html`;
        const [jsonText, html] = await Promise.all([
            readFile(path.join(repositoryRoot, jsonPath), 'utf8'),
            readFile(path.join(repositoryRoot, htmlPath), 'utf8')
        ]);
        const raw = JSON.parse(jsonText);
        for (const element of raw.elements) {
            for (const relativeSource of [
                element.source_location.file,
                element.source_location.translation_unit.replaceAll('\\', '/')
            ]) {
                const sourcePath = path.join(root, relativeSource);
                await mkdir(path.dirname(sourcePath), { recursive: true });
                await writeFile(sourcePath, `fixture source: ${relativeSource}\n`, 'utf8');
            }
        }
        for (const [relativePath, content] of [[jsonPath, jsonText], [htmlPath, html]]) {
            const target = path.join(root, relativePath);
            await mkdir(path.dirname(target), { recursive: true });
            await writeFile(target, content, 'utf8');
        }
    }

    const manifestPath = 'docs/diagrams/class/manifest.json';
    const manifestBytes = await readFile(path.join(repositoryRoot, manifestPath));
    const targetManifest = path.join(root, manifestPath);
    await mkdir(path.dirname(targetManifest), { recursive: true });
    await writeFile(targetManifest, manifestBytes);
    for (const relativePath of classEvidenceSnapshotPaths) {
        const target = path.join(root, relativePath);
        await mkdir(path.dirname(target), { recursive: true });
        await writeFile(target, await readFile(path.join(repositoryRoot, relativePath)));
    }

    return setVerifiedClassProof(document, {
        config: { path: configPath, sha256: normalizedTextSha256(config) },
        generator: { path: generatorPath, sha256: normalizedTextSha256(generator) }
    }, {
        path: manifestPath,
        sha256: sha256(manifestBytes)
    });
}

async function writeClassManifestFixture(root, item, manifest) {
    const content = `${JSON.stringify(manifest, null, 2)}\n`;
    await writeFile(path.join(root, 'docs/diagrams/class/manifest.json'), content, 'utf8');
    item.proof.manifest.sha256 = sha256(content);
}

async function updateClassSnapshotFixture(root, item, snapshotPath, transform, updateManifest) {
    const manifestPath = path.join(root, 'docs/diagrams/class/manifest.json');
    const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
    const absoluteSnapshotPath = path.join(root, snapshotPath);
    const original = await readFile(absoluteSnapshotPath, 'utf8');
    const updated = await transform(original, manifest);
    await writeFile(absoluteSnapshotPath, updated, 'utf8');
    const snapshot = manifest.snapshots.find((entry) => entry.path === snapshotPath);
    assert.ok(snapshot, `fixture manifest snapshot is missing: ${snapshotPath}`);
    snapshot.sha256 = sha256(updated);
    if (updateManifest) {
        await updateManifest(manifest, updated);
    }
    await writeClassManifestFixture(root, item, manifest);
}

function synchronizeInstalledMetadataManifest(manifest, metadata) {
    const entries = metadata.files.map(({ path: relativePath, sha256: digest }) => ({
        path: relativePath,
        sha256: digest
    }));
    manifest.dependencies.installed.metadataFiles = entries;
    manifest.dependencies.installed.metadataFileCount = entries.length;
    manifest.dependencies.installed.metadataSetSha256 = sha256(
        entries.map((entry) => `${entry.path}\0${entry.sha256}\n`).join('')
    );
}

async function listFixtureFiles(root, directory = root) {
    let entries;
    try {
        entries = await readdir(directory, { withFileTypes: true });
    } catch (error) {
        if (error.code === 'ENOENT') {
            return [];
        }
        throw error;
    }
    const files = [];
    for (const entry of entries) {
        const entryPath = path.join(directory, entry.name);
        if (entry.isDirectory()) {
            files.push(...await listFixtureFiles(root, entryPath));
        } else if (entry.isFile()) {
            files.push(path.relative(root, entryPath).split(path.sep).join('/'));
        }
    }
    return files.sort((left, right) => left.localeCompare(right, 'en'));
}

async function snapshotFixtureRepository(root, objectRoot) {
    return {
        status: runFixtureGit(root, ['status', '--porcelain=v1', '--untracked-files=all']).stdout,
        objectPaths: await listFixtureFiles(objectRoot)
    };
}

async function createLfsRepositoryFixture(options = {}) {
    const root = await mkdtemp(path.join(tmpdir(), 'dxa-portfolio-lfs-repository-'));
    runFixtureGit(root, ['init']);
    runFixtureGit(root, ['config', 'user.email', 'fixture@example.com']);
    runFixtureGit(root, ['config', 'user.name', 'Fixture']);
    runFixtureGit(root, ['config', 'core.autocrlf', 'false']);
    runFixtureGit(root, ['config', 'filter.lfs.clean', 'cat']);
    runFixtureGit(root, ['config', 'filter.lfs.smudge', 'cat']);
    runFixtureGit(root, ['config', 'filter.lfs.required', 'false']);
    runFixtureGit(root, ['config', 'filter.lfs.process', '']);

    const assetRecords = requiredRuntimeLfsPaths.map((relativePath, index) => {
        const content = Buffer.from(`runtime asset ${index}: ${relativePath}\n`, 'utf8');
        return {
            relativePath,
            content,
            oid: sha256(content),
            size: content.length
        };
    });
    const lfsAttributes = 'assets/runtime/** filter=lfs diff=lfs merge=lfs -text\n';
    await writeFile(
        path.join(root, '.gitattributes'),
        options.headHasLfsRule === false ? '*.txt text\n' : lfsAttributes,
        'utf8'
    );
    await writeFile(path.join(root, 'evidence.md'), '# evidence\n', 'utf8');
    await writeFile(path.join(root, 'LICENSE'), 'fixture license\n', 'utf8');
    for (const asset of assetRecords) {
        const worktreePath = path.join(root, asset.relativePath);
        await mkdir(path.dirname(worktreePath), { recursive: true });
        await writeFile(
            worktreePath,
            `version https://git-lfs.github.com/spec/v1\noid sha256:${asset.oid}\nsize ${asset.size}\n`,
            'utf8'
        );
    }

    for (const relativePath of ['.gitattributes', 'evidence.md', 'LICENSE', ...requiredRuntimeLfsPaths]) {
        const blob = runFixtureGit(root, ['hash-object', '-w', '--no-filters', relativePath]).stdout.trim();
        runFixtureGit(root, ['update-index', '--add', '--cacheinfo', `100644,${blob},${relativePath}`]);
    }
    runFixtureGit(root, ['commit', '-m', 'fixture']);
    if (options.headHasLfsRule === false) {
        await writeFile(path.join(root, '.gitattributes'), lfsAttributes, 'utf8');
    }

    const commonDirectoryOutput = runFixtureGit(root, ['rev-parse', '--git-common-dir']).stdout.trim();
    const commonDirectory = path.resolve(root, commonDirectoryOutput);
    const objectRoot = path.join(commonDirectory, 'lfs', 'objects');
    for (const asset of assetRecords) {
        const objectPath = path.join(objectRoot, asset.oid.slice(0, 2), asset.oid.slice(2, 4), asset.oid);
        await mkdir(path.dirname(objectPath), { recursive: true });
        await writeFile(objectPath, asset.content);
        await writeFile(path.join(root, asset.relativePath), asset.content);
        asset.objectPath = objectPath;
    }

    const document = createReleaseDocument();
    const lfs = document.items.find((item) => item.id === 'lfs-object-availability');
    lfs.status = 'verified';
    lfs.evidence = [...requiredRuntimeLfsPaths];
    lfs.proof = {
        checkedAt: '2026-08-27T12:00:00+09:00',
        objectsVerified: true,
        objectsHydrated: true,
        paths: [...requiredRuntimeLfsPaths]
    };

    return { root, objectRoot, assetRecords, document };
}

async function commitFixtureBlob(fixture, relativePath, content, message) {
    const worktreePath = path.join(fixture.root, relativePath);
    await writeFile(worktreePath, content);
    const blob = runFixtureGit(
        fixture.root,
        ['hash-object', '-w', '--no-filters', relativePath]
    ).stdout.trim();
    runFixtureGit(
        fixture.root,
        ['update-index', '--add', '--cacheinfo', `100644,${blob},${relativePath}`]
    );
    runFixtureGit(fixture.root, ['commit', '-m', message]);
    const asset = fixture.assetRecords.find((candidate) => candidate.relativePath === relativePath);
    if (asset) {
        await writeFile(worktreePath, asset.content);
    }
}

async function withFixture(callback) {
    const root = await mkdtemp(path.join(tmpdir(), 'dxa-portfolio-evidence-'));
    const document = createDocument();

    try {
        for (const sourcePath of [
            document.cases[0].devlog,
            document.cases[0].adr,
            ...document.cases[0].evidence
        ]) {
            const filePath = path.join(root, sourcePath);
            await mkdir(path.dirname(filePath), { recursive: true });
            await writeFile(filePath, 'sample result: 600/600\n', 'utf8');
        }
        const caseDocumentPath = path.join(root, document.cases[0].caseDocument);
        await mkdir(path.dirname(caseDocumentPath), { recursive: true });
        await writeFile(caseDocumentPath, [
            '# Sample case',
            '',
            '## 상황',
            '',
            '## 재현',
            '',
            '## 관찰',
            '',
            '## 가설과 비교한 대안',
            '',
            '## 선택',
            '',
            '## 구현',
            '',
            '## 검증',
            '',
            '[devlog](../../devlog/sample.md)',
            '[ADR](../../adr/sample.md)',
            '[evidence](../../benchmarks/sample/RESULT.md)',
            '',
            '## 남은 한계',
            ''
        ].join('\n'), 'utf8');

        await callback(document, root);
    } finally {
        await rm(root, { recursive: true, force: true });
    }
}

test('rejects duplicate case IDs', async () => {
    await withFixture(async (document, root) => {
        document.cases.push({ ...document.cases[0] });

        const errors = await validateEvidence(document, { root, verifyBasisCommit: false });

        assert.ok(errors.some((error) => error.includes('duplicate case id')));
    });
});

test('rejects a basis SHA that is not 40 lowercase hexadecimal characters', async () => {
    await withFixture(async (document, root) => {
        document.basisCommitSha = 'NOT-A-SHA';

        const errors = await validateEvidence(document, { root, verifyBasisCommit: false });

        assert.ok(errors.some((error) => error.includes('basisCommitSha')));
    });
});

test('rejects a well-formed SHA that is not the canonical portfolio basis', async () => {
    await withFixture(async (document, root) => {
        document.basisCommitSha = '5599de19687c3ed446f7242c72711bc9b34b3364';

        const errors = await validateEvidence(document, { root, verifyBasisCommit: false });

        assert.ok(errors.some((error) => (
            error.includes('basisCommitSha') && error.includes('canonical')
        )), errors.join('\n'));
    });
});

test('rejects source paths that escape the root', async () => {
    await withFixture(async (document, root) => {
        document.cases[0].devlog = '../outside.md';

        const errors = await validateEvidence(document, { root, verifyBasisCommit: false });

        assert.ok(errors.some((error) => error.includes('escapes root')));
    });
});

test('rejects missing source files', async () => {
    await withFixture(async (document, root) => {
        document.cases[0].evidence = ['docs/benchmarks/sample/missing.md'];

        const errors = await validateEvidence(document, { root, verifyBasisCommit: false });

        assert.ok(errors.some((error) => error.includes('source file is missing')));
    });
});

test('rejects a missing case document', async () => {
    await withFixture(async (document, root) => {
        document.cases[0].caseDocument = 'docs/portfolio/cases/missing.md';

        const errors = await validateEvidence(document, { root, verifyBasisCommit: false });

        assert.ok(errors.some((error) => (
            error.includes('case sample caseDocument') && error.includes('missing')
        )), errors.join('\n'));
    });
});

test('rejects case documents that escape by junction or resolve to a directory', async () => {
    const outsideRoot = await mkdtemp(path.join(tmpdir(), 'dxa-portfolio-case-outside-'));
    try {
        await withFixture(async (document, root) => {
            await writeFile(path.join(outsideRoot, 'case.md'), '# outside\n', 'utf8');
            await symlink(outsideRoot, path.join(root, 'case-escape'), 'junction');
            document.cases[0].caseDocument = 'case-escape/case.md';

            let errors = await validateEvidence(document, { root, verifyBasisCommit: false });
            assert.ok(errors.some((error) => (
                error.includes('case sample caseDocument') && error.includes('outside')
            )), errors.join('\n'));

            document.cases[0].caseDocument = 'docs/portfolio/cases';
            errors = await validateEvidence(document, { root, verifyBasisCommit: false });
            assert.ok(errors.some((error) => (
                error.includes('case sample caseDocument') && error.includes('regular file')
            )), errors.join('\n'));
        });
    } finally {
        await rm(outsideRoot, { recursive: true, force: true });
    }
});

test('rejects a case document with a required heading mismatch', async () => {
    await withFixture(async (document, root) => {
        const caseDocumentPath = path.join(root, document.cases[0].caseDocument);
        const content = await readFile(caseDocumentPath, 'utf8');
        await writeFile(caseDocumentPath, content.replace('## 검증', '## 검증 결과'), 'utf8');

        const errors = await validateEvidence(document, { root, verifyBasisCommit: false });

        assert.ok(errors.some((error) => (
            error.includes('case sample caseDocument') && error.includes('## 검증')
        )), errors.join('\n'));
    });
});

test('rejects a case document missing a required source link', async () => {
    await withFixture(async (document, root) => {
        const caseDocumentPath = path.join(root, document.cases[0].caseDocument);
        const content = await readFile(caseDocumentPath, 'utf8');
        await writeFile(
            caseDocumentPath,
            content.replace('[evidence](../../benchmarks/sample/RESULT.md)\n', ''),
            'utf8'
        );

        const errors = await validateEvidence(document, { root, verifyBasisCommit: false });

        assert.ok(errors.some((error) => (
            error.includes('case sample caseDocument')
            && error.includes('docs/benchmarks/sample/RESULT.md')
        )), errors.join('\n'));
    });
});

test('rejects metrics whose source text is absent from the evidence', async () => {
    await withFixture(async (document, root) => {
        document.cases[0].metrics[0].sourceText = 'not in the source';

        const errors = await validateEvidence(document, { root, verifyBasisCommit: false });

        assert.ok(errors.some((error) => error.includes('sourceText')));
    });
});

test('accepts an evidence document when all sources and metric text exist', async () => {
    await withFixture(async (document, root) => {
        const errors = await validateEvidence(document, { root, verifyBasisCommit: false });

        assert.deepEqual(errors, []);
    });
});

test('all evidence cases have structured case documents linked to their sources', async () => {
    const evidencePath = path.join(repositoryRoot, 'docs/portfolio/evidence.json');
    const document = JSON.parse(await readFile(evidencePath, 'utf8'));
    const missingDocuments = [];
    const documentErrors = [];

    for (const evidenceCase of document.cases) {
        const caseDocumentPath = path.join(repositoryRoot, evidenceCase.caseDocument);
        let caseText;

        try {
            caseText = await readFile(caseDocumentPath, 'utf8');
        } catch (error) {
            if (error.code === 'ENOENT') {
                missingDocuments.push(evidenceCase.caseDocument);
                continue;
            }
            throw error;
        }

        for (const heading of requiredCaseHeadings) {
            const count = caseText.split(/\r?\n/u).filter((line) => line === heading).length;
            if (count !== 1) {
                documentErrors.push(`${evidenceCase.id}: ${heading} count is ${count}`);
            }
        }

        const linkedPaths = [...caseText.matchAll(/\]\(([^)\s]+)\)/gu)]
            .map((match) => path.resolve(path.dirname(caseDocumentPath), match[1]));
        const requiredSources = [evidenceCase.devlog, evidenceCase.adr, ...evidenceCase.evidence];

        for (const sourcePath of requiredSources) {
            const expectedPath = path.resolve(repositoryRoot, sourcePath);
            if (!linkedPaths.includes(expectedPath)) {
                documentErrors.push(`${evidenceCase.id}: missing Markdown link to ${sourcePath}`);
            }
        }
    }

    assert.deepEqual(missingDocuments, [], `missing case documents:\n${missingDocuments.join('\n')}`);
    assert.deepEqual(documentErrors, []);
});

test('release status rejects values outside the public status vocabulary', async () => {
    await withReleaseFixture(async (document, root) => {
        document.items[0].status = 'complete';

        const errors = await validateReleaseStatus(document, { root });

        assert.ok(errors.some((error) => error.includes('historical-24-player-metrics.status')));
    });
});

test('verified release items require at least one evidence path', async () => {
    await withReleaseFixture(async (document, root) => {
        document.items[0].evidence = [];

        const errors = await validateReleaseStatus(document, { root });

        assert.ok(errors.some((error) => error.includes('historical-24-player-metrics.evidence')));
    });
});

test('release status requires the exact canonical code basis SHA', async () => {
    await withReleaseFixture(async (document, root) => {
        document.codeBasisCommitSha = '5599de19687c3ed446f7242c72711bc9b34b3364';

        const errors = await validateReleaseStatus(document, { root });

        assert.ok(errors.some((error) => error.includes('release-status.json.codeBasisCommitSha')));
    });
});

test('current HEAD and visual gates reject state-only verified mutations', async () => {
    await withReleaseFixture(async (document, root) => {
        for (const id of ['current-head-builds', 'warp-rtx-visual-artifacts']) {
            document.items.find((item) => item.id === id).status = 'verified';
        }

        const errors = await validateReleaseStatus(document, { root });

        for (const id of ['current-head-builds', 'warp-rtx-visual-artifacts']) {
            assert.ok(errors.some((error) => (
                error.includes(`${id}.status`) && error.includes('proof')
            )), `${id}:\n${errors.join('\n')}`);
        }
    });
});

test('current HEAD proof requires Windows, Linux server, and hosted CI on one HEAD', async () => {
    const root = await mkdtemp(path.join(tmpdir(), 'dxa-current-head-proof-'));
    try {
        await writeFile(path.join(root, 'evidence.md'), '# evidence\n', 'utf8');
        runFixtureGit(root, ['init']);
        runFixtureGit(root, ['config', 'user.email', 'fixture@example.com']);
        runFixtureGit(root, ['config', 'user.name', 'Fixture']);
        runFixtureGit(root, ['add', 'evidence.md']);
        runFixtureGit(root, ['commit', '-m', 'fixture']);
        const headSha = runFixtureGit(root, ['rev-parse', 'HEAD']).stdout.trim();
        const document = createReleaseDocument();
        const item = document.items.find((candidate) => candidate.id === 'current-head-builds');
        item.status = 'verified';
        item.proof = {
            releaseCandidateCommitSha: headSha,
            windows: {
                commitSha: headSha,
                buildPassed: true,
                testsPassed: true,
                checkedAt: '2026-08-28T12:00:00+09:00'
            },
            linuxServer: {
                commitSha: '5599de19687c3ed446f7242c72711bc9b34b3364',
                buildPassed: true,
                testsPassed: true,
                checkedAt: '2026-08-28T12:00:00+09:00'
            },
            hostedCi: {
                commitSha: headSha,
                status: 'success',
                runUrl: 'https://example.com/actions/runs/1',
                checkedAt: '2026-08-28T12:00:00+09:00'
            }
        };

        const errors = await validateReleaseStatus(document, { root });

        assert.ok(errors.some((error) => (
            error.includes('current-head-builds.proof.linuxServer.commitSha')
            && error.includes('release candidate')
        )), errors.join('\n'));
        for (const environment of ['windows', 'linuxServer']) {
            assert.ok(errors.some((error) => (
                error.includes(`current-head-builds.proof.${environment}.evidencePath`)
                && error.includes('evidence path')
            )), `${environment}:\n${errors.join('\n')}`);
        }
    } finally {
        await rm(root, { recursive: true, force: true });
    }
});

test('WARP and RTX proof requires contained files and explicit visual review data', async () => {
    await withReleaseFixture(async (document, root) => {
        const item = document.items.find((candidate) => candidate.id === 'warp-rtx-visual-artifacts');
        item.status = 'verified';
        item.proof = {
            warp: {
                resultPath: 'docs/visual/warp-result.json',
                offscreen: true,
                status: 'passed',
                checkedAt: '2026-08-28T12:00:00+09:00'
            },
            rtx: {
                artifactPaths: ['docs/visual/rtx-frame.png'],
                review: {
                    checkedAt: '',
                    reviewer: '',
                    verdict: 'pending',
                    notes: ''
                }
            }
        };

        const errors = await validateReleaseStatus(document, { root });

        assert.ok(errors.some((error) => (
            error.includes('warp-rtx-visual-artifacts.proof.warp.resultPath')
            && error.includes('evidence path')
        )), errors.join('\n'));
        assert.ok(errors.some((error) => (
            error.includes('warp-rtx-visual-artifacts.proof.rtx.artifactPaths[0]')
            && error.includes('evidence path')
        )), errors.join('\n'));
        for (const field of ['checkedAt', 'reviewer', 'verdict', 'notes']) {
            assert.ok(errors.some((error) => (
                error.includes(`warp-rtx-visual-artifacts.proof.rtx.review.${field}`)
            )), `${field}:\n${errors.join('\n')}`);
        }
    });
});

test('release evidence elements must be repository-relative path strings', async () => {
    await withReleaseFixture(async (document, root) => {
        document.items[0].evidence = [42];

        const errors = await validateReleaseStatus(document, { root });

        assert.ok(errors.some((error) => error.includes('historical-24-player-metrics.evidence[0]')));
    });
});

test('missing release evidence keeps an actionable field-level error', async () => {
    await withReleaseFixture(async (document, root) => {
        document.items[0].evidence = ['docs/missing.md'];

        const errors = await validateReleaseStatus(document, { root });

        assert.ok(errors.includes(
            'release-status.json.historical-24-player-metrics.evidence[0]: local evidence path is missing: docs/missing.md'
        ));
    });
});

test('release evidence and Markdown links reject a junction that resolves outside the root', async () => {
    const root = await mkdtemp(path.join(tmpdir(), 'dxa-portfolio-junction-root-'));
    const outsideRoot = await mkdtemp(path.join(tmpdir(), 'dxa-portfolio-junction-outside-'));
    try {
        await writeFile(path.join(root, 'evidence.md'), '# evidence\n', 'utf8');
        await writeFile(path.join(outsideRoot, 'proof.md'), '# outside\n', 'utf8');
        await symlink(outsideRoot, path.join(root, 'escape'), 'junction');

        const document = createReleaseDocument();
        document.items[0].evidence = ['escape/proof.md'];
        const releaseErrors = await validateReleaseStatus(document, { root });

        await writeFile(path.join(root, 'README.md'), '[outside](escape/proof.md)\n', 'utf8');
        const markdownErrors = await validateMarkdownLinks({ root, files: ['README.md'] });

        assert.ok(releaseErrors.some((error) => error.includes(
            'historical-24-player-metrics.evidence[0]: real target resolves outside repository root'
        )));
        assert.deepEqual(markdownErrors, [
            'README.md: local link target resolves outside repository root: escape/proof.md'
        ]);
    } finally {
        await rm(root, { recursive: true, force: true });
        await rm(outsideRoot, { recursive: true, force: true });
    }
});

test('empty class diagram outputs cannot satisfy a verified state', async () => {
    await withReleaseFixture(async (document, root) => {
        const paths = [
            'docs/diagrams/class/engine.json',
            'docs/diagrams/class/network.json',
            'docs/diagrams/class/engine.html',
            'docs/diagrams/class/network.html'
        ];
        for (const relativePath of paths) {
            const filePath = path.join(root, relativePath);
            await mkdir(path.dirname(filePath), { recursive: true });
            await writeFile(filePath, '', 'utf8');
        }
        const item = document.items.find((candidate) => candidate.id === 'class-diagrams');
        item.status = 'verified';
        item.evidence = paths;
        item.proof = {
            tool: 'clang-uml',
            toolVersion: '0.6.3',
            basisCommitSha,
            outputs: {
                engine: { json: paths[0], html: paths[2] },
                network: { json: paths[1], html: paths[3] }
            }
        };

        const errors = await validateReleaseStatus(document, { root });

        for (const index of paths.keys()) {
            assert.ok(errors.some((error) => error.includes(`class-diagrams.evidence[${index}]`)), paths[index]);
        }
    });
});

test('verified class diagrams require clang-uml 0.6.3 metadata at the canonical basis', async () => {
    await withReleaseFixture(async (document, root) => {
        const paths = [
            'docs/diagrams/class/engine.json',
            'docs/diagrams/class/network.json',
            'docs/diagrams/class/engine.html',
            'docs/diagrams/class/network.html'
        ];
        for (const relativePath of paths) {
            const filePath = path.join(root, relativePath);
            await mkdir(path.dirname(filePath), { recursive: true });
            await writeFile(filePath, 'generated output\n', 'utf8');
        }
        const item = document.items.find((candidate) => candidate.id === 'class-diagrams');
        item.status = 'verified';
        item.evidence = paths;
        item.proof = {
            tool: 'manual',
            toolVersion: '0.6.2',
            basisCommitSha: '5599de19687c3ed446f7242c72711bc9b34b3364',
            outputs: {
                engine: { json: paths[0], html: paths[2] },
                network: { json: paths[1], html: paths[3] }
            }
        };

        const errors = await validateReleaseStatus(document, { root });

        assert.ok(errors.some((error) => error.includes('class-diagrams.proof.tool')));
        assert.ok(errors.some((error) => error.includes('class-diagrams.proof.toolVersion')));
        assert.ok(errors.some((error) => error.includes('class-diagrams.proof.basisCommitSha')));
    });
});

test('verified class diagrams reject four nonempty hand-authored junk files', async () => {
    await withReleaseFixture(async (document, root) => {
        const config = 'diagrams: {}\n';
        const generator = 'Write-Output manual\n';
        await writeFile(path.join(root, '.clang-uml'), config, 'utf8');
        await mkdir(path.join(root, 'scripts/portfolio'), { recursive: true });
        await writeFile(path.join(root, 'scripts/portfolio/generate-class-diagrams.ps1'), generator, 'utf8');
        const proof = setVerifiedClassProof(document, {
            config: { path: '.clang-uml', sha256: normalizedTextSha256(config) },
            generator: {
                path: 'scripts/portfolio/generate-class-diagrams.ps1',
                sha256: normalizedTextSha256(generator)
            }
        });
        for (const relativePath of proof.paths) {
            const filePath = path.join(root, relativePath);
            await mkdir(path.dirname(filePath), { recursive: true });
            await writeFile(filePath, 'nonempty manual junk\n', 'utf8');
        }

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.ok(errors.some((error) => error.includes('engine.json') && error.includes('raw clang-uml')));
        assert.ok(errors.some((error) => error.includes('network.json') && error.includes('raw clang-uml')));
    });
});

test('verified class diagrams reject HTML that does not match the production renderer', async () => {
    await withReleaseFixture(async (document, root) => {
        await createValidClassProofFixture(document, root);
        const htmlPath = path.join(root, 'docs/diagrams/class/engine.html');
        const html = await readFile(htmlPath, 'utf8');
        await writeFile(htmlPath, `${html}\n<!-- tampered -->\n`, 'utf8');

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.ok(
            errors.some((error) => error.includes('engine.html') && error.includes('renderer')),
            errors.join('\n')
        );
    });
});

test('verified class diagrams reject changed config and generator provenance', async () => {
    await withReleaseFixture(async (document, root) => {
        await createValidClassProofFixture(document, root);
        await writeFile(path.join(root, '.clang-uml'), 'tampered config\n', 'utf8');
        await writeFile(
            path.join(root, 'scripts/portfolio/generate-class-diagrams.ps1'),
            'tampered generator\n',
            'utf8'
        );

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.ok(errors.some((error) => error.includes('proof.inputs.config.sha256')));
        assert.ok(errors.some((error) => error.includes('proof.inputs.generator.sha256')));
    });
});

test('verified class diagrams require a generation-time provenance manifest', async () => {
    await withReleaseFixture(async (document, root) => {
        const { item } = await createValidClassProofFixture(document, root);
        item.evidence = item.evidence.filter((relativePath) => relativePath !== 'docs/diagrams/class/manifest.json');
        delete item.proof.manifest;
        await rm(path.join(root, 'docs/diagrams/class/manifest.json'));

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.ok(errors.some((error) => error.includes('proof.manifest.path')));
        assert.ok(errors.some((error) => error.includes('proof.manifest.sha256')));
    });
});

test('verified class manifest validates without ignored generation build artifacts', async () => {
    await withReleaseFixture(async (document, root) => {
        await createValidClassProofFixture(document, root);

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.equal(errors.some((error) => error.includes('class-diagrams')), false, errors.join('\n'));
        assert.equal(await readdir(path.join(root, 'out')).catch((error) => error.code), 'ENOENT');
    });
});

test('quoted long-path compiler canonicalizes through generation and the public verifier', async () => {
    await withReleaseFixture(async (document, root) => {
        const { item } = await createValidClassProofFixture(document, root);
        const snapshotPath = 'docs/diagrams/class/evidence/compile-commands.json';
        const generator = await readFile(generatorScriptPath, 'utf8');

        await updateClassSnapshotFixture(
            root,
            item,
            snapshotPath,
            (original) => {
                const snapshot = JSON.parse(original);
                const canonicalCommand = snapshot.entries[0].command;
                const suffix = canonicalCommand.slice('${MSVC_COMPILER}'.length);
                const quotedCommand = '"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe"'
                    + suffix;
                const generatedCommand = canonicalizeCompileCommandThroughGenerator(quotedCommand);

                assert.equal(generatedCommand, canonicalCommand);
                assert.equal(generatedCommand.slice('${MSVC_COMPILER}'.length), suffix);
                snapshot.entries[0].command = generatedCommand;
                return `${JSON.stringify(snapshot, null, 2)}\n`;
            },
            (manifest) => {
                manifest.tooling.generator.sha256 = normalizedTextSha256(generator);
            }
        );

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });
        const compileErrors = errors.filter((error) => error.includes('compile-commands.json'));

        assert.deepEqual(compileErrors, [], errors.join('\n'));
    });
});

test('compiler first-token canonicalization does not mutate quoted or unquoted arguments', () => {
    const cases = [
        {
            label: 'quoted long path with leading and mixed argument whitespace',
            compilerPath: compileReplacementFixture.compiler,
            commandPrefix: `   "${compileReplacementFixture.compiler}"`,
            suffix: `  /nologo\t/DWRAPPED="${compileReplacementFixture.compiler}" /DQUOTED=""value with spaces""`
        },
        {
            label: 'unquoted path with caret-escaped argument quotes',
            compilerPath: 'C:/MSVC/bin/cl.exe',
            commandPrefix: 'C:/MSVC/bin/cl.exe',
            suffix: '\t/nologo  /DWRAPPED="C:/MSVC/bin/cl.exe" /DQUOTED=^"value with spaces^"'
        }
    ];

    for (const scenario of cases) {
        const generated = canonicalizeCompileCommandThroughGenerator(
            scenario.commandPrefix + scenario.suffix,
            { compilerPath: scenario.compilerPath }
        );

        assert.equal(
            generated,
            '${MSVC_COMPILER}' + scenario.suffix,
            scenario.label
        );
        assert.equal(
            generated.slice('${MSVC_COMPILER}'.length),
            scenario.suffix,
            `${scenario.label}: argument suffix changed`
        );
    }
});

test('compiler exclusion retains repository, build and vcpkg privacy replacements', () => {
    const suffix = [
        `  /DWRAPPED="${compileReplacementFixture.compiler}"`,
        ` /I"${compileReplacementFixture.vcpkg}/include"`,
        ` /Fo"${compileReplacementFixture.build}/obj/example.obj"`,
        ` /c "${compileReplacementFixture.repository}/apps/example.cpp"`
    ].join('');
    const generated = canonicalizeCompileCommandThroughGenerator(
        `"${compileReplacementFixture.compiler}"${suffix}`
    );

    assert.equal(
        generated,
        '${MSVC_COMPILER}'
            + `  /DWRAPPED="${compileReplacementFixture.compiler}"`
            + ' /I"${VCPKG_INSTALLED_ROOT}/include"'
            + ' /Fo"${BUILD_ROOT}/obj/example.obj"'
            + ' /c "${REPOSITORY_ROOT}/apps/example.cpp"'
    );
});

test('compiler replacement ambiguity fails closed without excluding a same-token rule by value', () => {
    const result = runCompileCommandThroughGenerator(
        `"${compileReplacementFixture.compiler}" /nologo`,
        {
            additionalReplacements: [
                { source: 'C:/unrelated/tool.exe', token: '${MSVC_COMPILER}' }
            ]
        }
    );

    assert.notEqual(result.status, 0, result.stdout);
    assert.match(
        result.stderr + result.stdout,
        /exactly one compiler replacement/iu
    );
});

test('verified class diagrams reject a hand-edited manifest even when its proof hash is updated', async () => {
    await withReleaseFixture(async (document, root) => {
        const { item } = await createValidClassProofFixture(document, root);
        const manifestPath = path.join(root, 'docs/diagrams/class/manifest.json');
        const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
        manifest.schemaVersion = 99;
        const tampered = `${JSON.stringify(manifest, null, 2)}\n`;
        await writeFile(manifestPath, tampered, 'utf8');
        item.proof.manifest.sha256 = sha256(tampered);

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.ok(errors.some((error) => error.includes('proof.manifest.schemaVersion')));
    });
});

test('verified class diagrams reject raw JSON that no longer matches the generation manifest', async () => {
    await withReleaseFixture(async (document, root) => {
        await createValidClassProofFixture(document, root);
        const enginePath = path.join(root, 'docs/diagrams/class/engine.json');
        const engine = JSON.parse(await readFile(enginePath, 'utf8'));
        engine.elements[0].name = 'TamperedRenderer';
        await writeFile(enginePath, `${JSON.stringify(engine, null, 2)}\n`, 'utf8');

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.ok(errors.some((error) => error.includes('proof.manifest.diagrams.engine')));
    });
});

test('verified class diagrams require the exact committed generation snapshot set', async () => {
    await withReleaseFixture(async (document, root) => {
        await createValidClassProofFixture(document, root);
        await rm(path.join(root, classEvidenceSnapshotPaths[0]));

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.ok(
            errors.some((error) => error.includes('evidence/cmake-cache.json') && error.includes('missing')),
            errors.join('\n')
        );
    });

    await withReleaseFixture(async (document, root) => {
        await createValidClassProofFixture(document, root);
        await writeFile(
            path.join(root, 'docs/diagrams/class/evidence/manual-proof.txt'),
            'manual proof\n',
            'utf8'
        );

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.ok(
            errors.some((error) => error.includes('evidence/manual-proof.txt') && error.includes('extra')),
            errors.join('\n')
        );
    });
});

test('verified class diagrams reconstruct compile evidence after manifest hashes are recomputed', async () => {
    await withReleaseFixture(async (document, root) => {
        const { item } = await createValidClassProofFixture(document, root);
        const snapshotPath = 'docs/diagrams/class/evidence/compile-commands.json';
        await updateClassSnapshotFixture(root, item, snapshotPath, (original) => {
            const snapshot = JSON.parse(original);
            snapshot.entries[0].command = snapshot.entries[0].command.replace(
                '${MSVC_COMPILER}',
                'manual-cl.exe'
            );
            return `${JSON.stringify(snapshot, null, 2)}\n`;
        });

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.ok(
            errors.some((error) => error.includes('compile-commands.json') && error.includes('compiler token')),
            errors.join('\n')
        );
    });
});

test('verified class diagrams reconstruct normalized CMake and vcpkg roots', async () => {
    await withReleaseFixture(async (document, root) => {
        const { item } = await createValidClassProofFixture(document, root);
        const snapshotPath = 'docs/diagrams/class/evidence/cmake-cache.json';
        await updateClassSnapshotFixture(root, item, snapshotPath, (original) => {
            const snapshot = JSON.parse(original);
            snapshot.content = snapshot.content.replace(
                'VCPKG_MANIFEST_DIR:PATH=${REPOSITORY_ROOT}',
                'VCPKG_MANIFEST_DIR:PATH=${BUILD_ROOT}'
            );
            return `${JSON.stringify(snapshot, null, 2)}\n`;
        });

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.ok(
            errors.some((error) => error.includes('VCPKG_MANIFEST_DIR')),
            errors.join('\n')
        );
    });
});

test('verified class diagrams reject schema-valid forged opaque generation digests', async () => {
    await withReleaseFixture(async (document, root) => {
        const { item } = await createValidClassProofFixture(document, root);
        const manifestPath = path.join(root, 'docs/diagrams/class/manifest.json');
        const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
        manifest.compilation.compileCommands.sha256 = 'a'.repeat(64);
        manifest.compilation.cmakeCache.sha256 = 'b'.repeat(64);
        manifest.dependencies.installed.status.sha256 = 'c'.repeat(64);
        await writeClassManifestFixture(root, item, manifest);

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.ok(
            errors.some((error) => error.includes('tool-identities.json') && error.includes('sourceDigests')),
            errors.join('\n')
        );
    });
});

const privateHomePathScenarios = [
    ['Windows user alice', 'C:/Users/alice/AppData/Local/Temp/build'],
    ['Windows user bob', 'D:\\Users\\bob\\source\\project'],
    ['Unix user carol', '/home/carol/source/project'],
    ['macOS user dana', '/Users/dana/source/project']
];

for (const [label, privatePath] of privateHomePathScenarios) {
    test(`verified class diagrams reject ${label} home paths`, async () => {
        await withReleaseFixture(async (document, root) => {
            const { item } = await createValidClassProofFixture(document, root);
            const snapshotPath = 'docs/diagrams/class/evidence/cmake-cache.json';
            await updateClassSnapshotFixture(
                root,
                item,
                snapshotPath,
                (original) => {
                    const snapshot = JSON.parse(original);
                    snapshot.content += `\nPRIVATE_PATH:INTERNAL=${privatePath}\n`;
                    return `${JSON.stringify(snapshot, null, 2)}\n`;
                }
            );

            const errors = await validateReleaseStatus(document, {
                root,
                verifyClassBasisCommit: false
            });

            assert.ok(
                errors.some((error) => error.includes('cmake-cache.json') && error.includes('privacy-safe')),
                errors.join('\n')
            );
        });
    });
}

test('privacy scan accepts repository-relative documentation paths containing home', async () => {
    await withReleaseFixture(async (document, root) => {
        const { item } = await createValidClassProofFixture(document, root);
        const snapshotPath = 'docs/diagrams/class/evidence/cmake-cache.json';
        await updateClassSnapshotFixture(
            root,
            item,
            snapshotPath,
            (original) => {
                const snapshot = JSON.parse(original);
                snapshot.content += '\nPROJECT_GUIDE:INTERNAL=docs/home/carol/README.md\n';
                return `${JSON.stringify(snapshot, null, 2)}\n`;
            }
        );

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.equal(
            errors.some((error) => error.includes(snapshotPath) && error.includes('privacy-safe')),
            false,
            errors.join('\n')
        );
    });
});

const machineHostSnapshotScenarios = [
    {
        label: 'CMake SITE cache entry',
        snapshotPath: 'docs/diagrams/class/evidence/cmake-cache.json',
        mutate(snapshot) {
            snapshot.content += '\nSITE:STRING=LEAKED_HOST_VALUE\n';
        }
    },
    {
        label: 'compile command COMPUTERNAME define',
        snapshotPath: 'docs/diagrams/class/evidence/compile-commands.json',
        mutate(snapshot) {
            snapshot.entries[0].command += ' /DCOMPUTERNAME=LEAKED_HOST_VALUE';
        }
    },
    {
        label: 'tool identity hostName field',
        snapshotPath: 'docs/diagrams/class/evidence/tool-identities.json',
        mutate(snapshot) {
            snapshot.hostName = 'LEAKED_HOST_VALUE';
        }
    },
    {
        label: 'vcpkg metadata HOSTNAME value',
        snapshotPath: 'docs/diagrams/class/evidence/vcpkg-metadata.json',
        mutate(snapshot) {
            snapshot.files[0].content += 'HOSTNAME=LEAKED_HOST_VALUE\n';
            snapshot.files[0].sha256 = sha256(snapshot.files[0].content);
        },
        updateManifest(manifest, updated) {
            synchronizeInstalledMetadataManifest(manifest, JSON.parse(updated));
        }
    },
    {
        label: 'vcpkg status Host field',
        snapshotPath: 'docs/diagrams/class/evidence/vcpkg-status.json',
        mutate(snapshot) {
            snapshot.content = snapshot.content.replace(
                '\n\n',
                '\nHost: LEAKED_HOST_VALUE\n\n'
            );
        },
        async updateManifest(manifest, updated, root) {
            const status = JSON.parse(updated);
            const statusSha256 = sha256(status.content);
            manifest.dependencies.installed.status.sha256 = statusSha256;

            const toolPath = 'docs/diagrams/class/evidence/tool-identities.json';
            const absoluteToolPath = path.join(root, toolPath);
            const tools = JSON.parse(await readFile(absoluteToolPath, 'utf8'));
            tools.sourceDigests.vcpkgStatusRawSha256 = statusSha256;
            const updatedTools = `${JSON.stringify(tools, null, 2)}\n`;
            await writeFile(absoluteToolPath, updatedTools, 'utf8');
            manifest.snapshots.find((entry) => entry.path === toolPath).sha256 = sha256(updatedTools);
        }
    }
];

for (const scenario of machineHostSnapshotScenarios) {
    test(`verified class diagrams scan ${scenario.label} for machine identifiers`, async () => {
        await withReleaseFixture(async (document, root) => {
            const { item } = await createValidClassProofFixture(document, root);
            await updateClassSnapshotFixture(
                root,
                item,
                scenario.snapshotPath,
                (original) => {
                    const snapshot = JSON.parse(original);
                    scenario.mutate(snapshot);
                    return `${JSON.stringify(snapshot, null, 2)}\n`;
                },
                scenario.updateManifest
                    ? (manifest, updated) => scenario.updateManifest(manifest, updated, root)
                    : undefined
            );

            const errors = await validateReleaseStatus(document, {
                root,
                verifyClassBasisCommit: false
            });

            assert.ok(
                errors.some((error) => (
                    error.includes(scenario.snapshotPath) && error.includes('privacy-safe')
                )),
                errors.join('\n')
            );
        });
    });
}

test('verified class diagrams bind compiler volume identity as well as file ID', async () => {
    await withReleaseFixture(async (document, root) => {
        const { item } = await createValidClassProofFixture(document, root);
        const snapshotPath = 'docs/diagrams/class/evidence/tool-identities.json';
        await updateClassSnapshotFixture(root, item, snapshotPath, (original) => {
            const snapshot = JSON.parse(original);
            snapshot.compilerIdentity.volumeSerialNumber = '0xdeadbeef';
            return `${JSON.stringify(snapshot, null, 2)}\n`;
        });

        const errors = await validateReleaseStatus(document, {
            root,
            verifyClassBasisCommit: false
        });

        assert.ok(
            errors.some((error) => error.includes('compilerIdentity.volumeSerialNumber')),
            errors.join('\n')
        );
    });
});

test('verified class diagrams enforce vcpkg status, list, and ABI completeness', async () => {
    const scenarios = [
        {
            label: 'missing list',
            mutate(files) {
                const index = files.findIndex((entry) => entry.path.endsWith('.list'));
                files.splice(index, 1);
            },
            expected: 'missing installed list'
        },
        {
            label: 'extra list',
            mutate(files) {
                const source = files.find((entry) => entry.path.endsWith('.list'));
                files.push({
                    ...source,
                    path: 'vcpkg/info/manual-package_1.0.0_x64-windows.list'
                });
            },
            expected: 'extra installed list'
        },
        {
            label: 'missing ABI',
            mutate(files) {
                const index = files.findIndex((entry) => entry.path.endsWith('/vcpkg_abi_info.txt'));
                files.splice(index, 1);
            },
            expected: 'missing ABI metadata'
        },
        {
            label: 'extra ABI',
            mutate(files) {
                const source = files.find((entry) => entry.path.endsWith('/vcpkg_abi_info.txt'));
                files.push({
                    ...source,
                    path: 'x64-windows/share/manual-package/vcpkg_abi_info.txt'
                });
            },
            expected: 'extra ABI metadata'
        }
    ];

    for (const scenario of scenarios) {
        await withReleaseFixture(async (document, root) => {
            const { item } = await createValidClassProofFixture(document, root);
            const snapshotPath = 'docs/diagrams/class/evidence/vcpkg-metadata.json';
            await updateClassSnapshotFixture(
                root,
                item,
                snapshotPath,
                (original) => {
                    const metadata = JSON.parse(original);
                    scenario.mutate(metadata.files);
                    metadata.files.sort((left, right) => left.path.localeCompare(right.path, 'en'));
                    return `${JSON.stringify(metadata, null, 2)}\n`;
                },
                (manifest, updated) => synchronizeInstalledMetadataManifest(
                    manifest,
                    JSON.parse(updated)
                )
            );

            const errors = await validateReleaseStatus(document, {
                root,
                verifyClassBasisCommit: false
            });

            assert.ok(
                errors.some((error) => error.includes(scenario.expected)),
                `${scenario.label}:\n${errors.join('\n')}`
            );
        });
    }
});

test('empty PDF and demo files cannot satisfy verified states', async () => {
    await withReleaseFixture(async (document, root) => {
        const pdfPath = 'docs/portfolio/portfolio.pdf';
        const videoPath = 'docs/portfolio/demo.mp4';
        await mkdir(path.join(root, 'docs/portfolio'), { recursive: true });
        await writeFile(path.join(root, pdfPath), '', 'utf8');
        await writeFile(path.join(root, videoPath), '', 'utf8');

        const pdf = document.items.find((item) => item.id === 'portfolio-pdf');
        pdf.status = 'verified';
        pdf.evidence = [pdfPath];
        pdf.proof = { path: pdfPath, pageCount: 20, rendered: true, linksChecked: true };

        const video = document.items.find((item) => item.id === 'demo-video');
        video.status = 'verified';
        video.evidence = [videoPath];
        video.proof = {
            path: videoPath,
            durationSeconds: 180,
            playbackChecked: true,
            localDemoChecked: true
        };

        const errors = await validateReleaseStatus(document, { root });

        assert.ok(errors.some((error) => error.includes('portfolio-pdf.evidence[0]')));
        assert.ok(errors.some((error) => error.includes('demo-video.evidence[0]')));
    });
});

test('verified PDF and demo states require complete render and playback proof', async () => {
    await withReleaseFixture(async (document, root) => {
        const pdfPath = 'docs/portfolio/portfolio.pdf';
        const videoPath = 'docs/portfolio/demo.mp4';
        await mkdir(path.join(root, 'docs/portfolio'), { recursive: true });
        await writeFile(path.join(root, pdfPath), 'pdf\n', 'utf8');
        await writeFile(path.join(root, videoPath), 'video\n', 'utf8');

        const pdf = document.items.find((item) => item.id === 'portfolio-pdf');
        pdf.status = 'verified';
        pdf.evidence = [pdfPath];
        pdf.proof = { path: pdfPath, pageCount: 17, rendered: false, linksChecked: false };

        const video = document.items.find((item) => item.id === 'demo-video');
        video.status = 'verified';
        video.evidence = [videoPath];
        video.proof = {
            path: videoPath,
            durationSeconds: 0,
            playbackChecked: false,
            localDemoChecked: false
        };

        const errors = await validateReleaseStatus(document, { root });

        for (const field of ['pageCount', 'rendered', 'linksChecked']) {
            assert.ok(errors.some((error) => error.includes(`portfolio-pdf.proof.${field}`)), field);
        }
        for (const field of ['durationSeconds', 'playbackChecked', 'localDemoChecked']) {
            assert.ok(errors.some((error) => error.includes(`demo-video.proof.${field}`)), field);
        }
    });
});

test('external verified states require nonempty dated checks', async () => {
    await withReleaseFixture(async (document, root) => {
        const aws = document.items.find((item) => item.id === 'aws-external-test');
        aws.status = 'verified';
        aws.resourceState = {
            created: true,
            externalTestVerified: true,
            cleanupVerified: true,
            checkedAt: ''
        };

        const visibility = document.items.find((item) => item.id === 'repository-visibility');
        visibility.status = 'verified';
        visibility.evidence = ['evidence.md'];
        visibility.externalVerification = {
            visibility: 'public',
            checkedAt: '',
            url: 'https://example.com/repository'
        };

        const errors = await validateReleaseStatus(document, { root });

        assert.ok(errors.some((error) => error.includes('aws-external-test.resourceState.checkedAt')));
        assert.ok(errors.some((error) => error.includes('repository-visibility.externalVerification.checkedAt')));
    });
});

test('AWS cannot be verified unless resources, external test, and cleanup were all verified', async () => {
    await withReleaseFixture(async (document, root) => {
        const aws = document.items.find((item) => item.id === 'aws-external-test');
        aws.status = 'verified';
        aws.resourceState = {
            created: false,
            externalTestVerified: false,
            cleanupVerified: false,
            checkedAt: '2026-08-27T12:00:00+09:00'
        };

        const errors = await validateReleaseStatus(document, { root });

        for (const field of ['created', 'externalTestVerified', 'cleanupVerified']) {
            assert.ok(errors.some((error) => error.includes(`aws-external-test.resourceState.${field}`)), field);
        }
    });
});

test('LFS object availability requires dated object-store and hydration proof before verified', async () => {
    await withReleaseFixture(async (document, root) => {
        const lfs = document.items.find((item) => item.id === 'lfs-object-availability');
        lfs.status = 'verified';
        lfs.proof = {
            checkedAt: '',
            objectsVerified: false,
            objectsHydrated: false,
            paths: []
        };

        const errors = await validateReleaseStatus(document, { root });

        for (const field of ['checkedAt', 'objectsVerified', 'objectsHydrated', 'paths']) {
            assert.ok(errors.some((error) => error.includes(`lfs-object-availability.proof.${field}`)), field);
        }
    });
});

test('LFS verification is read-only for exact tracked pointer, object-store, and worktree matches', async () => {
    const fixture = await createLfsRepositoryFixture();
    try {
        const before = await snapshotFixtureRepository(fixture.root, fixture.objectRoot);

        const errors = await validateReleaseStatus(fixture.document, { root: fixture.root });

        const after = await snapshotFixtureRepository(fixture.root, fixture.objectRoot);
        assert.deepEqual(errors.filter((error) => error.includes('lfs-object-availability')), []);
        assert.deepEqual(after, before);
    } finally {
        await rm(fixture.root, { recursive: true, force: true });
    }
});

test('LFS verification rejects an uncommitted worktree attribute rule absent from HEAD', async () => {
    const fixture = await createLfsRepositoryFixture({ headHasLfsRule: false });
    try {
        const errors = await validateReleaseStatus(fixture.document, { root: fixture.root });

        assert.ok(errors.some((error) => error.includes('proof.paths[0].attribute')
            && error.includes(requiredRuntimeLfsPaths[0])
            && error.includes('committed HEAD')));
    } finally {
        await rm(fixture.root, { recursive: true, force: true });
    }
});

test('LFS verification rejects CRLF and missing-terminal-LF committed pointer bytes', async () => {
    const fixture = await createLfsRepositoryFixture();
    const [firstAsset] = fixture.assetRecords;
    try {
        const crlfPointer = Buffer.from(
            `version https://git-lfs.github.com/spec/v1\r\noid sha256:${firstAsset.oid}\r\nsize ${firstAsset.size}\r\n`,
            'ascii'
        );
        await commitFixtureBlob(fixture, firstAsset.relativePath, crlfPointer, 'crlf pointer');
        let errors = await validateReleaseStatus(fixture.document, { root: fixture.root });
        assert.ok(errors.some((error) => error.includes('proof.paths[0].pointer')
            && error.includes(firstAsset.relativePath)));

        const missingTerminalLf = Buffer.from(
            `version https://git-lfs.github.com/spec/v1\noid sha256:${firstAsset.oid}\nsize ${firstAsset.size}`,
            'ascii'
        );
        await commitFixtureBlob(fixture, firstAsset.relativePath, missingTerminalLf, 'missing terminal LF');
        errors = await validateReleaseStatus(fixture.document, { root: fixture.root });
        assert.ok(errors.some((error) => error.includes('proof.paths[0].pointer')
            && error.includes(firstAsset.relativePath)));
    } finally {
        await rm(fixture.root, { recursive: true, force: true });
    }
});

test('LFS verification rejects corrupted, missing, worktree-mismatched, and wrong-path objects read-only', async () => {
    const fixture = await createLfsRepositoryFixture();
    const [firstAsset] = fixture.assetRecords;
    try {
        await writeFile(firstAsset.objectPath, 'corrupt object\n', 'utf8');
        let before = await snapshotFixtureRepository(fixture.root, fixture.objectRoot);
        let errors = await validateReleaseStatus(fixture.document, { root: fixture.root });
        let after = await snapshotFixtureRepository(fixture.root, fixture.objectRoot);
        assert.ok(errors.some((error) => error.includes('object SHA-256 or size does not match committed pointer')));
        assert.deepEqual(after, before);

        await rm(firstAsset.objectPath, { force: true });
        before = await snapshotFixtureRepository(fixture.root, fixture.objectRoot);
        errors = await validateReleaseStatus(fixture.document, { root: fixture.root });
        after = await snapshotFixtureRepository(fixture.root, fixture.objectRoot);
        assert.ok(errors.some((error) => error.includes('LFS object file is missing')));
        assert.deepEqual(after, before);

        await mkdir(path.dirname(firstAsset.objectPath), { recursive: true });
        await writeFile(firstAsset.objectPath, firstAsset.content);
        await writeFile(path.join(fixture.root, firstAsset.relativePath), 'corrupt worktree\n', 'utf8');
        before = await snapshotFixtureRepository(fixture.root, fixture.objectRoot);
        errors = await validateReleaseStatus(fixture.document, { root: fixture.root });
        after = await snapshotFixtureRepository(fixture.root, fixture.objectRoot);
        assert.ok(errors.some((error) => error.includes('worktree SHA-256 or size does not match LFS object')));
        assert.deepEqual(after, before);

        await writeFile(path.join(fixture.root, firstAsset.relativePath), firstAsset.content);
        const lfs = fixture.document.items.find((item) => item.id === 'lfs-object-availability');
        lfs.evidence = [...requiredRuntimeLfsPaths.slice(0, 2), 'LICENSE'];
        lfs.proof.paths = [...requiredRuntimeLfsPaths.slice(0, 2), 'LICENSE'];
        before = await snapshotFixtureRepository(fixture.root, fixture.objectRoot);
        errors = await validateReleaseStatus(fixture.document, { root: fixture.root });
        after = await snapshotFixtureRepository(fixture.root, fixture.objectRoot);
        assert.ok(errors.some((error) => error.includes('must exactly list the sorted runtime LFS paths')));
        assert.ok(errors.some((error) => error.includes('LICENSE') && error.includes('not LFS-tracked')));
        assert.deepEqual(after, before);
    } finally {
        await rm(fixture.root, { recursive: true, force: true });
    }
});

test('v0.1.0 requires an actual local tag and every publication prerequisite', async () => {
    await withReleaseFixture(async (document, root) => {
        const tag = document.items.find((item) => item.id === 'v0.1.0');
        tag.status = 'verified';
        tag.evidence = ['evidence.md'];
        tag.proof = { tag: 'v0.1.0' };

        const errors = await validateReleaseStatus(document, { root });

        assert.ok(errors.some((error) => error.includes('v0.1.0.proof.tag') && error.includes('local tag')));
        assert.ok(errors.some((error) => error.includes('v0.1.0.status') && error.includes('prerequisites')));
    });
});

test('v0.1.0 target must equal the verified release-candidate HEAD', async () => {
    const root = await mkdtemp(path.join(tmpdir(), 'dxa-tag-target-proof-'));
    try {
        await writeFile(path.join(root, 'evidence.md'), '# first\n', 'utf8');
        runFixtureGit(root, ['init']);
        runFixtureGit(root, ['config', 'user.email', 'fixture@example.com']);
        runFixtureGit(root, ['config', 'user.name', 'Fixture']);
        runFixtureGit(root, ['add', 'evidence.md']);
        runFixtureGit(root, ['commit', '-m', 'first']);
        const tagTargetSha = runFixtureGit(root, ['rev-parse', 'HEAD']).stdout.trim();
        runFixtureGit(root, ['tag', 'v0.1.0', tagTargetSha]);

        await writeFile(path.join(root, 'evidence.md'), '# release candidate\n', 'utf8');
        runFixtureGit(root, ['add', 'evidence.md']);
        runFixtureGit(root, ['commit', '-m', 'release candidate']);
        const releaseCandidateCommitSha = runFixtureGit(root, ['rev-parse', 'HEAD']).stdout.trim();

        const document = createReleaseDocument();
        for (const item of document.items) {
            item.status = 'verified';
            item.evidence = ['evidence.md'];
        }
        const currentHead = document.items.find((item) => item.id === 'current-head-builds');
        currentHead.proof = {
            releaseCandidateCommitSha,
            windows: {
                commitSha: releaseCandidateCommitSha,
                buildPassed: true,
                testsPassed: true,
                checkedAt: '2026-08-28T12:00:00+09:00'
            },
            linuxServer: {
                commitSha: releaseCandidateCommitSha,
                buildPassed: true,
                testsPassed: true,
                checkedAt: '2026-08-28T12:00:00+09:00'
            },
            hostedCi: {
                commitSha: releaseCandidateCommitSha,
                status: 'success',
                runUrl: 'https://example.com/actions/runs/1',
                checkedAt: '2026-08-28T12:00:00+09:00'
            }
        };
        const tag = document.items.find((item) => item.id === 'v0.1.0');
        tag.proof = { tag: 'v0.1.0', targetCommitSha: tagTargetSha };

        const errors = await validateReleaseStatus(document, { root });

        assert.ok(errors.some((error) => (
            error.includes('v0.1.0.proof.targetCommitSha')
            && error.includes('release-candidate commit')
        )), errors.join('\n'));
    } finally {
        await rm(root, { recursive: true, force: true });
    }
});

test('missing release artifacts and external approval cannot be marked verified', async () => {
    await withReleaseFixture(async (document, root) => {
        for (const id of ['class-diagrams', 'portfolio-pdf', 'demo-video', 'repository-visibility']) {
            const item = document.items.find((candidate) => candidate.id === id);
            item.status = 'verified';
            item.evidence = ['evidence.md'];
        }

        const errors = await validateReleaseStatus(document, { root });

        for (const id of ['class-diagrams', 'portfolio-pdf', 'demo-video', 'repository-visibility']) {
            assert.ok(errors.some((error) => error.includes(`${id}.status`)), id);
        }
    });
});

test('AWS resources that were never created are not described as cleaned up', async () => {
    await withReleaseFixture(async (document, root) => {
        const awsItem = document.items.find((item) => item.id === 'aws-external-test');
        awsItem.resourceState.cleanupVerified = true;

        const errors = await validateReleaseStatus(document, { root });

        assert.ok(errors.some((error) => error.includes('aws-external-test.resourceState.cleanupVerified')));
    });
});

test('Markdown link validation reports missing local targets with file context', async () => {
    const root = await mkdtemp(path.join(tmpdir(), 'dxa-portfolio-links-'));
    try {
        await mkdir(path.join(root, 'docs'), { recursive: true });
        await writeFile(path.join(root, 'docs', 'exists.md'), '# exists\n', 'utf8');
        await writeFile(
            path.join(root, 'README.md'),
            '[exists](docs/exists.md)\n[missing](docs/missing.md)\n[external](https://example.com)\n',
            'utf8'
        );

        const errors = await validateMarkdownLinks({ root, files: ['README.md'] });

        assert.deepEqual(errors, ['README.md: local link target is missing: docs/missing.md']);
    } finally {
        await rm(root, { recursive: true, force: true });
    }
});

test('Markdown link validation accepts balanced and escaped parentheses in local destinations', async () => {
    const root = await mkdtemp(path.join(tmpdir(), 'dxa-portfolio-balanced-links-'));
    try {
        await mkdir(path.join(root, 'docs'), { recursive: true });
        await writeFile(path.join(root, 'docs', 'guide(v2).md'), '# guide\n', 'utf8');
        await writeFile(
            path.join(root, 'README.md'),
            '[balanced](docs/guide(v2).md)\n[escaped](docs/guide\\(v2\\).md)\n',
            'utf8'
        );

        const errors = await validateMarkdownLinks({ root, files: ['README.md'] });

        assert.deepEqual(errors, []);
    } finally {
        await rm(root, { recursive: true, force: true });
    }
});

test('Markdown link validation resolves full, collapsed, and shortcut references', async () => {
    const root = await mkdtemp(path.join(tmpdir(), 'dxa-portfolio-reference-links-'));
    try {
        await writeFile(
            path.join(root, 'README.md'),
            [
                '[full]: docs/missing-full.md',
                '[collapsed]: docs/missing-collapsed.md',
                '[shortcut]: docs/missing-shortcut.md',
                '',
                '[read][full]',
                '[collapsed][]',
                '[shortcut]'
            ].join('\n'),
            'utf8'
        );

        const errors = await validateMarkdownLinks({ root, files: ['README.md'] });

        assert.deepEqual(errors, [
            'README.md: local link target is missing: docs/missing-full.md',
            'README.md: local link target is missing: docs/missing-collapsed.md',
            'README.md: local link target is missing: docs/missing-shortcut.md'
        ]);
    } finally {
        await rm(root, { recursive: true, force: true });
    }
});

test('Markdown duplicate references keep an earlier missing target over a later existing target', async () => {
    const root = await mkdtemp(path.join(tmpdir(), 'dxa-portfolio-reference-first-missing-'));
    try {
        await mkdir(path.join(root, 'docs'), { recursive: true });
        await writeFile(path.join(root, 'docs', 'existing.md'), '# existing\n', 'utf8');
        await writeFile(
            path.join(root, 'README.md'),
            '[target]: docs/missing.md\n[target]: docs/existing.md\n\n[target]\n',
            'utf8'
        );

        const errors = await validateMarkdownLinks({ root, files: ['README.md'] });

        assert.deepEqual(errors, ['README.md: local link target is missing: docs/missing.md']);
    } finally {
        await rm(root, { recursive: true, force: true });
    }
});

test('Markdown duplicate references keep an earlier existing target over a later missing target', async () => {
    const root = await mkdtemp(path.join(tmpdir(), 'dxa-portfolio-reference-first-existing-'));
    try {
        await mkdir(path.join(root, 'docs'), { recursive: true });
        await writeFile(path.join(root, 'docs', 'existing.md'), '# existing\n', 'utf8');
        await writeFile(
            path.join(root, 'README.md'),
            '[target]: docs/existing.md\n[target]: docs/missing.md\n\n[target]\n',
            'utf8'
        );

        const errors = await validateMarkdownLinks({ root, files: ['README.md'] });

        assert.deepEqual(errors, []);
    } finally {
        await rm(root, { recursive: true, force: true });
    }
});

test('Markdown link validation ignores link-shaped examples in inline and fenced code', async () => {
    const root = await mkdtemp(path.join(tmpdir(), 'dxa-portfolio-code-links-'));
    try {
        await writeFile(
            path.join(root, 'README.md'),
            '`[inline](docs/missing-inline.md)`\n\n```text\n[fenced](docs/missing-fenced.md)\n```\n',
            'utf8'
        );

        const errors = await validateMarkdownLinks({ root, files: ['README.md'] });

        assert.deepEqual(errors, []);
    } finally {
        await rm(root, { recursive: true, force: true });
    }
});

test('committed release status and README local links satisfy the release contract', async () => {
    const releaseStatus = await loadReleaseStatus(path.join(repositoryRoot, 'docs/portfolio/release-status.json'));
    const statusErrors = await validateReleaseStatus(releaseStatus, { root: repositoryRoot });
    const linkErrors = await validateMarkdownLinks({ root: repositoryRoot, files: ['README.md'] });

    assert.deepEqual(statusErrors, []);
    assert.deepEqual(linkErrors, []);
});
