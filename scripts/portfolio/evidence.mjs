import { access, readFile, realpath, stat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

const SHA_PATTERN = /^[0-9a-f]{40}$/;
const CANONICAL_BASIS_COMMIT_SHA = '884e5e70d68d9fcf9dfe5638d97e06623da154c2';
const REQUIRED_CASE_HEADINGS = [
    '## 상황',
    '## 재현',
    '## 관찰',
    '## 가설과 비교한 대안',
    '## 선택',
    '## 구현',
    '## 검증',
    '## 남은 한계'
];

export async function loadEvidence(evidencePath) {
    return JSON.parse(await readFile(evidencePath, 'utf8'));
}

function resolveSourcePath(root, relativePath) {
    if (typeof relativePath !== 'string' || relativePath.length === 0) {
        return { error: 'source path must be a non-empty string' };
    }

    const resolvedPath = path.resolve(root, relativePath);
    const rootPrefix = `${root}${path.sep}`;

    if (!resolvedPath.startsWith(rootPrefix)) {
        return { error: `source path escapes root: ${relativePath}` };
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

function extractLocalMarkdownLinkPaths(caseDocumentPath, content) {
    const linkPaths = [];
    for (const match of content.matchAll(/\]\(([^)\s]+)\)/gu)) {
        const target = match[1].split('#', 1)[0].split('?', 1)[0];
        if (!target || /^[a-z][a-z0-9+.-]*:/iu.test(target) || target.startsWith('//')) {
            continue;
        }
        try {
            linkPaths.push(path.resolve(path.dirname(caseDocumentPath), decodeURIComponent(target)));
        } catch {
            continue;
        }
    }
    return linkPaths;
}

async function validateCaseDocument({ root, realRoot, caseRecord }) {
    const label = `case ${caseRecord.id} caseDocument`;
    const pathResult = resolveSourcePath(root, caseRecord.caseDocument);
    if (pathResult.error) {
        return [`${label}: ${pathResult.error}`];
    }

    let realTarget;
    let targetStats;
    try {
        realTarget = await realpath(pathResult.resolvedPath);
        if (!isWithinRealRoot(realRoot, realTarget)) {
            return [`${label}: real target resolves outside repository root: ${caseRecord.caseDocument}`];
        }
        targetStats = await stat(realTarget);
    } catch (error) {
        if (error.code === 'ENOENT') {
            return [`${label}: file is missing: ${caseRecord.caseDocument}`];
        }
        return [`${label}: cannot resolve file: ${caseRecord.caseDocument} (${error.message})`];
    }
    if (!targetStats.isFile()) {
        return [`${label}: must resolve to a regular file: ${caseRecord.caseDocument}`];
    }

    const errors = [];
    const content = await readFile(realTarget, 'utf8');
    const lines = content.split(/\r?\n/u);
    for (const heading of REQUIRED_CASE_HEADINGS) {
        const count = lines.filter((line) => line === heading).length;
        if (count !== 1) {
            errors.push(`${label}: ${heading} must appear exactly once, found ${count}`);
        }
    }

    const linkedPaths = extractLocalMarkdownLinkPaths(pathResult.resolvedPath, content);
    const requiredSources = [
        caseRecord.devlog,
        ...(caseRecord.adr === undefined ? [] : [caseRecord.adr]),
        ...(Array.isArray(caseRecord.evidence) ? caseRecord.evidence : [])
    ];
    for (const sourcePath of requiredSources) {
        const sourceResult = resolveSourcePath(root, sourcePath);
        if (!sourceResult.error && !linkedPaths.includes(sourceResult.resolvedPath)) {
            errors.push(`${label}: missing required Markdown link to ${sourcePath}`);
        }
    }
    return errors;
}

async function validateSourcePath({ root, basisCommitSha, relativePath, label, verifyBasisCommit }) {
    const errors = [];
    const pathResult = resolveSourcePath(root, relativePath);

    if (pathResult.error) {
        return [`${label}: ${pathResult.error}`];
    }

    try {
        await access(pathResult.resolvedPath);
    } catch {
        errors.push(`${label}: source file is missing from current checkout: ${relativePath}`);
        return errors;
    }

    if (!verifyBasisCommit || !SHA_PATTERN.test(basisCommitSha)) {
        return errors;
    }

    const result = spawnSync('git', ['cat-file', '-e', `${basisCommitSha}:${relativePath}`], {
        cwd: root,
        encoding: 'utf8'
    });

    if (result.status !== 0) {
        errors.push(`${label}: source file is missing from basis commit: ${relativePath}`);
    }

    return errors;
}

function collectExcludedPaths(caseRecord) {
    if (!Array.isArray(caseRecord.excludedEvidence)) {
        return [];
    }

    return caseRecord.excludedEvidence.map((entry, index) => ({
        label: `case ${caseRecord.id ?? index} excludedEvidence[${index}]`,
        path: typeof entry === 'string' ? entry : entry?.path
    }));
}

async function validateEvidenceUnchecked(document, options = {}) {
    const errors = [];
    const root = path.resolve(options.root ?? process.cwd());
    const realRoot = await realpath(root);
    const verifyBasisCommit = options.verifyBasisCommit ?? true;

    if (!document || typeof document !== 'object' || Array.isArray(document)) {
        return ['evidence document must be an object'];
    }

    if (document.schemaVersion !== 1) {
        errors.push('schemaVersion must be 1');
    }

    if (!SHA_PATTERN.test(document.basisCommitSha ?? '')) {
        errors.push('basisCommitSha must be a 40-character lowercase hexadecimal SHA');
    } else if (document.basisCommitSha !== CANONICAL_BASIS_COMMIT_SHA) {
        errors.push(`basisCommitSha must match the canonical portfolio basis: ${CANONICAL_BASIS_COMMIT_SHA}`);
    }

    if (!Array.isArray(document.cases)) {
        errors.push('cases must be an array');
        return errors;
    }

    const ids = new Set();
    for (const [index, caseRecord] of document.cases.entries()) {
        const caseLabel = `case[${index}]`;
        if (!caseRecord || typeof caseRecord !== 'object' || Array.isArray(caseRecord)) {
            errors.push(`${caseLabel} must be an object`);
            continue;
        }

        if (typeof caseRecord.id !== 'string' || caseRecord.id.length === 0) {
            errors.push(`${caseLabel} id must be a non-empty string`);
        } else if (ids.has(caseRecord.id)) {
            errors.push(`duplicate case id: ${caseRecord.id}`);
        } else {
            ids.add(caseRecord.id);
        }

        const evidencePaths = Array.isArray(caseRecord.evidence) ? caseRecord.evidence : [];
        if (!Array.isArray(caseRecord.evidence)) {
            errors.push(`case ${caseRecord.id} evidence must be an array`);
        }

        const sourcePaths = [
            { label: `case ${caseRecord.id} devlog`, path: caseRecord.devlog },
            ...(caseRecord.adr === undefined ? [] : [{ label: `case ${caseRecord.id} adr`, path: caseRecord.adr }]),
            ...evidencePaths.map((entry, evidenceIndex) => ({
                label: `case ${caseRecord.id} evidence[${evidenceIndex}]`,
                path: entry
            })),
            ...collectExcludedPaths(caseRecord)
        ];

        for (const sourcePath of sourcePaths) {
            errors.push(...await validateSourcePath({
                root,
                basisCommitSha: document.basisCommitSha,
                relativePath: sourcePath.path,
                label: sourcePath.label,
                verifyBasisCommit
            }));
        }

        errors.push(...await validateCaseDocument({ root, realRoot, caseRecord }));

        if (!Array.isArray(caseRecord.metrics)) {
            errors.push(`case ${caseRecord.id} metrics must be an array`);
            continue;
        }

        const evidenceContents = await Promise.all(evidencePaths.map(async (evidencePath) => {
            const pathResult = resolveSourcePath(root, evidencePath);
            if (pathResult.error) {
                return '';
            }

            try {
                return await readFile(pathResult.resolvedPath, 'utf8');
            } catch {
                return '';
            }
        }));

        const sourceText = evidenceContents.join('\n');
        for (const [metricIndex, metric] of caseRecord.metrics.entries()) {
            if (!metric || typeof metric.sourceText !== 'string' || metric.sourceText.length === 0) {
                errors.push(`case ${caseRecord.id} metrics[${metricIndex}] sourceText must be a non-empty string`);
            } else if (!sourceText.includes(metric.sourceText)) {
                errors.push(`case ${caseRecord.id} metrics[${metricIndex}] sourceText is absent from evidence: ${metric.sourceText}`);
            }
        }
    }

    return errors;
}

export async function validateEvidence(document, options = {}) {
    try {
        return await validateEvidenceUnchecked(document, options);
    } catch (error) {
        return [`evidence validation failed: ${error.message}`];
    }
}

export function formatValidationErrors(errors) {
    return errors.join('\n');
}

function parseArguments(argumentsList) {
    const options = {};
    for (let index = 0; index < argumentsList.length; index += 2) {
        const name = argumentsList[index];
        const value = argumentsList[index + 1];
        if ((name !== '--root' && name !== '--evidence') || value === undefined) {
            return { error: 'usage: node scripts/portfolio/evidence.mjs --root <path> --evidence <path>' };
        }
        options[name.slice(2)] = value;
    }

    if (!options.root || !options.evidence) {
        return { error: 'usage: node scripts/portfolio/evidence.mjs --root <path> --evidence <path>' };
    }

    return options;
}

async function main() {
    const argumentsResult = parseArguments(process.argv.slice(2));
    if (argumentsResult.error) {
        console.error(argumentsResult.error);
        process.exitCode = 1;
        return;
    }

    const root = path.resolve(argumentsResult.root);
    const evidencePath = path.resolve(root, argumentsResult.evidence);
    let document;
    try {
        document = await loadEvidence(evidencePath);
    } catch (error) {
        console.error(`failed to load evidence: ${error.message}`);
        process.exitCode = 1;
        return;
    }

    const errors = await validateEvidence(document, { root });
    if (errors.length > 0) {
        console.error(formatValidationErrors(errors));
        process.exitCode = 1;
        return;
    }

    for (const caseRecord of document.cases) {
        console.log(caseRecord.id);
    }
}

const isCli = process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url);
if (isCli) {
    await main();
}
