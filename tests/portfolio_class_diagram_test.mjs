import assert from 'node:assert/strict';
import { access, readFile } from 'node:fs/promises';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import test from 'node:test';

const repositoryRoot = path.resolve(import.meta.dirname, '..');
const basisCommitSha = '884e5e70d68d9fcf9dfe5638d97e06623da154c2';
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

test('class diagrams move to verified only with the four generated outputs and pinned proof', async () => {
    const releaseStatus = JSON.parse(await readFile(
        path.join(repositoryRoot, 'docs/portfolio/release-status.json'),
        'utf8'
    ));
    const item = releaseStatus.items.find((candidate) => candidate.id === 'class-diagrams');
    const expectedEvidence = [
        'docs/diagrams/class/engine.json',
        'docs/diagrams/class/network.json',
        'docs/diagrams/class/engine.html',
        'docs/diagrams/class/network.html'
    ];

    assert.equal(item?.status, 'verified');
    assert.deepEqual(item?.evidence, expectedEvidence);
    assert.equal(item?.proof?.tool, 'clang-uml');
    assert.equal(item?.proof?.toolVersion, clangUmlVersion);
    assert.equal(item?.proof?.basisCommitSha, basisCommitSha);
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
        for (const element of elements) {
            assert.match(firstRender, new RegExp(element.name.replace(/[.*+?^${}()|[\]\\]/gu, '\\$&')));
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
