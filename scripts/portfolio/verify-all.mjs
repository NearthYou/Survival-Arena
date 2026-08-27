import { access, readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { loadEvidence, validateEvidence } from './evidence.mjs';
import {
    renderDiagram,
    renderIndex,
    validateDiagram
} from './render-diagrams.mjs';

const RELEASE_STATUSES = new Set(['verified', 'partial', 'missing', 'blocked']);
const REQUIRED_RELEASE_ITEM_IDS = [
    'historical-24-player-metrics',
    'historical-30-minute-soak',
    'licenses-assets-manifest',
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

async function pathExists(filePath) {
    try {
        await access(filePath);
        return true;
    } catch {
        return false;
    }
}

export async function loadReleaseStatus(statusPath) {
    return JSON.parse(await readFile(statusPath, 'utf8'));
}

function validateCurrentGate(item, evidencePaths) {
    const errors = [];
    if (item.status !== 'verified') {
        return errors;
    }

    if (item.id === 'class-diagrams') {
        const missingPaths = REQUIRED_CLASS_DIAGRAM_PATHS.filter((requiredPath) => !evidencePaths.includes(requiredPath));
        if (missingPaths.length > 0) {
            errors.push(`${item.id}.status: cannot be verified until AST class JSON and HTML evidence are listed`);
        }
    }

    if (item.id === 'portfolio-pdf' && !evidencePaths.some((evidencePath) => evidencePath.toLowerCase().endsWith('.pdf'))) {
        errors.push(`${item.id}.status: cannot be verified without a rendered PDF evidence path`);
    }

    if (item.id === 'demo-video'
        && !evidencePaths.some((evidencePath) => /\.(?:mp4|mov|webm)$/iu.test(evidencePath))) {
        errors.push(`${item.id}.status: cannot be verified without a demo video evidence path`);
    }

    if (item.id === 'repository-visibility') {
        const verification = item.externalVerification;
        if (!verification
            || verification.visibility !== 'public'
            || typeof verification.checkedAt !== 'string'
            || !/^https:\/\//iu.test(verification.url ?? '')) {
            errors.push(`${item.id}.status: cannot be verified without a dated public repository check`);
        }
    }

    return errors;
}

export async function validateReleaseStatus(document, options = {}) {
    const errors = [];
    const root = path.resolve(options.root ?? process.cwd());

    if (!document || typeof document !== 'object' || Array.isArray(document)) {
        return ['release-status.json: document must be an object'];
    }
    if (document.schemaVersion !== 1) {
        errors.push('release-status.json.schemaVersion: must be 1');
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
            const pathResult = resolveRepositoryPath(root, evidencePath);
            if (pathResult.error) {
                errors.push(`${field}: ${pathResult.error}`);
            } else if (!await pathExists(pathResult.resolvedPath)) {
                errors.push(`${field}: local evidence path is missing: ${evidencePath}`);
            }
        }

        errors.push(...validateCurrentGate(item, evidencePaths).map((error) => `release-status.json.${error}`));

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

function removeFencedCode(markdown) {
    return markdown
        .replace(/```[\s\S]*?```/gu, '')
        .replace(/~~~[\s\S]*?~~~/gu, '');
}

function extractMarkdownTargets(markdown) {
    const targets = [];
    const linkPattern = /!?\[[^\]]*\]\(\s*(<[^>]+>|[^)\s]+)(?:\s+(?:"[^"]*"|'[^']*'|\([^)]*\)))?\s*\)/gu;
    for (const match of removeFencedCode(markdown).matchAll(linkPattern)) {
        const rawTarget = match[1].startsWith('<') ? match[1].slice(1, -1) : match[1];
        targets.push(rawTarget.replace(/\\([\\() ])/gu, '$1'));
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
            if (!await pathExists(resolvedTarget)) {
                const missingPath = toPosixPath(path.relative(root, resolvedTarget));
                errors.push(`${relativeFile}: local link target is missing: ${missingPath}`);
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
