import assert from 'node:assert/strict';
import { mkdtemp, mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

import { validateEvidence } from '../scripts/portfolio/evidence.mjs';

const basisCommitSha = '884e5e70d68d9fcf9dfe5638d97e06623da154c2';
const repositoryRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
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
