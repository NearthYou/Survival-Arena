import assert from 'node:assert/strict';
import { mkdtemp, mkdir, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';

import { validateEvidence } from '../scripts/portfolio/evidence.mjs';

const basisCommitSha = '884e5e70d68d9fcf9dfe5638d97e06623da154c2';

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
