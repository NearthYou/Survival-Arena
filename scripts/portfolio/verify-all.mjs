import { createHash } from 'node:crypto';
import { lstat, readFile, readdir, realpath, stat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

import { loadEvidence, validateEvidence } from './evidence.mjs';
import {
    renderClassDiagram,
    renderDiagram,
    renderIndex,
    validateClassDiagram,
    validateDiagram
} from './render-diagrams.mjs';

const RELEASE_STATUSES = new Set(['verified', 'partial', 'missing', 'blocked']);
const CANONICAL_CODE_BASIS_SHA = '884e5e70d68d9fcf9dfe5638d97e06623da154c2';
const REQUIRED_RELEASE_ITEM_IDS = [
    'historical-24-player-metrics',
    'historical-30-minute-soak',
    'licenses-assets-manifest',
    'lfs-object-availability',
    'current-head-builds',
    'warp-rtx-visual-artifacts',
    'architecture-diagrams',
    'class-diagrams',
    'portfolio-pdf',
    'demo-video',
    'aws-external-test',
    'repository-visibility',
    'v0.1.0'
];
const EXPECTED_DIAGRAM_SOURCE_FILES = [
    'game-start-sequence.json',
    'room-lifecycle.json',
    'snapshot-data-flow.json',
    'system-architecture.json'
];
const REQUIRED_CLASS_DIAGRAM_PATHS = [
    'docs/diagrams/class/engine.json',
    'docs/diagrams/class/network.json',
    'docs/diagrams/class/engine.html',
    'docs/diagrams/class/network.html'
];
const CLASS_GENERATION_MANIFEST_PATH = 'docs/diagrams/class/manifest.json';
const CLASS_GENERATION_EVIDENCE_DIRECTORY = 'docs/diagrams/class/evidence';
const REQUIRED_CLASS_GENERATION_SNAPSHOTS = [
    `${CLASS_GENERATION_EVIDENCE_DIRECTORY}/cmake-cache.json`,
    `${CLASS_GENERATION_EVIDENCE_DIRECTORY}/compile-commands.json`,
    `${CLASS_GENERATION_EVIDENCE_DIRECTORY}/tool-identities.json`,
    `${CLASS_GENERATION_EVIDENCE_DIRECTORY}/vcpkg-metadata.json`,
    `${CLASS_GENERATION_EVIDENCE_DIRECTORY}/vcpkg-status.json`
];
const REQUIRED_RUNTIME_LFS_PATHS = [
    'assets/runtime/characters/cyber-runner.dxam',
    'assets/runtime/environment/colormap.dds',
    'assets/runtime/environment/prototype-floor.dxam'
];
const MARKDOWN_SCAN_EXCLUSIONS = new Set([
    '.git',
    '.superpowers',
    '.vcpkg',
    'node_modules',
    'out'
]);

function toPosixPath(filePath) {
    return filePath.split(path.sep).join('/');
}

function resolveRepositoryPath(root, relativePath) {
    if (typeof relativePath !== 'string' || relativePath.length === 0) {
        return { error: 'must be a non-empty repository-relative path' };
    }

    const resolvedPath = path.resolve(root, relativePath);
    if (resolvedPath === root || !resolvedPath.startsWith(`${root}${path.sep}`)) {
        return { error: `escapes repository root: ${relativePath}` };
    }

    return { resolvedPath };
}

function isWithinRealRoot(realRoot, realTarget) {
    const relative = path.relative(realRoot, realTarget);
    return relative === ''
        || (relative !== '..'
            && !relative.startsWith(`..${path.sep}`)
            && !path.isAbsolute(relative));
}

async function inspectResolvedRepositoryPath(root, realRoot, resolvedPath) {
    try {
        const realTarget = await realpath(resolvedPath);
        if (!isWithinRealRoot(realRoot, realTarget)) {
            return { status: 'outside-real' };
        }
        return { status: 'found', realTarget, stats: await stat(realTarget) };
    } catch (error) {
        if (error.code !== 'ENOENT') {
            return { status: 'unreadable', error };
        }

        let ancestor = path.dirname(resolvedPath);
        while (ancestor === root || ancestor.startsWith(`${root}${path.sep}`)) {
            try {
                const realAncestor = await realpath(ancestor);
                if (!isWithinRealRoot(realRoot, realAncestor)) {
                    return { status: 'outside-real' };
                }
                break;
            } catch (ancestorError) {
                if (ancestorError.code !== 'ENOENT' || ancestor === root) {
                    return { status: 'unreadable', error: ancestorError };
                }
                ancestor = path.dirname(ancestor);
            }
        }
        return { status: 'missing' };
    }
}

async function inspectRepositoryPath(root, realRoot, relativePath) {
    const pathResult = resolveRepositoryPath(root, relativePath);
    if (pathResult.error) {
        return { status: 'invalid', error: pathResult.error };
    }
    return inspectResolvedRepositoryPath(root, realRoot, pathResult.resolvedPath);
}

export async function loadReleaseStatus(statusPath) {
    return JSON.parse(await readFile(statusPath, 'utf8'));
}

function hasDatedCheck(value) {
    return typeof value === 'string'
        && value.trim().length > 0
        && !Number.isNaN(Date.parse(value));
}

function normalizedTextSha256(content) {
    return sha256(String(content).replaceAll('\r\n', '\n'));
}

function selectedTranslationUnit(relativePath) {
    return /^engine\/src\/.+\.cpp$/u.test(relativePath)
        || /^apps\/(?:game_client|game_server|lobby_server)\/src\/.+\.cpp$/u.test(relativePath)
        || relativePath === 'tests/engine_resource_pool_test.cpp';
}

function compareOrdinal(left, right) {
    return left < right ? -1 : (left > right ? 1 : 0);
}

function validateSortedHashEntries(entries, field) {
    const errors = [];
    if (!Array.isArray(entries) || entries.length === 0) {
        return [`${field}: must be a nonempty array`];
    }
    const paths = entries.map((entry) => entry?.path);
    const sortedPaths = [...paths].sort((left, right) => compareOrdinal(String(left), String(right)));
    if (paths.some((entryPath, index) => entryPath !== sortedPaths[index])) {
        errors.push(`${field}: paths must be sorted in ordinal repository order`);
    }
    if (new Set(paths).size !== paths.length) {
        errors.push(`${field}: paths must be unique`);
    }
    for (const [index, entry] of entries.entries()) {
        if (typeof entry?.path !== 'string'
            || !/^(?:vcpkg\/info\/.+\.list|x64-windows\/share\/.+\/vcpkg_abi_info\.txt)$/u.test(entry.path)) {
            errors.push(`${field}[${index}].path: must be installed list or ABI metadata`);
        }
        if (!/^[0-9a-f]{64}$/u.test(entry?.sha256 ?? '')) {
            errors.push(`${field}[${index}].sha256: must be a lowercase SHA-256`);
        }
    }
    return errors;
}

function equalJson(left, right) {
    return JSON.stringify(left) === JSON.stringify(right);
}

function privacySensitiveSnapshotPattern(content) {
    return [
        /[A-Z]:[\\/]{1,2}(?:Users|Documents and Settings)[\\/]{1,2}[^\\/\s"'<>:]+/iu,
        /(?:^|[\s"'=:(])\/(?:home|Users)\/[^/\s"'<>]+(?:\/|$)/iu,
        /AppData|[\\/]Temp[\\/]|\.worktrees/iu,
        /"(?:generatedAt|timestamp|createdAt)"\s*:/iu,
        /(?:^|\\n|\n)\s*(?:SITE|COMPUTERNAME|HOSTNAME|HOST)(?:(?::[^=\\\r\n]*)?=|:\s*)/iu,
        /"(?:site|computerName|hostName|hostname|host)"\s*:/iu,
        /(?:\/D|-D)(?:SITE|COMPUTERNAME|HOSTNAME|HOST)=/iu
    ].find((pattern) => pattern.test(content));
}

function parseSnapshotCompileCommand(command) {
    if (typeof command !== 'string') {
        return undefined;
    }
    const match = /^(\S+)(?=\s|$)/u.exec(command);
    if (!match) {
        return undefined;
    }
    return {
        executable: match[1],
        arguments: command.slice(match[0].length)
    };
}

function parseCmakeCacheSnapshot(content) {
    const entries = new Map();
    const duplicates = new Set();
    for (const line of content.split('\n')) {
        const match = /^([^#/:][^:=]*):[^=]*=(.*)$/u.exec(line);
        if (!match) {
            continue;
        }
        if (entries.has(match[1])) {
            duplicates.add(match[1]);
        }
        entries.set(match[1], match[2]);
    }
    return { entries, duplicates };
}

function parseVcpkgStatusSnapshot(content) {
    return content
        .trim()
        .split(/\n\n+/u)
        .map((paragraph) => {
            const fields = {};
            for (const line of paragraph.split('\n')) {
                const separator = line.indexOf(': ');
                if (separator > 0) {
                    fields[line.slice(0, separator)] = line.slice(separator + 2);
                }
            }
            return fields;
        });
}

function snapshotNormalizationMatches(actual, expected) {
    return actual
        && typeof actual === 'object'
        && !Array.isArray(actual)
        && equalJson(actual, expected);
}

function validateCompilerIdentity(actual, expected, field) {
    const errors = [];
    const formats = {
        volumeSerialNumber: /^0x[0-9a-f]{8}$/u,
        fileId: /^0x[0-9a-f]{32}$/u,
        sha256: /^[0-9a-f]{64}$/u
    };
    for (const [name, pattern] of Object.entries(formats)) {
        if (!pattern.test(actual?.[name] ?? '')) {
            errors.push(`${field}.${name}: invalid Windows compiler identity`);
        } else if (actual[name] !== expected?.[name]) {
            errors.push(`${field}.${name}: does not match the CMake compiler identity`);
        }
    }
    return errors;
}

async function loadClassGenerationSnapshots(manifest, options, field) {
    const errors = [];
    const snapshots = new Map();
    const snapshotEntries = manifest.snapshots;
    if (!Array.isArray(snapshotEntries)
        || snapshotEntries.length !== REQUIRED_CLASS_GENERATION_SNAPSHOTS.length
        || snapshotEntries.some((entry, index) => entry?.path !== REQUIRED_CLASS_GENERATION_SNAPSHOTS[index])) {
        errors.push(`${field}.snapshots: must enumerate the exact sorted generation evidence snapshot set`);
    }

    const manifestSnapshotByPath = new Map();
    if (Array.isArray(snapshotEntries)) {
        for (const [index, entry] of snapshotEntries.entries()) {
            if (typeof entry?.path !== 'string' || !/^[0-9a-f]{64}$/u.test(entry?.sha256 ?? '')) {
                errors.push(`${field}.snapshots[${index}]: must contain a path and lowercase SHA-256`);
                continue;
            }
            if (manifestSnapshotByPath.has(entry.path)) {
                errors.push(`${field}.snapshots[${index}].path: must be unique`);
            }
            manifestSnapshotByPath.set(entry.path, entry);
        }
    }

    const evidenceDirectory = path.join(options.root, CLASS_GENERATION_EVIDENCE_DIRECTORY);
    try {
        const directoryEntries = await readdir(evidenceDirectory, { withFileTypes: true });
        const actualPaths = directoryEntries
            .map((entry) => `${CLASS_GENERATION_EVIDENCE_DIRECTORY}/${entry.name}`)
            .sort(compareOrdinal);
        for (const requiredPath of REQUIRED_CLASS_GENERATION_SNAPSHOTS) {
            if (!actualPaths.includes(requiredPath)) {
                errors.push(`${field}.snapshots: ${requiredPath} is missing`);
            }
        }
        for (const actualPath of actualPaths) {
            if (!REQUIRED_CLASS_GENERATION_SNAPSHOTS.includes(actualPath)) {
                errors.push(`${field}.snapshots: ${actualPath} is extra`);
            }
        }
        for (const entry of directoryEntries) {
            const relativePath = `${CLASS_GENERATION_EVIDENCE_DIRECTORY}/${entry.name}`;
            if (REQUIRED_CLASS_GENERATION_SNAPSHOTS.includes(relativePath) && !entry.isFile()) {
                errors.push(`${field}.snapshots: ${relativePath} must be a regular file`);
            }
        }
    } catch (error) {
        if (error.code === 'ENOENT') {
            for (const requiredPath of REQUIRED_CLASS_GENERATION_SNAPSHOTS) {
                errors.push(`${field}.snapshots: ${requiredPath} is missing`);
            }
        } else {
            errors.push(`${field}.snapshots: cannot enumerate evidence directory: ${error.message}`);
        }
    }

    let realRoot;
    try {
        realRoot = await realpath(options.root);
    } catch (error) {
        errors.push(`${field}.snapshots: cannot resolve repository root: ${error.message}`);
        return { errors, snapshots };
    }
    for (const relativePath of REQUIRED_CLASS_GENERATION_SNAPSHOTS) {
        try {
            const inspected = await inspectRepositoryPath(options.root, realRoot, relativePath);
            if (inspected.status !== 'found' || !inspected.stats.isFile()) {
                continue;
            }
            const bytes = await readFile(path.join(options.root, relativePath));
            const content = bytes.toString('utf8');
            snapshots.set(relativePath, { bytes, content });
            const manifestEntry = manifestSnapshotByPath.get(relativePath);
            if (!manifestEntry || sha256(bytes) !== manifestEntry.sha256) {
                errors.push(`${field}.snapshots: ${relativePath} hash does not match the manifest`);
            }
            const privacyPattern = privacySensitiveSnapshotPattern(content);
            if (privacyPattern) {
                errors.push(`${field}.snapshots: ${relativePath} is not privacy-safe`);
            }
            if (content.includes('\r')) {
                errors.push(`${field}.snapshots: ${relativePath} must use LF line endings`);
            }
        } catch (error) {
            if (error.code !== 'ENOENT') {
                errors.push(`${field}.snapshots: cannot read ${relativePath}: ${error.message}`);
            }
        }
    }
    return { errors, snapshots };
}

function validateCompileCommandsSnapshot(snapshot, manifest, field) {
    const errors = [];
    const snapshotField = `${field}.snapshots.compile-commands.json`;
    let document;
    try {
        document = JSON.parse(snapshot.content);
    } catch (error) {
        return [`${snapshotField}: invalid JSON: ${error.message}`];
    }
    if (document.schemaVersion !== 1) {
        errors.push(`${snapshotField}.schemaVersion: must be 1`);
    }
    if (!snapshotNormalizationMatches(document.normalization, {
        encoding: 'UTF-8',
        lineEndings: 'LF',
        pathSeparator: '/',
        commandCompilerToken: '${MSVC_COMPILER}'
    })) {
        errors.push(`${snapshotField}.normalization: unexpected normalization contract`);
    }
    const entries = document.entries;
    const expectedTotal = manifest.compilation?.compileCommands?.totalTranslationUnits;
    if (!Array.isArray(entries) || entries.length !== expectedTotal || entries.length !== 172) {
        errors.push(`${snapshotField}.entries: must contain all 172 compile database entries`);
        return errors;
    }
    const files = entries.map((entry) => entry?.file);
    if (new Set(files).size !== files.length
        || files.some((entryPath, index) => entryPath !== [...files].sort(compareOrdinal)[index])) {
        errors.push(`${snapshotField}.entries: source paths must be sorted and unique`);
    }
    for (const [index, entry] of entries.entries()) {
        const entryField = `${snapshotField}.entries[${index}]`;
        if (typeof entry?.file !== 'string'
            || !/^(?:apps|engine|protocol|simulation|tests)\/.+\.cpp$/u.test(entry.file)) {
            errors.push(`${entryField}.file: must be a normalized repository C++ source path`);
        }
        if (entry?.directory !== '${BUILD_ROOT}') {
            errors.push(`${entryField}.directory: must be the normalized build root`);
        }
        const parsedCommand = parseSnapshotCompileCommand(entry?.command);
        if (parsedCommand?.executable !== '${MSVC_COMPILER}'
            || !/^\s+\S/u.test(parsedCommand.arguments)) {
            errors.push(`${entryField}.command: compiler token must be the exact first executable`);
        }
        if (typeof entry?.command !== 'string'
            || !entry.command.includes(`\${REPOSITORY_ROOT}/${entry?.file}`)) {
            errors.push(`${entryField}.command: must compile its normalized repository source`);
        }
        if (typeof entry?.command === 'string' && /[A-Z]:[\\/]/iu.test(entry.command)) {
            errors.push(`${entryField}.command: must not retain an absolute build or dependency root`);
        }
    }
    const selectedFiles = files.filter((entryPath) => selectedTranslationUnit(entryPath));
    const manifestSelected = manifest.compilation?.compileCommands?.selectedPaths;
    if (!equalJson(selectedFiles, manifestSelected)) {
        errors.push(`${snapshotField}.entries: selected TU set does not match the generation manifest`);
    }
    return errors;
}

function validateCmakeCacheSnapshot(snapshot, manifest, field) {
    const errors = [];
    const snapshotField = `${field}.snapshots.cmake-cache.json`;
    let document;
    try {
        document = JSON.parse(snapshot.content);
    } catch (error) {
        return [`${snapshotField}: invalid JSON: ${error.message}`];
    }
    if (document.schemaVersion !== 1
        || document.format !== 'cmake-cache'
        || !snapshotNormalizationMatches(document.normalization, {
            encoding: 'UTF-8',
            lineEndings: 'LF',
            pathSeparator: '/',
            hostIdentifiers: 'omitted'
        })
        || typeof document.content !== 'string') {
        errors.push(`${snapshotField}: unexpected schema or normalization contract`);
        return errors;
    }
    const { entries, duplicates } = parseCmakeCacheSnapshot(document.content);
    if (duplicates.size > 0) {
        errors.push(`${snapshotField}: required cache keys must be unique`);
    }
    const hostSpecificKeys = [...entries.keys()].filter((name) => (
        /^(?:SITE|COMPUTERNAME|HOSTNAME|HOST)(?:-ADVANCED)?$/iu.test(name)
    ));
    if (hostSpecificKeys.length > 0) {
        errors.push(`${snapshotField}: host-specific cache entries must be omitted`);
    }
    const vcpkgRoot = typeof manifest.compilation?.cmakeCache?.toolchain === 'string'
        ? manifest.compilation.cmakeCache.toolchain.replace(/\/scripts\/buildsystems\/vcpkg\.cmake$/iu, '')
        : undefined;
    const expected = {
        CMAKE_CXX_COMPILER: '${MSVC_COMPILER}',
        CMAKE_MAKE_PROGRAM: '${NINJA}',
        CMAKE_COMMAND: '${CMAKE}',
        CMAKE_TOOLCHAIN_FILE: '${VCPKG_TOOLCHAIN}',
        VCPKG_INSTALLED_DIR: '${VCPKG_INSTALLED_ROOT}',
        CMAKE_GENERATOR: 'Ninja',
        CMAKE_HOME_DIRECTORY: '${REPOSITORY_ROOT}',
        CMAKE_CACHEFILE_DIR: '${BUILD_ROOT}',
        VCPKG_MANIFEST_DIR: '${REPOSITORY_ROOT}',
        VCPKG_TARGET_TRIPLET: 'x64-windows',
        _VCPKG_INSTALLED_DIR: '${VCPKG_INSTALLED_ROOT}',
        Z_VCPKG_ROOT_DIR: vcpkgRoot,
        DXA_POWERSHELL_EXECUTABLE: '${POWERSHELL}',
        Z_VCPKG_PWSH_PATH: '${POWERSHELL}',
        Z_VCPKG_POWERSHELL_PATH: '${POWERSHELL}'
    };
    for (const [name, expectedValue] of Object.entries(expected)) {
        if (entries.get(name) !== expectedValue) {
            errors.push(`${snapshotField}.${name}: must be ${expectedValue}`);
        }
    }
    if (manifest.compilation?.cmakeCache?.generator !== entries.get('CMAKE_GENERATOR')) {
        errors.push(`${snapshotField}.CMAKE_GENERATOR: does not match generation manifest`);
    }
    return errors;
}

function validateToolIdentitiesSnapshot(snapshot, manifest, field) {
    const errors = [];
    const snapshotField = `${field}.snapshots.tool-identities.json`;
    let document;
    try {
        document = JSON.parse(snapshot.content);
    } catch (error) {
        return [`${snapshotField}: invalid JSON: ${error.message}`];
    }
    if (document.schemaVersion !== 1) {
        errors.push(`${snapshotField}.schemaVersion: must be 1`);
    }
    const cache = manifest.compilation?.cmakeCache;
    const expectedRootTokens = {
        '${REPOSITORY_ROOT}': '.',
        '${BUILD_ROOT}': 'out/build/portfolio-clang-uml',
        '${VCPKG_INSTALLED_ROOT}': 'out/build/portfolio-clang-uml/vcpkg_installed',
        '${MSVC_COMPILER}': cache?.compiler,
        '${CMAKE}': 'C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe',
        '${NINJA}': cache?.makeProgram,
        '${VCPKG_TOOLCHAIN}': cache?.toolchain,
        '${POWERSHELL}': 'external/powershell'
    };
    if (!snapshotNormalizationMatches(document.normalization, {
        encoding: 'UTF-8',
        lineEndings: 'LF',
        pathSeparator: '/',
        rootTokens: expectedRootTokens
    })) {
        errors.push(`${snapshotField}.normalization: root-token map does not match manifest tool and build roots`);
    }
    if (!equalJson(document.clangUml, manifest.tooling?.clangUml)) {
        errors.push(`${snapshotField}.clangUml: does not match manifest tool identity`);
    }
    for (const name of ['cmakeVersion', 'ninjaVersion', 'msvcVersion']) {
        if (document[name] !== manifest.tooling?.[name]) {
            errors.push(`${snapshotField}.${name}: does not match manifest tool identity`);
        }
    }
    errors.push(...validateCompilerIdentity(
        document.compilerIdentity,
        cache?.compilerIdentity,
        `${snapshotField}.compilerIdentity`
    ));
    const expectedSourceDigests = {
        compileCommandsRawSha256: manifest.compilation?.compileCommands?.sha256,
        cmakeCacheRawSha256: cache?.sha256,
        vcpkgStatusRawSha256: manifest.dependencies?.installed?.status?.sha256
    };
    if (!equalJson(document.sourceDigests, expectedSourceDigests)) {
        errors.push(`${snapshotField}.sourceDigests: raw generation inputs do not match the manifest`);
    }
    return errors;
}

function validateVcpkgSnapshots(statusSnapshot, metadataSnapshot, manifest, field) {
    const errors = [];
    const statusField = `${field}.snapshots.vcpkg-status.json`;
    const metadataField = `${field}.snapshots.vcpkg-metadata.json`;
    let statusDocument;
    try {
        statusDocument = JSON.parse(statusSnapshot.content);
    } catch (error) {
        return [`${statusField}: invalid JSON: ${error.message}`];
    }
    if (statusDocument.schemaVersion !== 1
        || statusDocument.format !== 'vcpkg-status'
        || !snapshotNormalizationMatches(statusDocument.normalization, {
            encoding: 'UTF-8',
            lineEndings: 'LF'
        })
        || typeof statusDocument.content !== 'string'
        || statusDocument.content.includes('\r')) {
        errors.push(`${statusField}: unexpected schema or normalization contract`);
        return errors;
    }
    const paragraphs = parseVcpkgStatusSnapshot(statusDocument.content);
    const installedPackages = paragraphs.filter((entry) => entry.Status === 'install ok installed'
        && entry.Architecture === 'x64-windows'
        && /^[0-9a-f]{64}$/u.test(entry.Abi ?? ''));
    const packageNames = installedPackages.map((entry) => entry.Package);
    if (installedPackages.length !== 71 || new Set(packageNames).size !== installedPackages.length) {
        errors.push(`${statusField}: must contain exactly 71 unique installed base-package records with ABI hashes`);
    }
    if (manifest.dependencies?.installed?.status?.path !== 'vcpkg/status'
        || manifest.dependencies?.installed?.status?.sha256 !== sha256(statusDocument.content)) {
        errors.push(`${statusField}: bytes do not match installed status provenance`);
    }

    let metadata;
    try {
        metadata = JSON.parse(metadataSnapshot.content);
    } catch (error) {
        return [...errors, `${metadataField}: invalid JSON: ${error.message}`];
    }
    if (metadata.schemaVersion !== 1
        || !snapshotNormalizationMatches(metadata.normalization, {
            encoding: 'UTF-8',
            lineEndings: 'LF'
        })) {
        errors.push(`${metadataField}: unexpected schema or normalization contract`);
    }
    const files = metadata.files;
    if (!Array.isArray(files)) {
        errors.push(`${metadataField}.files: must be an array`);
        return errors;
    }
    const paths = files.map((entry) => entry?.path);
    if (new Set(paths).size !== paths.length
        || paths.some((entryPath, index) => entryPath !== [...paths].sort(compareOrdinal)[index])) {
        errors.push(`${metadataField}.files: paths must be sorted and unique`);
    }
    for (const [index, entry] of files.entries()) {
        if (typeof entry?.content !== 'string'
            || entry.content.includes('\r')
            || privacySensitiveSnapshotPattern(entry.content)
            || sha256(entry.content) !== entry?.sha256) {
            errors.push(`${metadataField}.files[${index}]: content and SHA-256 must be normalized and privacy-safe`);
        }
    }
    const listFiles = files.filter((entry) => /^vcpkg\/info\/.+\.list$/u.test(entry?.path ?? ''));
    const abiFiles = files.filter((entry) => /^x64-windows\/share\/.+\/vcpkg_abi_info\.txt$/u.test(entry?.path ?? ''));
    if (listFiles.length !== 71) {
        errors.push(`${metadataField}: expected 71 installed list snapshots`);
    }
    if (abiFiles.length !== 71) {
        errors.push(`${metadataField}: expected 71 ABI metadata snapshots`);
    }
    const matchedLists = new Set();
    const matchedAbis = new Set();
    for (const installedPackage of installedPackages) {
        const packageName = installedPackage.Package;
        const lists = listFiles.filter((entry) => entry.path.startsWith(`vcpkg/info/${packageName}_`)
            && entry.path.endsWith('_x64-windows.list'));
        const abiPath = `x64-windows/share/${packageName}/vcpkg_abi_info.txt`;
        const abis = abiFiles.filter((entry) => entry.path === abiPath);
        if (lists.length !== 1) {
            errors.push(`${metadataField}: missing installed list for ${packageName}`);
        } else {
            matchedLists.add(lists[0].path);
            if (!lists[0].content.includes(`${abiPath}\n`)) {
                errors.push(`${metadataField}: installed list for ${packageName} omits its ABI metadata path`);
            }
        }
        if (abis.length !== 1) {
            errors.push(`${metadataField}: missing ABI metadata for ${packageName}`);
        } else {
            matchedAbis.add(abis[0].path);
            if (abis[0].sha256 !== installedPackage.Abi) {
                errors.push(`${metadataField}: ABI metadata hash does not match status for ${packageName}`);
            }
        }
    }
    for (const entry of listFiles) {
        if (!matchedLists.has(entry.path)) {
            errors.push(`${metadataField}: extra installed list ${entry.path}`);
        }
    }
    for (const entry of abiFiles) {
        if (!matchedAbis.has(entry.path)) {
            errors.push(`${metadataField}: extra ABI metadata ${entry.path}`);
        }
    }

    const installed = manifest.dependencies?.installed;
    const snapshotEntries = files.map((entry) => ({ path: entry.path, sha256: entry.sha256 }));
    if (!equalJson(installed?.metadataFiles, snapshotEntries)) {
        errors.push(`${metadataField}: file hashes do not match manifest installed metadata`);
    }
    if (installed?.metadataFileCount !== files.length) {
        errors.push(`${metadataField}: file count does not match manifest installed metadata`);
    }
    const setMaterial = snapshotEntries.map((entry) => `${entry.path}\0${entry.sha256}\n`).join('');
    if (installed?.metadataSetSha256 !== sha256(setMaterial)) {
        errors.push(`${metadataField}: set hash does not match actual snapshot files`);
    }
    return errors;
}

async function validateClassGenerationSnapshots(manifest, options, field) {
    const loaded = await loadClassGenerationSnapshots(manifest, options, field);
    const errors = [...loaded.errors];
    const get = (name) => loaded.snapshots.get(`${CLASS_GENERATION_EVIDENCE_DIRECTORY}/${name}`);
    const compile = get('compile-commands.json');
    const cache = get('cmake-cache.json');
    const tools = get('tool-identities.json');
    const metadata = get('vcpkg-metadata.json');
    const status = get('vcpkg-status.json');
    if (compile) {
        errors.push(...validateCompileCommandsSnapshot(compile, manifest, field));
    }
    if (cache) {
        errors.push(...validateCmakeCacheSnapshot(cache, manifest, field));
    }
    if (tools) {
        errors.push(...validateToolIdentitiesSnapshot(tools, manifest, field));
    }
    if (status && metadata) {
        errors.push(...validateVcpkgSnapshots(status, metadata, manifest, field));
    }
    return { errors, compile };
}

async function validateClassGenerationManifest(manifest, options) {
    const errors = [];
    const field = 'class-diagrams.proof.manifest';
    if (!manifest || typeof manifest !== 'object' || Array.isArray(manifest)) {
        return [`${field}: must be an object`];
    }
    if (manifest.schemaVersion !== 2) {
        errors.push(`${field}.schemaVersion: must be 2`);
    }
    if (manifest.basis?.commitSha !== CANONICAL_CODE_BASIS_SHA) {
        errors.push(`${field}.basis.commitSha: must match the canonical code basis SHA`);
    }
    if (manifest.basis?.treeSha !== 'a3d167d7ddb3fadfe5ce9a2dfea6f5a58b170890') {
        errors.push(`${field}.basis.treeSha: must match the canonical code basis tree`);
    }
    if (options.verifyClassBasisCommit ?? true) {
        const treeResult = spawnSync(
            'git',
            ['show', '-s', '--format=%T', CANONICAL_CODE_BASIS_SHA],
            { cwd: options.root, encoding: 'utf8' }
        );
        if (treeResult.status !== 0 || treeResult.stdout.trim() !== manifest.basis?.treeSha) {
            errors.push(`${field}.basis.treeSha: current Git object does not match manifest`);
        }
    }

    const clangUml = manifest.tooling?.clangUml;
    if (clangUml?.version !== '0.6.3') {
        errors.push(`${field}.tooling.clangUml.version: must be 0.6.3`);
    }
    if (typeof clangUml?.fullVersion !== 'string'
        || !/^clang-uml 0\.6\.3\n/iu.test(clangUml.fullVersion)
        || !clangUml.fullVersion.includes(`Using LLVM/Clang libraries version: ${clangUml.llvmIdentity}`)) {
        errors.push(`${field}.tooling.clangUml.fullVersion: must contain the exact clang-uml and LLVM identity`);
    }
    if (typeof clangUml?.llvmIdentity !== 'string'
        || !/^clang version 22\.1\.8\b/u.test(clangUml.llvmIdentity)) {
        errors.push(`${field}.tooling.clangUml.llvmIdentity: must identify clang 22.1.8`);
    }
    for (const [inputName, requiredPath] of [
        ['config', '.clang-uml'],
        ['generator', 'scripts/portfolio/generate-class-diagrams.ps1']
    ]) {
        const input = manifest.tooling?.[inputName];
        if (input?.path !== requiredPath) {
            errors.push(`${field}.tooling.${inputName}.path: must be ${requiredPath}`);
            continue;
        }
        try {
            const content = await readFile(path.join(options.root, requiredPath), 'utf8');
            if (normalizedTextSha256(content) !== input.sha256) {
                errors.push(`${field}.tooling.${inputName}.sha256: current file does not match generation manifest`);
            }
        } catch (error) {
            errors.push(`${field}.tooling.${inputName}.path: cannot read current input: ${error.message}`);
        }
    }
    if (!/^cmake version 3\.31\.6/iu.test(manifest.tooling?.cmakeVersion ?? '')) {
        errors.push(`${field}.tooling.cmakeVersion: must identify CMake 3.31.6`);
    }
    if (manifest.tooling?.ninjaVersion !== '1.12.1') {
        errors.push(`${field}.tooling.ninjaVersion: must be 1.12.1`);
    }
    if (!/^19\.44\.35228/u.test(manifest.tooling?.msvcVersion ?? '')) {
        errors.push(`${field}.tooling.msvcVersion: must identify MSVC 19.44.35228`);
    }

    const compileCommands = manifest.compilation?.compileCommands;
    if (compileCommands?.path !== 'out/build/portfolio-clang-uml/compile_commands.json') {
        errors.push(`${field}.compilation.compileCommands.path: unexpected generation path`);
    }
    if (!/^[0-9a-f]{64}$/u.test(compileCommands?.sha256 ?? '')) {
        errors.push(`${field}.compilation.compileCommands.sha256: must be a lowercase SHA-256`);
    }
    if (compileCommands?.totalTranslationUnits !== 172 || compileCommands?.selectedTranslationUnits !== 44) {
        errors.push(`${field}.compilation.compileCommands: expected 172 total and 44 selected translation units`);
    }
    const selectedPaths = compileCommands?.selectedPaths;
    if (!Array.isArray(selectedPaths)
        || selectedPaths.length !== compileCommands?.selectedTranslationUnits
        || new Set(selectedPaths).size !== selectedPaths.length
        || selectedPaths.some((entryPath, index) => entryPath !== [...selectedPaths].sort(compareOrdinal)[index])
        || selectedPaths.some((entryPath) => !selectedTranslationUnit(entryPath))) {
        errors.push(`${field}.compilation.compileCommands.selectedPaths: must be the sorted unique selected TU set`);
    } else {
        const selectedHash = sha256(`${selectedPaths.join('\n')}\n`);
        if (compileCommands.selectedPathsSha256 !== selectedHash) {
            errors.push(`${field}.compilation.compileCommands.selectedPathsSha256: selected TU set hash mismatch`);
        }
        if (options.verifyClassBasisCommit ?? true) {
            const expectedResult = spawnSync(
                'git',
                [
                    'ls-tree', '-r', '--name-only', CANONICAL_CODE_BASIS_SHA, '--',
                    'engine/src', 'apps/game_client/src', 'apps/game_server/src',
                    'apps/lobby_server/src', 'tests/engine_resource_pool_test.cpp'
                ],
                { cwd: options.root, encoding: 'utf8' }
            );
            const expectedPaths = expectedResult.stdout
                .split(/\r?\n/u)
                .filter((entryPath) => entryPath && selectedTranslationUnit(entryPath))
                .sort(compareOrdinal);
            if (expectedResult.status !== 0 || JSON.stringify(expectedPaths) !== JSON.stringify(selectedPaths)) {
                errors.push(`${field}.compilation.compileCommands.selectedPaths: basis TU set mismatch`);
            }
        }
    }

    const cache = manifest.compilation?.cmakeCache;
    const normalizedCacheFields = {
        path: 'out/build/portfolio-clang-uml/CMakeCache.txt',
        generator: 'Ninja',
        homeDirectory: '.',
        buildDirectory: 'out/build/portfolio-clang-uml',
        vcpkgInstalled: 'out/build/portfolio-clang-uml/vcpkg_installed'
    };
    for (const [cacheField, expected] of Object.entries(normalizedCacheFields)) {
        if (cache?.[cacheField] !== expected) {
            errors.push(`${field}.compilation.cmakeCache.${cacheField}: must be ${expected}`);
        }
    }
    if (!/^[0-9a-f]{64}$/u.test(cache?.sha256 ?? '')) {
        errors.push(`${field}.compilation.cmakeCache.sha256: must be a lowercase SHA-256`);
    }
    for (const [cacheField, filePattern] of [
        ['compiler', /\/cl\.exe$/iu],
        ['makeProgram', /\/ninja\.exe$/iu],
        ['toolchain', /\/vcpkg\.cmake$/iu]
    ]) {
        if (typeof cache?.[cacheField] !== 'string'
            || !/^[A-Za-z]:\//u.test(cache[cacheField])
            || !filePattern.test(cache[cacheField])) {
            errors.push(`${field}.compilation.cmakeCache.${cacheField}: invalid normalized tool path`);
        }
    }
    errors.push(...validateCompilerIdentity(
        cache?.compilerIdentity,
        cache?.compilerIdentity,
        `${field}.compilation.cmakeCache.compilerIdentity`
    ));

    const vcpkgManifest = manifest.dependencies?.vcpkgManifest;
    if (vcpkgManifest?.path !== 'vcpkg.json') {
        errors.push(`${field}.dependencies.vcpkgManifest.path: must be vcpkg.json`);
    } else {
        try {
            const content = await readFile(path.join(options.root, 'vcpkg.json'));
            if (sha256(content) !== vcpkgManifest.sha256) {
                errors.push(`${field}.dependencies.vcpkgManifest.sha256: current vcpkg.json mismatch`);
            }
        } catch (error) {
            errors.push(`${field}.dependencies.vcpkgManifest.path: cannot read vcpkg.json: ${error.message}`);
        }
    }
    const configurationPath = path.join(options.root, 'vcpkg-configuration.json');
    try {
        const content = await readFile(configurationPath);
        if (manifest.dependencies?.vcpkgConfiguration?.path !== 'vcpkg-configuration.json'
            || manifest.dependencies.vcpkgConfiguration.sha256 !== sha256(content)) {
            errors.push(`${field}.dependencies.vcpkgConfiguration: current file mismatch`);
        }
    } catch (error) {
        if (error.code !== 'ENOENT' || manifest.dependencies?.vcpkgConfiguration !== null) {
            errors.push(`${field}.dependencies.vcpkgConfiguration: must be null when file is absent`);
        }
    }
    const installed = manifest.dependencies?.installed;
    if (installed?.status?.path !== 'vcpkg/status'
        || !/^[0-9a-f]{64}$/u.test(installed?.status?.sha256 ?? '')) {
        errors.push(`${field}.dependencies.installed.status: invalid status provenance`);
    }
    const metadataErrors = validateSortedHashEntries(
        installed?.metadataFiles,
        `${field}.dependencies.installed.metadataFiles`
    );
    errors.push(...metadataErrors);
    if (Array.isArray(installed?.metadataFiles)) {
        if (installed.metadataFileCount !== installed.metadataFiles.length) {
            errors.push(`${field}.dependencies.installed.metadataFileCount: does not match metadata files`);
        }
        const material = installed.metadataFiles.map((entry) => `${entry.path}\0${entry.sha256}\n`).join('');
        if (installed.metadataSetSha256 !== sha256(material)) {
            errors.push(`${field}.dependencies.installed.metadataSetSha256: package metadata set hash mismatch`);
        }
    }

    const snapshotValidation = await validateClassGenerationSnapshots(manifest, options, field);
    errors.push(...snapshotValidation.errors);
    if ((options.verifyClassBasisCommit ?? true) && snapshotValidation.compile) {
        try {
            const compileSnapshot = JSON.parse(snapshotValidation.compile.content);
            const basisFilesResult = spawnSync(
                'git',
                [
                    'ls-tree', '-r', '--name-only', CANONICAL_CODE_BASIS_SHA, '--',
                    'apps', 'engine', 'protocol', 'simulation', 'tests'
                ],
                { cwd: options.root, encoding: 'utf8' }
            );
            const basisFiles = new Set(basisFilesResult.stdout.split(/\r?\n/u).filter(Boolean));
            const missingAtBasis = Array.isArray(compileSnapshot.entries)
                ? compileSnapshot.entries
                    .map((entry) => entry?.file)
                    .filter((entryPath) => typeof entryPath === 'string' && !basisFiles.has(entryPath))
                : [];
            if (basisFilesResult.status !== 0 || missingAtBasis.length > 0) {
                errors.push(`${field}.snapshots.compile-commands.json: compile source is not tracked at the basis commit`);
            }
        } catch (error) {
            errors.push(`${field}.snapshots.compile-commands.json: cannot validate basis source set: ${error.message}`);
        }
    }

    for (const [diagramName, expectedCounts] of Object.entries({
        engine: { classes: 9, relationships: 5 },
        network: { classes: 9, relationships: 1 }
    })) {
        const diagramProof = manifest.diagrams?.[diagramName];
        const relativePath = `docs/diagrams/class/${diagramName}.json`;
        if (diagramProof?.path !== relativePath) {
            errors.push(`${field}.diagrams.${diagramName}.path: must be ${relativePath}`);
            continue;
        }
        try {
            const bytes = await readFile(path.join(options.root, relativePath));
            const diagram = JSON.parse(bytes.toString('utf8'));
            if (diagramProof.sha256 !== sha256(bytes)
                || diagramProof.classCount !== expectedCounts.classes
                || diagramProof.relationshipCount !== expectedCounts.relationships
                || diagramProof.classCount !== diagram.elements?.length
                || diagramProof.relationshipCount !== diagram.relationships?.length) {
                errors.push(`${field}.diagrams.${diagramName}: committed raw JSON hash or counts mismatch`);
            }
        } catch (error) {
            errors.push(`${field}.diagrams.${diagramName}: cannot validate raw JSON: ${error.message}`);
        }
    }
    return errors;
}

async function validateClassDiagramProof(item, evidencePaths, options) {
    const errors = [];
    const proof = item.proof;
    if (proof?.tool !== 'clang-uml') {
        errors.push(`${item.id}.proof.tool: must be clang-uml`);
    }
    if (proof?.toolVersion !== '0.6.3') {
        errors.push(`${item.id}.proof.toolVersion: must be 0.6.3`);
    }
    if (proof?.basisCommitSha !== CANONICAL_CODE_BASIS_SHA) {
        errors.push(`${item.id}.proof.basisCommitSha: must match the canonical code basis SHA`);
    }
    const proofPaths = [
        proof?.outputs?.engine?.json,
        proof?.outputs?.network?.json,
        proof?.outputs?.engine?.html,
        proof?.outputs?.network?.html
    ];
    for (const [index, requiredPath] of REQUIRED_CLASS_DIAGRAM_PATHS.entries()) {
        if (proofPaths[index] !== requiredPath || !evidencePaths.includes(requiredPath)) {
            errors.push(`${item.id}.proof.outputs: must list ${requiredPath}`);
        }
    }

    const manifestProof = proof?.manifest;
    if (manifestProof?.path !== CLASS_GENERATION_MANIFEST_PATH
        || !evidencePaths.includes(CLASS_GENERATION_MANIFEST_PATH)) {
        errors.push(`${item.id}.proof.manifest.path: must be ${CLASS_GENERATION_MANIFEST_PATH}`);
    }
    if (!/^[0-9a-f]{64}$/u.test(manifestProof?.sha256 ?? '')) {
        errors.push(`${item.id}.proof.manifest.sha256: must be a lowercase SHA-256`);
    } else if (manifestProof?.path === CLASS_GENERATION_MANIFEST_PATH) {
        try {
            const manifestBytes = await readFile(path.join(options.root, CLASS_GENERATION_MANIFEST_PATH));
            if (sha256(manifestBytes) !== manifestProof.sha256) {
                errors.push(`${item.id}.proof.manifest.sha256: current manifest does not match release proof`);
            }
            const manifest = JSON.parse(manifestBytes.toString('utf8'));
            errors.push(...await validateClassGenerationManifest(manifest, options));
            if (proof?.inputs?.config?.sha256 !== manifest.tooling?.config?.sha256
                || proof?.inputs?.generator?.sha256 !== manifest.tooling?.generator?.sha256) {
                errors.push(`${item.id}.proof.manifest: tooling hashes do not match release proof inputs`);
            }
        } catch (error) {
            errors.push(`${item.id}.proof.manifest: cannot load generation manifest: ${error.message}`);
        }
    }

    const requiredInputs = [
        ['config', '.clang-uml'],
        ['generator', 'scripts/portfolio/generate-class-diagrams.ps1']
    ];
    for (const [inputName, requiredPath] of requiredInputs) {
        const input = proof?.inputs?.[inputName];
        const field = `${item.id}.proof.inputs.${inputName}`;
        if (input?.path !== requiredPath) {
            errors.push(`${field}.path: must be ${requiredPath}`);
            continue;
        }
        if (!/^[0-9a-f]{64}$/u.test(input?.sha256 ?? '')) {
            errors.push(`${field}.sha256: must be a lowercase SHA-256`);
            continue;
        }
        try {
            const content = await readFile(path.join(options.root, requiredPath), 'utf8');
            if (normalizedTextSha256(content) !== input.sha256) {
                errors.push(`${field}.sha256: current file does not match release provenance`);
            }
        } catch (error) {
            errors.push(`${field}.path: cannot read ${requiredPath}: ${error.message}`);
        }
    }

    for (const diagramName of ['engine', 'network']) {
        const jsonPath = proof?.outputs?.[diagramName]?.json;
        const htmlPath = proof?.outputs?.[diagramName]?.html;
        if (jsonPath !== `docs/diagrams/class/${diagramName}.json`
            || htmlPath !== `docs/diagrams/class/${diagramName}.html`) {
            continue;
        }
        let diagram;
        try {
            diagram = JSON.parse(await readFile(path.join(options.root, jsonPath), 'utf8'));
        } catch (error) {
            errors.push(`${item.id}.proof.outputs.${diagramName}.json: raw clang-uml JSON is invalid: ${error.message}`);
            continue;
        }
        const diagramErrors = await validateClassDiagram(diagram, {
            root: options.root,
            verifyBasisCommit: options.verifyClassBasisCommit ?? true
        });
        errors.push(...diagramErrors.map((error) => (
            `${item.id}.proof.outputs.${diagramName}.json: raw clang-uml validation failed: ${error}`
        )));
        if (diagramErrors.length > 0) {
            continue;
        }
        try {
            const actualHtml = await readFile(path.join(options.root, htmlPath), 'utf8');
            if (actualHtml !== renderClassDiagram(diagram)) {
                errors.push(`${item.id}.proof.outputs.${diagramName}.html: does not match the production renderer`);
            }
        } catch (error) {
            errors.push(`${item.id}.proof.outputs.${diagramName}.html: renderer output cannot be read: ${error.message}`);
        }
    }
    return errors;
}

function validatePdfProof(item, evidencePaths) {
    const errors = [];
    const proof = item.proof;
    if (typeof proof?.path !== 'string'
        || !proof.path.toLowerCase().endsWith('.pdf')
        || !evidencePaths.includes(proof.path)) {
        errors.push(`${item.id}.proof.path: must name the PDF evidence file`);
    }
    if (!Number.isInteger(proof?.pageCount) || proof.pageCount < 18 || proof.pageCount > 22) {
        errors.push(`${item.id}.proof.pageCount: must be an integer from 18 through 22`);
    }
    if (proof?.rendered !== true) {
        errors.push(`${item.id}.proof.rendered: must be true`);
    }
    if (proof?.linksChecked !== true) {
        errors.push(`${item.id}.proof.linksChecked: must be true`);
    }
    return errors;
}

function validateDemoProof(item, evidencePaths) {
    const errors = [];
    const proof = item.proof;
    if (typeof proof?.path !== 'string'
        || !/\.(?:mp4|mov|webm)$/iu.test(proof.path)
        || !evidencePaths.includes(proof.path)) {
        errors.push(`${item.id}.proof.path: must name the demo video evidence file`);
    }
    if (typeof proof?.durationSeconds !== 'number'
        || !Number.isFinite(proof.durationSeconds)
        || proof.durationSeconds <= 0) {
        errors.push(`${item.id}.proof.durationSeconds: must be a positive finite number`);
    }
    if (proof?.playbackChecked !== true) {
        errors.push(`${item.id}.proof.playbackChecked: must be true`);
    }
    if (proof?.localDemoChecked !== true) {
        errors.push(`${item.id}.proof.localDemoChecked: must be true`);
    }
    return errors;
}

function sha256(content) {
    return createHash('sha256').update(content).digest('hex');
}

function parseLfsPointer(pointerBytes) {
    if (!Buffer.isBuffer(pointerBytes) || pointerBytes.some((byte) => byte > 0x7f)) {
        return null;
    }
    const pointerText = pointerBytes.toString('ascii');
    const match = /^version https:\/\/git-lfs\.github\.com\/spec\/v1\noid sha256:([0-9a-f]{64})\nsize (0|[1-9][0-9]*)\n$/u.exec(pointerText);
    if (!match) {
        return null;
    }
    const size = Number(match[2]);
    return Number.isSafeInteger(size) ? { oid: match[1], size } : null;
}

function runReadOnlyGit(root, argumentsList, options = {}) {
    return spawnSync('git', argumentsList, {
        cwd: root,
        ...(options.raw ? {} : { encoding: 'utf8' })
    });
}

function hasExactOrderedPaths(actualPaths, expectedPaths) {
    return Array.isArray(actualPaths)
        && actualPaths.length === expectedPaths.length
        && actualPaths.every((entry, index) => entry === expectedPaths[index]);
}

async function validateLfsObject(item, lfsPath, index, options) {
    const errors = [];
    const field = `${item.id}.proof.paths[${index}]`;
    const attribute = runReadOnlyGit(
        options.root,
        ['check-attr', '--source=HEAD', 'filter', '--', lfsPath]
    );
    if (attribute.status !== 0) {
        const detail = (attribute.stderr || attribute.stdout || '').trim();
        errors.push(`${field}.attribute: git check-attr --source=HEAD failed or is unsupported for ${lfsPath}${detail ? ` (${detail})` : ''}`);
    } else if (attribute.stdout.trim() !== `${lfsPath}: filter: lfs`) {
        errors.push(`${field}.attribute: ${lfsPath} is not LFS-tracked in committed HEAD attributes`);
    }

    const blob = runReadOnlyGit(
        options.root,
        ['cat-file', 'blob', `HEAD:${lfsPath}`],
        { raw: true }
    );
    if (blob.status !== 0) {
        errors.push(`${field}.pointer: committed blob is missing`);
        return errors;
    }
    const pointer = parseLfsPointer(blob.stdout);
    if (!pointer) {
        errors.push(`${field}.pointer: committed blob for ${lfsPath} is not exact canonical Git LFS pointer bytes`);
        return errors;
    }

    const objectPath = path.join(
        options.gitCommonDirectory,
        'lfs',
        'objects',
        pointer.oid.slice(0, 2),
        pointer.oid.slice(2, 4),
        pointer.oid
    );
    let objectContent;
    try {
        const objectStats = await lstat(objectPath);
        if (!objectStats.isFile()) {
            errors.push(`${field}.object: LFS object path is not a regular file`);
            return errors;
        }
        objectContent = await readFile(objectPath);
    } catch (error) {
        if (error.code === 'ENOENT') {
            errors.push(`${field}.object: LFS object file is missing`);
        } else {
            errors.push(`${field}.object: LFS object file cannot be read (${error.message})`);
        }
        return errors;
    }
    if (objectContent.length !== pointer.size || sha256(objectContent) !== pointer.oid) {
        errors.push(`${field}.object: object SHA-256 or size does not match committed pointer`);
    }

    const inspection = await inspectRepositoryPath(options.root, options.realRoot, lfsPath);
    if (inspection.status !== 'found') {
        return errors;
    }
    try {
        const worktreePath = path.join(options.root, lfsPath);
        const worktreeStats = await lstat(worktreePath);
        if (!worktreeStats.isFile()) {
            errors.push(`${field}.worktree: path is not a regular file`);
            return errors;
        }
        const worktreeContent = await readFile(inspection.realTarget);
        if (parseLfsPointer(worktreeContent)) {
            errors.push(`${field}.worktree: path still contains an LFS pointer`);
        } else if (worktreeContent.length !== objectContent.length
            || sha256(worktreeContent) !== sha256(objectContent)) {
            errors.push(`${field}.worktree: worktree SHA-256 or size does not match LFS object`);
        }
    } catch (error) {
        errors.push(`${field}.worktree: path cannot be read (${error.message})`);
    }
    return errors;
}

async function validateLfsProof(item, evidencePaths, options) {
    const errors = [];
    const proof = item.proof;
    if (!hasDatedCheck(proof?.checkedAt)) {
        errors.push(`${item.id}.proof.checkedAt: must be a nonempty parseable date`);
    }
    if (proof?.objectsVerified !== true) {
        errors.push(`${item.id}.proof.objectsVerified: must be true`);
    }
    if (proof?.objectsHydrated !== true) {
        errors.push(`${item.id}.proof.objectsHydrated: must be true`);
    }
    if (!hasExactOrderedPaths(proof?.paths, REQUIRED_RUNTIME_LFS_PATHS)) {
        errors.push(`${item.id}.proof.paths: must exactly list the sorted runtime LFS paths`);
    }
    if (!Array.isArray(proof?.paths)) {
        return errors;
    }
    for (const [index, lfsPath] of proof.paths.entries()) {
        if (typeof lfsPath !== 'string' || !evidencePaths.includes(lfsPath)) {
            errors.push(`${item.id}.proof.paths[${index}]: must name a listed evidence path`);
            continue;
        }
    }

    const commonDirectory = runReadOnlyGit(options.root, ['rev-parse', '--git-common-dir']);
    if (commonDirectory.status !== 0 || commonDirectory.stdout.trim().length === 0) {
        errors.push(`${item.id}.proof.objectsVerified: Git common directory cannot be resolved`);
        return errors;
    }
    const gitCommonDirectory = path.resolve(options.root, commonDirectory.stdout.trim());
    for (const [index, lfsPath] of proof.paths.entries()) {
        if (typeof lfsPath === 'string') {
            errors.push(...await validateLfsObject(item, lfsPath, index, {
                ...options,
                gitCommonDirectory
            }));
        }
    }
    return errors;
}

function validateReleaseCandidateSha(value, field, expectedSha) {
    if (!/^[0-9a-f]{40}$/u.test(value ?? '')) {
        return [`${field}: must be a 40-character lowercase commit SHA`];
    }
    if (expectedSha && value !== expectedSha) {
        return [`${field}: must match the release candidate commit ${expectedSha}`];
    }
    return [];
}

function validateBuildEnvironmentProof(environment, field, releaseCandidateCommitSha) {
    const errors = [];
    errors.push(...validateReleaseCandidateSha(
        environment?.commitSha,
        `${field}.commitSha`,
        releaseCandidateCommitSha
    ));
    if (environment?.buildPassed !== true) {
        errors.push(`${field}.buildPassed: must be true`);
    }
    if (environment?.testsPassed !== true) {
        errors.push(`${field}.testsPassed: must be true`);
    }
    if (!hasDatedCheck(environment?.checkedAt)) {
        errors.push(`${field}.checkedAt: must be a nonempty parseable date`);
    }
    return errors;
}

async function validateReleaseCandidateBuildProof(item, evidencePaths, options) {
    const errors = [];
    const proof = item.proof;
    const releaseCandidateCommitSha = proof?.releaseCandidateCommitSha;
    errors.push(...validateReleaseCandidateSha(
        releaseCandidateCommitSha,
        `${item.id}.proof.releaseCandidateCommitSha`
    ));

    if (/^[0-9a-f]{40}$/u.test(releaseCandidateCommitSha ?? '')) {
        const candidateResult = runReadOnlyGit(
            options.root,
            ['rev-parse', '--verify', `${releaseCandidateCommitSha}^{commit}`]
        );
        const resolvedCandidateSha = String(candidateResult.stdout ?? '').trim();
        if (candidateResult.status !== 0 || resolvedCandidateSha !== releaseCandidateCommitSha) {
            errors.push(`${item.id}.proof.releaseCandidateCommitSha: release candidate commit cannot be resolved`);
        } else {
            const ancestorResult = runReadOnlyGit(
                options.root,
                ['merge-base', '--is-ancestor', releaseCandidateCommitSha, 'HEAD']
            );
            if (ancestorResult.status === 1) {
                errors.push(`${item.id}.proof.releaseCandidateCommitSha: release candidate must be an ancestor of the proof record HEAD`);
            } else if (ancestorResult.status !== 0) {
                errors.push(`${item.id}.proof.releaseCandidateCommitSha: ancestry to the proof record HEAD cannot be verified`);
            }
        }
    }

    errors.push(...validateBuildEnvironmentProof(
        proof?.windows,
        `${item.id}.proof.windows`,
        releaseCandidateCommitSha
    ));
    errors.push(...await validateProofEvidenceFile(
        item,
        proof?.windows?.evidencePath,
        `${item.id}.proof.windows.evidencePath`,
        evidencePaths,
        options
    ));
    errors.push(...validateBuildEnvironmentProof(
        proof?.linuxServer,
        `${item.id}.proof.linuxServer`,
        releaseCandidateCommitSha
    ));
    errors.push(...await validateProofEvidenceFile(
        item,
        proof?.linuxServer?.evidencePath,
        `${item.id}.proof.linuxServer.evidencePath`,
        evidencePaths,
        options
    ));
    errors.push(...validateReleaseCandidateSha(
        proof?.hostedCi?.commitSha,
        `${item.id}.proof.hostedCi.commitSha`,
        releaseCandidateCommitSha
    ));
    if (proof?.hostedCi?.status !== 'success') {
        errors.push(`${item.id}.proof.hostedCi.status: must be success`);
    }
    if (!/^https:\/\//iu.test(proof?.hostedCi?.runUrl ?? '')) {
        errors.push(`${item.id}.proof.hostedCi.runUrl: must be an HTTPS run URL`);
    }
    if (!hasDatedCheck(proof?.hostedCi?.checkedAt)) {
        errors.push(`${item.id}.proof.hostedCi.checkedAt: must be a nonempty parseable date`);
    }
    return errors;
}

async function validateProofEvidenceFile(item, relativePath, field, evidencePaths, options) {
    if (typeof relativePath !== 'string' || !evidencePaths.includes(relativePath)) {
        return [`${field}: must name a listed evidence path`];
    }
    const inspection = await inspectRepositoryPath(options.root, options.realRoot, relativePath);
    if (inspection.status === 'invalid') {
        return [`${field}: ${inspection.error}`];
    }
    if (inspection.status === 'outside-real') {
        return [`${field}: real target resolves outside repository root`];
    }
    if (inspection.status === 'missing') {
        return [`${field}: evidence file is missing`];
    }
    if (inspection.status === 'unreadable') {
        return [`${field}: evidence file cannot be resolved (${inspection.error.message})`];
    }
    if (!inspection.stats.isFile() || inspection.stats.size === 0) {
        return [`${field}: must resolve to a nonempty regular file`];
    }
    return [];
}

async function validateVisualArtifactProof(item, evidencePaths, options) {
    const errors = [];
    const proof = item.proof;
    errors.push(...await validateProofEvidenceFile(
        item,
        proof?.warp?.resultPath,
        `${item.id}.proof.warp.resultPath`,
        evidencePaths,
        options
    ));
    if (proof?.warp?.offscreen !== true) {
        errors.push(`${item.id}.proof.warp.offscreen: must be true`);
    }
    if (proof?.warp?.status !== 'passed') {
        errors.push(`${item.id}.proof.warp.status: must be passed`);
    }
    if (!hasDatedCheck(proof?.warp?.checkedAt)) {
        errors.push(`${item.id}.proof.warp.checkedAt: must be a nonempty parseable date`);
    }

    const artifactPaths = proof?.rtx?.artifactPaths;
    if (!Array.isArray(artifactPaths) || artifactPaths.length === 0) {
        errors.push(`${item.id}.proof.rtx.artifactPaths: must be a nonempty array`);
    } else {
        if (new Set(artifactPaths).size !== artifactPaths.length) {
            errors.push(`${item.id}.proof.rtx.artifactPaths: paths must be unique`);
        }
        for (const [index, artifactPath] of artifactPaths.entries()) {
            errors.push(...await validateProofEvidenceFile(
                item,
                artifactPath,
                `${item.id}.proof.rtx.artifactPaths[${index}]`,
                evidencePaths,
                options
            ));
        }
    }

    const review = proof?.rtx?.review;
    if (!hasDatedCheck(review?.checkedAt)) {
        errors.push(`${item.id}.proof.rtx.review.checkedAt: must be a nonempty parseable date`);
    }
    if (typeof review?.reviewer !== 'string' || review.reviewer.trim().length === 0) {
        errors.push(`${item.id}.proof.rtx.review.reviewer: must be a nonempty reviewer label`);
    }
    if (review?.verdict !== 'approved') {
        errors.push(`${item.id}.proof.rtx.review.verdict: must be approved`);
    }
    if (typeof review?.notes !== 'string' || review.notes.trim().length === 0) {
        errors.push(`${item.id}.proof.rtx.review.notes: must be nonempty`);
    }
    return errors;
}

async function validateVerifiedProof(item, evidencePaths, options) {
    if (item.status !== 'verified') {
        return [];
    }

    if (item.id === 'current-head-builds') {
        return validateReleaseCandidateBuildProof(item, evidencePaths, options);
    }
    if (item.id === 'warp-rtx-visual-artifacts') {
        return validateVisualArtifactProof(item, evidencePaths, options);
    }
    if (item.id === 'class-diagrams') {
        return validateClassDiagramProof(item, evidencePaths, options);
    }
    if (item.id === 'portfolio-pdf') {
        return validatePdfProof(item, evidencePaths);
    }
    if (item.id === 'demo-video') {
        return validateDemoProof(item, evidencePaths);
    }
    if (item.id === 'lfs-object-availability') {
        return validateLfsProof(item, evidencePaths, options);
    }
    if (item.id === 'aws-external-test') {
        const errors = [];
        const resourceState = item.resourceState;
        for (const field of ['created', 'externalTestVerified', 'cleanupVerified']) {
            if (resourceState?.[field] !== true) {
                errors.push(`${item.id}.resourceState.${field}: must be true before verification`);
            }
        }
        if (!hasDatedCheck(resourceState?.checkedAt)) {
            errors.push(`${item.id}.resourceState.checkedAt: must be a nonempty parseable date`);
        }
        return errors;
    }
    if (item.id === 'repository-visibility') {
        const errors = [];
        const verification = item.externalVerification;
        if (verification?.visibility !== 'public') {
            errors.push(`${item.id}.externalVerification.visibility: must be public`);
        }
        if (!hasDatedCheck(verification?.checkedAt)) {
            errors.push(`${item.id}.externalVerification.checkedAt: must be a nonempty parseable date`);
        }
        if (!/^https:\/\//iu.test(verification?.url ?? '')) {
            errors.push(`${item.id}.externalVerification.url: must be an HTTPS repository URL`);
        }
        return errors;
    }
    if (item.id === 'v0.1.0') {
        const errors = [];
        if (item.proof?.tag !== 'v0.1.0') {
            errors.push(`${item.id}.proof.tag: must be v0.1.0`);
        }
        errors.push(...validateReleaseCandidateSha(
            item.proof?.targetCommitSha,
            `${item.id}.proof.targetCommitSha`
        ));
        const tagResult = runReadOnlyGit(
            options.root,
            ['rev-parse', '--verify', 'refs/tags/v0.1.0^{commit}']
        );
        const tagTargetSha = String(tagResult.stdout ?? '').trim();
        if (tagResult.status !== 0 || !/^[0-9a-f]{40}$/u.test(tagTargetSha)) {
            errors.push(`${item.id}.proof.tag: local tag v0.1.0 does not exist`);
        } else {
            if (item.proof?.targetCommitSha !== tagTargetSha) {
                errors.push(`${item.id}.proof.targetCommitSha: must equal actual v0.1.0 target ${tagTargetSha}`);
            }
            const currentHeadItem = options.items.find((candidate) => candidate.id === 'current-head-builds');
            const releaseCandidateCommitSha = currentHeadItem?.proof?.releaseCandidateCommitSha;
            if (currentHeadItem?.status !== 'verified' || !/^[0-9a-f]{40}$/u.test(releaseCandidateCommitSha ?? '')) {
                errors.push(`${item.id}.proof.targetCommitSha: verified current-head-builds release candidate is missing`);
            } else if (tagTargetSha !== releaseCandidateCommitSha) {
                errors.push(`${item.id}.proof.targetCommitSha: actual tag target must equal release-candidate commit ${releaseCandidateCommitSha}`);
            }
        }
        const openPrerequisites = options.items
            .filter((candidate) => candidate.id !== 'v0.1.0' && candidate.status !== 'verified')
            .map((candidate) => candidate.id);
        if (openPrerequisites.length > 0) {
            errors.push(`${item.id}.status: publication prerequisites are not verified: ${openPrerequisites.join(', ')}`);
        }
        return errors;
    }
    return [];
}

export async function validateReleaseStatus(document, options = {}) {
    const errors = [];
    const root = path.resolve(options.root ?? process.cwd());
    let realRoot;
    try {
        realRoot = await realpath(root);
    } catch (error) {
        return [`release-status.json: repository root cannot be resolved: ${error.message}`];
    }

    if (!document || typeof document !== 'object' || Array.isArray(document)) {
        return ['release-status.json: document must be an object'];
    }
    if (document.schemaVersion !== 1) {
        errors.push('release-status.json.schemaVersion: must be 1');
    }
    if (document.codeBasisCommitSha !== CANONICAL_CODE_BASIS_SHA) {
        errors.push(`release-status.json.codeBasisCommitSha: must be ${CANONICAL_CODE_BASIS_SHA}`);
    }
    if (!Array.isArray(document.items)) {
        errors.push('release-status.json.items: must be an array');
        return errors;
    }

    const seenIds = new Set();
    for (const [index, item] of document.items.entries()) {
        if (!item || typeof item !== 'object' || Array.isArray(item)) {
            errors.push(`release-status.json.items[${index}]: must be an object`);
            continue;
        }

        const id = typeof item.id === 'string' && item.id.length > 0 ? item.id : `items[${index}]`;
        if (id === `items[${index}]`) {
            errors.push(`release-status.json.${id}.id: must be a non-empty string`);
        } else if (seenIds.has(id)) {
            errors.push(`release-status.json.${id}.id: duplicate release item id`);
        } else {
            seenIds.add(id);
        }

        if (typeof item.label !== 'string' || item.label.length === 0) {
            errors.push(`release-status.json.${id}.label: must be a non-empty string`);
        }
        if (!RELEASE_STATUSES.has(item.status)) {
            errors.push(`release-status.json.${id}.status: must be verified, partial, missing, or blocked`);
        }

        const evidencePaths = Array.isArray(item.evidence) ? item.evidence : [];
        if (!Array.isArray(item.evidence)) {
            errors.push(`release-status.json.${id}.evidence: must be an array`);
        }
        if (item.status === 'verified' && evidencePaths.length === 0) {
            errors.push(`release-status.json.${id}.evidence: verified items require at least one evidence path`);
        }

        for (const [evidenceIndex, evidencePath] of evidencePaths.entries()) {
            const field = `release-status.json.${id}.evidence[${evidenceIndex}]`;
            const inspection = await inspectRepositoryPath(root, realRoot, evidencePath);
            if (inspection.status === 'invalid') {
                errors.push(`${field}: ${inspection.error}`);
            } else if (inspection.status === 'outside-real') {
                errors.push(`${field}: real target resolves outside repository root: ${evidencePath}`);
            } else if (inspection.status === 'missing') {
                errors.push(`${field}: local evidence path is missing: ${evidencePath}`);
            } else if (inspection.status === 'unreadable') {
                errors.push(`${field}: local evidence path cannot be resolved: ${evidencePath} (${inspection.error.message})`);
            } else if (!inspection.stats.isFile() || inspection.stats.size === 0) {
                errors.push(`${field}: evidence must be a nonempty regular file: ${evidencePath}`);
            }
        }

        const proofErrors = await validateVerifiedProof(item, evidencePaths, {
            root,
            realRoot,
            items: document.items,
            verifyClassBasisCommit: options.verifyClassBasisCommit
        });
        if (proofErrors.length > 0) {
            errors.push(`release-status.json.${id}.status: verified proof requirements are not satisfied`);
        }
        errors.push(...proofErrors.map((error) => `release-status.json.${error}`));

        if (id === 'aws-external-test') {
            const resourceState = item.resourceState;
            if (!resourceState
                || typeof resourceState.created !== 'boolean'
                || typeof resourceState.cleanupVerified !== 'boolean') {
                errors.push('release-status.json.aws-external-test.resourceState: created and cleanupVerified must be booleans');
            } else if (!resourceState.created && resourceState.cleanupVerified) {
                errors.push('release-status.json.aws-external-test.resourceState.cleanupVerified: cannot be true when no AWS resource was created');
            }
        }
    }

    for (const requiredId of REQUIRED_RELEASE_ITEM_IDS) {
        if (!seenIds.has(requiredId)) {
            errors.push(`release-status.json.items.${requiredId}: required release item is missing`);
        }
    }

    return errors;
}

function countRun(markdown, start, character) {
    let end = start;
    while (markdown[end] === character) {
        end += 1;
    }
    return end - start;
}

function maskRange(mask, markdown, start, end) {
    for (let index = start; index < end; index += 1) {
        if (markdown[index] !== '\n' && markdown[index] !== '\r') {
            mask[index] = true;
        }
    }
}

function createCodeMask(markdown) {
    const mask = Array(markdown.length).fill(false);
    let activeFence;
    let lineStart = 0;
    while (lineStart < markdown.length) {
        const newlineIndex = markdown.indexOf('\n', lineStart);
        const lineEnd = newlineIndex === -1 ? markdown.length : newlineIndex + 1;
        const contentEnd = newlineIndex === -1 ? markdown.length : newlineIndex;
        let cursor = lineStart;
        while (cursor < contentEnd && cursor - lineStart < 3 && markdown[cursor] === ' ') {
            cursor += 1;
        }
        const fenceCharacter = markdown[cursor];
        const fenceLength = fenceCharacter === '`' || fenceCharacter === '~'
            ? countRun(markdown, cursor, fenceCharacter)
            : 0;
        const isFence = fenceLength >= 3;

        if (!activeFence && isFence) {
            activeFence = { character: fenceCharacter, length: fenceLength };
            maskRange(mask, markdown, lineStart, lineEnd);
        } else if (activeFence) {
            maskRange(mask, markdown, lineStart, lineEnd);
            if (isFence
                && fenceCharacter === activeFence.character
                && fenceLength >= activeFence.length
                && markdown.slice(cursor + fenceLength, contentEnd).trim().length === 0) {
                activeFence = undefined;
            }
        }
        lineStart = lineEnd;
    }

    for (let index = 0; index < markdown.length; index += 1) {
        if (mask[index] || markdown[index] !== '`') {
            continue;
        }
        const openingLength = countRun(markdown, index, '`');
        let closingStart = index + openingLength;
        while (closingStart < markdown.length) {
            if (!mask[closingStart] && markdown[closingStart] === '`') {
                const closingLength = countRun(markdown, closingStart, '`');
                if (closingLength === openingLength) {
                    maskRange(mask, markdown, index, closingStart + closingLength);
                    index = closingStart + closingLength - 1;
                    break;
                }
                closingStart += closingLength;
            } else {
                closingStart += 1;
            }
        }
    }
    return mask;
}

function parseBracket(markdown, start, mask) {
    if (markdown[start] !== '[' || mask[start]) {
        return null;
    }
    let depth = 1;
    let value = '';
    for (let index = start + 1; index < markdown.length; index += 1) {
        if (mask[index]) {
            return null;
        }
        if (markdown[index] === '\\' && index + 1 < markdown.length) {
            value += markdown[index + 1];
            index += 1;
        } else if (markdown[index] === '[') {
            depth += 1;
            value += markdown[index];
        } else if (markdown[index] === ']') {
            depth -= 1;
            if (depth === 0) {
                return { value, end: index };
            }
            value += markdown[index];
        } else {
            value += markdown[index];
        }
    }
    return null;
}

function normalizeReferenceLabel(label) {
    return label.trim().replace(/\s+/gu, ' ').toLowerCase();
}

function parseDefinitionDestination(markdown, start, lineEnd) {
    let cursor = start;
    if (markdown[cursor] === '<') {
        let value = '';
        for (cursor += 1; cursor < lineEnd; cursor += 1) {
            if (markdown[cursor] === '\\' && cursor + 1 < lineEnd) {
                value += markdown[cursor + 1];
                cursor += 1;
            } else if (markdown[cursor] === '>') {
                return value;
            } else {
                value += markdown[cursor];
            }
        }
        return null;
    }

    let value = '';
    let depth = 0;
    for (; cursor < lineEnd; cursor += 1) {
        const character = markdown[cursor];
        if (character === '\\' && cursor + 1 < lineEnd) {
            value += markdown[cursor + 1];
            cursor += 1;
        } else if (/\s/u.test(character) && depth === 0) {
            break;
        } else {
            if (character === '(') {
                depth += 1;
            } else if (character === ')' && depth > 0) {
                depth -= 1;
            }
            value += character;
        }
    }
    return value.length > 0 && depth === 0 ? value : null;
}

function collectReferenceDefinitions(markdown, mask) {
    const definitions = new Map();
    const definitionLines = [];
    let lineStart = 0;
    while (lineStart < markdown.length) {
        const newlineIndex = markdown.indexOf('\n', lineStart);
        const lineEnd = newlineIndex === -1 ? markdown.length : newlineIndex;
        let cursor = lineStart;
        while (cursor < lineEnd && cursor - lineStart < 3 && markdown[cursor] === ' ') {
            cursor += 1;
        }
        const label = parseBracket(markdown, cursor, mask);
        if (label && markdown[label.end + 1] === ':') {
            cursor = label.end + 2;
            while (cursor < lineEnd && /[ \t]/u.test(markdown[cursor])) {
                cursor += 1;
            }
            const target = parseDefinitionDestination(markdown, cursor, lineEnd);
            if (target !== null) {
                const normalizedLabel = normalizeReferenceLabel(label.value);
                if (!definitions.has(normalizedLabel)) {
                    definitions.set(normalizedLabel, target);
                }
                definitionLines.push({ start: lineStart, end: lineEnd });
            }
        }
        lineStart = newlineIndex === -1 ? markdown.length : newlineIndex + 1;
    }
    return { definitions, definitionLines };
}

function parseInlineDestination(markdown, openParenthesis, mask) {
    let cursor = openParenthesis + 1;
    while (cursor < markdown.length && /\s/u.test(markdown[cursor])) {
        cursor += 1;
    }
    let target = '';
    let angled = false;
    if (markdown[cursor] === '<') {
        angled = true;
        cursor += 1;
    }

    let depth = 0;
    let targetFinished = false;
    let quote;
    for (; cursor < markdown.length; cursor += 1) {
        if (mask[cursor]) {
            return null;
        }
        const character = markdown[cursor];
        if (character === '\\' && cursor + 1 < markdown.length) {
            if (!targetFinished) {
                target += markdown[cursor + 1];
            }
            cursor += 1;
            continue;
        }
        if (angled && !targetFinished) {
            if (character === '>') {
                targetFinished = true;
            } else {
                target += character;
            }
            continue;
        }
        if (!angled && !targetFinished && /\s/u.test(character) && depth === 0) {
            targetFinished = true;
            continue;
        }
        if (targetFinished && (character === '"' || character === "'")) {
            quote = quote === character ? undefined : (quote ?? character);
            continue;
        }
        if (!quote && character === '(') {
            depth += 1;
            if (!targetFinished) {
                target += character;
            }
        } else if (!quote && character === ')') {
            if (depth === 0) {
                return target.length > 0 ? { target, end: cursor } : null;
            }
            depth -= 1;
            if (!targetFinished) {
                target += character;
            }
        } else if (!targetFinished) {
            target += character;
        }
    }
    return null;
}

function extractMarkdownTargets(markdown) {
    const mask = createCodeMask(markdown);
    const { definitions, definitionLines } = collectReferenceDefinitions(markdown, mask);
    for (const line of definitionLines) {
        maskRange(mask, markdown, line.start, line.end);
    }

    const targets = [];
    const seenTargets = new Set();
    const addTarget = (target) => {
        if (!seenTargets.has(target)) {
            targets.push(target);
            seenTargets.add(target);
        }
    };

    for (let index = 0; index < markdown.length; index += 1) {
        if (mask[index]) {
            continue;
        }
        const bracketStart = markdown[index] === '!' && markdown[index + 1] === '['
            ? index + 1
            : index;
        if (markdown[bracketStart] !== '[' || mask[bracketStart]) {
            continue;
        }
        const label = parseBracket(markdown, bracketStart, mask);
        if (!label) {
            continue;
        }

        const nextIndex = label.end + 1;
        if (markdown[nextIndex] === '(' && !mask[nextIndex]) {
            const destination = parseInlineDestination(markdown, nextIndex, mask);
            if (destination) {
                addTarget(destination.target);
                index = destination.end;
            }
        } else if (markdown[nextIndex] === '[' && !mask[nextIndex]) {
            const reference = parseBracket(markdown, nextIndex, mask);
            if (reference) {
                const referenceLabel = normalizeReferenceLabel(reference.value || label.value);
                if (definitions.has(referenceLabel)) {
                    addTarget(definitions.get(referenceLabel));
                }
                index = reference.end;
            }
        } else {
            const referenceLabel = normalizeReferenceLabel(label.value);
            if (definitions.has(referenceLabel)) {
                addTarget(definitions.get(referenceLabel));
            }
            index = label.end;
        }
    }
    return targets;
}

function isExternalTarget(target) {
    return target.startsWith('#')
        || target.startsWith('//')
        || /^[a-z][a-z0-9+.-]*:/iu.test(target);
}

async function listMarkdownFiles(root) {
    const files = [];
    const directories = [root];
    while (directories.length > 0) {
        const directory = directories.pop();
        const entries = await readdir(directory, { withFileTypes: true });
        for (const entry of entries) {
            if (entry.isDirectory()) {
                if (!MARKDOWN_SCAN_EXCLUSIONS.has(entry.name)) {
                    directories.push(path.join(directory, entry.name));
                }
            } else if (entry.isFile() && entry.name.toLowerCase().endsWith('.md')) {
                files.push(toPosixPath(path.relative(root, path.join(directory, entry.name))));
            }
        }
    }
    return files.sort((left, right) => left.localeCompare(right, 'en'));
}

export async function validateMarkdownLinks(options = {}) {
    const root = path.resolve(options.root ?? process.cwd());
    let realRoot;
    try {
        realRoot = await realpath(root);
    } catch (error) {
        return [`Markdown scan: repository root cannot be resolved: ${error.message}`];
    }
    let files;
    try {
        files = options.files ?? await listMarkdownFiles(root);
    } catch (error) {
        return [`Markdown scan: failed to enumerate files: ${error.message}`];
    }

    const errors = [];
    for (const relativeFile of files) {
        const fileResult = resolveRepositoryPath(root, relativeFile);
        if (fileResult.error) {
            errors.push(`${relativeFile}: ${fileResult.error}`);
            continue;
        }
        const sourceInspection = await inspectResolvedRepositoryPath(root, realRoot, fileResult.resolvedPath);
        if (sourceInspection.status === 'outside-real') {
            errors.push(`${relativeFile}: Markdown source resolves outside repository root`);
            continue;
        }

        let markdown;
        try {
            markdown = await readFile(fileResult.resolvedPath, 'utf8');
        } catch (error) {
            errors.push(`${relativeFile}: failed to read Markdown: ${error.message}`);
            continue;
        }

        for (const target of extractMarkdownTargets(markdown)) {
            if (isExternalTarget(target)) {
                continue;
            }

            const withoutFragment = target.split('#', 1)[0].split('?', 1)[0];
            if (!withoutFragment) {
                continue;
            }

            let decodedTarget;
            try {
                decodedTarget = decodeURIComponent(withoutFragment);
            } catch {
                errors.push(`${relativeFile}: local link target is not valid URI encoding: ${target}`);
                continue;
            }

            const resolvedTarget = decodedTarget.startsWith('/')
                ? path.resolve(root, decodedTarget.slice(1))
                : path.resolve(path.dirname(fileResult.resolvedPath), decodedTarget);
            if (resolvedTarget !== root && !resolvedTarget.startsWith(`${root}${path.sep}`)) {
                errors.push(`${relativeFile}: local link target escapes repository root: ${target}`);
                continue;
            }
            const targetInspection = await inspectResolvedRepositoryPath(root, realRoot, resolvedTarget);
            if (targetInspection.status === 'outside-real') {
                errors.push(`${relativeFile}: local link target resolves outside repository root: ${target}`);
            } else if (targetInspection.status === 'missing') {
                const missingPath = toPosixPath(path.relative(root, resolvedTarget));
                errors.push(`${relativeFile}: local link target is missing: ${missingPath}`);
            } else if (targetInspection.status === 'unreadable') {
                errors.push(`${relativeFile}: local link target cannot be resolved: ${target} (${targetInspection.error.message})`);
            }
        }
    }

    return errors;
}

async function verifyEvidenceContract(root) {
    const evidencePath = path.join(root, 'docs', 'portfolio', 'evidence.json');
    try {
        const document = await loadEvidence(evidencePath);
        const errors = await validateEvidence(document, { root });
        return {
            errors: errors.map((error) => `docs/portfolio/evidence.json: ${error}`),
            summary: `evidence: ${document.cases?.length ?? 0} cases validated`
        };
    } catch (error) {
        return {
            errors: [`docs/portfolio/evidence.json: failed to load: ${error.message}`],
            summary: 'evidence: validation failed'
        };
    }
}

async function verifyDiagramContract(root) {
    const sourceDirectory = path.join(root, 'docs', 'diagrams', 'source');
    const errors = [];
    const entries = [];
    let sourceFiles;
    try {
        sourceFiles = (await readdir(sourceDirectory))
            .filter((file) => file.endsWith('.json'))
            .sort((left, right) => left.localeCompare(right, 'en'));
    } catch (error) {
        return {
            errors: [`docs/diagrams/source: failed to read diagram sources: ${error.message}`],
            summary: 'diagrams: validation failed'
        };
    }

    if (sourceFiles.length !== EXPECTED_DIAGRAM_SOURCE_FILES.length
        || sourceFiles.some((file, index) => file !== EXPECTED_DIAGRAM_SOURCE_FILES[index])) {
        errors.push(`docs/diagrams/source: expected files ${EXPECTED_DIAGRAM_SOURCE_FILES.join(', ')}`);
        return { errors, summary: 'diagrams: validation failed' };
    }

    for (const sourceFile of sourceFiles) {
        const relativeSource = `docs/diagrams/source/${sourceFile}`;
        let diagram;
        try {
            diagram = JSON.parse(await readFile(path.join(sourceDirectory, sourceFile), 'utf8'));
        } catch (error) {
            errors.push(`${relativeSource}: failed to load JSON: ${error.message}`);
            continue;
        }

        const diagramErrors = await validateDiagram(diagram, { root });
        errors.push(...diagramErrors.map((error) => `${relativeSource}: ${error}`));
        if (diagramErrors.length > 0) {
            continue;
        }

        const outputFile = sourceFile.replace(/\.json$/u, '.html');
        const relativeOutput = `docs/diagrams/${outputFile}`;
        try {
            const actual = await readFile(path.join(root, relativeOutput), 'utf8');
            const expected = renderDiagram(diagram);
            if (actual !== expected) {
                errors.push(`${relativeOutput}: generated HTML is stale; run render-diagrams.mjs --write`);
            }
        } catch (error) {
            errors.push(`${relativeOutput}: generated HTML is missing or unreadable: ${error.message}`);
        }
        entries.push({ title: diagram.title, file: outputFile, basisCommitSha: diagram.basisCommitSha });
    }

    const relativeIndex = 'docs/diagrams/index.html';
    try {
        const actualIndex = await readFile(path.join(root, relativeIndex), 'utf8');
        if (actualIndex !== renderIndex(entries)) {
            errors.push(`${relativeIndex}: generated HTML is stale; run render-diagrams.mjs --write`);
        }
    } catch (error) {
        errors.push(`${relativeIndex}: generated HTML is missing or unreadable: ${error.message}`);
    }

    return {
        errors,
        summary: `diagrams: ${sourceFiles.length} sources and 5 HTML files checked`
    };
}

async function verifyReleaseContract(root) {
    const statusPath = path.join(root, 'docs', 'portfolio', 'release-status.json');
    try {
        const document = await loadReleaseStatus(statusPath);
        return {
            errors: await validateReleaseStatus(document, { root }),
            summary: `release status: ${document.items?.length ?? 0} items validated`
        };
    } catch (error) {
        return {
            errors: [`docs/portfolio/release-status.json: failed to load: ${error.message}`],
            summary: 'release status: validation failed'
        };
    }
}

export async function verifyAll(rootPath) {
    const root = path.resolve(rootPath);
    const evidence = await verifyEvidenceContract(root);
    const diagrams = await verifyDiagramContract(root);
    const release = await verifyReleaseContract(root);
    const markdownFiles = await listMarkdownFiles(root);
    const markdownErrors = await validateMarkdownLinks({ root, files: markdownFiles });

    return {
        errors: [...evidence.errors, ...diagrams.errors, ...release.errors, ...markdownErrors],
        summaries: [
            evidence.summary,
            diagrams.summary,
            release.summary,
            `Markdown links: ${markdownFiles.length} files checked`
        ]
    };
}

function parseArguments(argumentsList) {
    if (argumentsList.length !== 2 || argumentsList[0] !== '--root') {
        return { error: 'usage: node scripts/portfolio/verify-all.mjs --root <path>' };
    }
    return { root: argumentsList[1] };
}

async function main() {
    const options = parseArguments(process.argv.slice(2));
    if (options.error) {
        console.error(options.error);
        process.exitCode = 1;
        return;
    }

    const result = await verifyAll(options.root);
    if (result.errors.length > 0) {
        console.error(result.errors.join('\n'));
        process.exitCode = 1;
        return;
    }
    for (const summary of result.summaries) {
        console.log(summary);
    }
}

const isCli = process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url);
if (isCli) {
    await main();
}
