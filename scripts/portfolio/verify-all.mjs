import { createHash } from 'node:crypto';
import { lstat, readFile, readdir, realpath, stat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

import { loadEvidence, validateEvidence } from './evidence.mjs';
import {
    renderDiagram,
    renderIndex,
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

function validateClassDiagramProof(item, evidencePaths) {
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

function parseLfsPointer(pointerText) {
    const normalized = pointerText.replaceAll('\r\n', '\n');
    const match = /^version https:\/\/git-lfs\.github\.com\/spec\/v1\noid sha256:([0-9a-f]{64})\nsize (0|[1-9][0-9]*)\n?$/u.exec(normalized);
    if (!match) {
        return null;
    }
    const size = Number(match[2]);
    return Number.isSafeInteger(size) ? { oid: match[1], size } : null;
}

function runReadOnlyGit(root, argumentsList) {
    return spawnSync('git', argumentsList, { cwd: root, encoding: 'utf8' });
}

function hasExactOrderedPaths(actualPaths, expectedPaths) {
    return Array.isArray(actualPaths)
        && actualPaths.length === expectedPaths.length
        && actualPaths.every((entry, index) => entry === expectedPaths[index]);
}

async function validateLfsObject(item, lfsPath, index, options) {
    const errors = [];
    const field = `${item.id}.proof.paths[${index}]`;
    const attribute = runReadOnlyGit(options.root, ['check-attr', 'filter', '--', lfsPath]);
    if (attribute.status !== 0 || attribute.stdout.trim() !== `${lfsPath}: filter: lfs`) {
        errors.push(`${field}: ${lfsPath} is not LFS-tracked`);
    }

    const blob = runReadOnlyGit(options.root, ['cat-file', 'blob', `HEAD:${lfsPath}`]);
    if (blob.status !== 0) {
        errors.push(`${field}.pointer: committed blob is missing`);
        return errors;
    }
    const pointer = parseLfsPointer(blob.stdout);
    if (!pointer) {
        errors.push(`${field}.pointer: committed blob is not an exact Git LFS pointer`);
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
        if (parseLfsPointer(worktreeContent.toString('utf8'))) {
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

async function validateVerifiedProof(item, evidencePaths, options) {
    if (item.status !== 'verified') {
        return [];
    }

    if (item.id === 'class-diagrams') {
        return validateClassDiagramProof(item, evidencePaths);
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
        const tagResult = spawnSync(
            'git',
            ['show-ref', '--verify', '--quiet', 'refs/tags/v0.1.0'],
            { cwd: options.root, encoding: 'utf8' }
        );
        if (tagResult.status !== 0) {
            errors.push(`${item.id}.proof.tag: local tag v0.1.0 does not exist`);
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
            items: document.items
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
