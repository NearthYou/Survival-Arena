import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';

import { renderDiagram, validateDiagram } from '../scripts/portfolio/render-diagrams.mjs';

const basisCommitSha = '884e5e70d68d9fcf9dfe5638d97e06623da154c2';
const repositoryRoot = path.resolve(import.meta.dirname, '..');
const generatedHtmlPaths = [
    'docs/diagrams/game-start-sequence.html',
    'docs/diagrams/room-lifecycle.html',
    'docs/diagrams/snapshot-data-flow.html',
    'docs/diagrams/system-architecture.html',
    'docs/diagrams/class/engine.html',
    'docs/diagrams/class/network.html',
    'docs/diagrams/index.html'
];
const flowDiagramNames = [
    'system-architecture',
    'room-lifecycle',
    'snapshot-data-flow'
];

function normalizeMarkupText(markup) {
    return markup.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
}

function extractGroupRectangles(markup, groupClass) {
    const pattern = new RegExp(
        `<g class="${groupClass}"[\\s\\S]*?<rect x="([^"]+)" y="([^"]+)" width="([^"]+)" height="([^"]+)"`,
        'g'
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

function createFlowDiagram() {
    return {
        schemaVersion: 1,
        basisCommitSha,
        title: 'Small flow',
        kind: 'flow',
        sources: [{ label: 'Room contract', path: 'apps/lobby_server/include/dxa/lobby/Room.hpp' }],
        nodes: [
            { id: 'waiting', label: 'Waiting', x: 40, y: 40, width: 180, height: 72 },
            { id: 'starting', label: 'Starting', x: 300, y: 40, width: 180, height: 72 }
        ],
        edges: [{ from: 'waiting', to: 'starting', label: 'Host starts a ready room.' }]
    };
}

function createSequenceDiagram() {
    return {
        schemaVersion: 1,
        basisCommitSha,
        title: 'Small sequence',
        kind: 'sequence',
        sources: [{ label: 'Lobby service', path: 'apps/lobby_server/src/LobbyService.cpp' }],
        nodes: [
            { id: 'host', label: 'Host' },
            { id: 'lobby', label: 'Lobby' },
            { id: 'server', label: 'Game server' }
        ],
        steps: [
            { from: 'host', to: 'lobby', label: 'The host requests match start.' },
            { from: 'lobby', to: 'server', label: 'The lobby assigns the room.' }
        ]
    };
}

test('generated HTML is pinned to LF for deterministic public Windows checkout', () => {
    const result = spawnSync(
        'git',
        ['check-attr', 'eol', '--', ...generatedHtmlPaths],
        { cwd: repositoryRoot, encoding: 'utf8' }
    );

    assert.equal(result.status, 0, result.stderr || result.stdout);
    assert.deepEqual(
        result.stdout.trim().split(/\r?\n/u),
        generatedHtmlPaths.map((relativePath) => `${relativePath}: eol: lf`)
    );
});

test('rejects duplicate node IDs', async () => {
    const diagram = createFlowDiagram();
    diagram.nodes.push({ ...diagram.nodes[0] });

    const errors = await validateDiagram(diagram, { root: process.cwd(), verifyBasisCommit: false });

    assert.ok(errors.some((error) => error.includes('duplicate node id')));
});

test('rejects edges with unknown endpoints', async () => {
    const diagram = createFlowDiagram();
    diagram.edges[0].to = 'missing';

    const errors = await validateDiagram(diagram, { root: process.cwd(), verifyBasisCommit: false });

    assert.ok(errors.some((error) => error.includes('unknown edge endpoint')));
});

test('rejects an empty source list', async () => {
    const diagram = createFlowDiagram();
    diagram.sources = [];

    const errors = await validateDiagram(diagram, { root: process.cwd(), verifyBasisCommit: false });

    assert.ok(errors.some((error) => error.includes('sources must not be empty')));
});

test('rejects a source absent from the basis commit', async () => {
    const diagram = createFlowDiagram();
    diagram.sources = [{ label: 'New test', path: 'tests/portfolio_diagram_test.mjs' }];

    const errors = await validateDiagram(diagram, { root: process.cwd() });

    assert.ok(errors.some((error) => error.includes('missing from basis commit')));
});

test('rejects a valid commit SHA that is not the canonical basis commit', async () => {
    const diagram = createFlowDiagram();
    diagram.basisCommitSha = '911bc87d1d38611e4cba44e09713060361e0e800';

    const errors = await validateDiagram(diagram, { root: process.cwd(), verifyBasisCommit: false });

    assert.ok(errors.some((error) => error.includes('canonical basis commit')));
});

test('renders the same sequence diagram to identical bytes', () => {
    const diagram = createSequenceDiagram();

    assert.equal(renderDiagram(diagram), renderDiagram(diagram));
});

test('renders a self-contained accessible HTML document with source evidence', () => {
    const diagram = createFlowDiagram();

    const html = renderDiagram(diagram);

    assert.match(html, /<title>Small flow<\/title>/);
    assert.match(html, new RegExp(basisCommitSha));
    assert.match(html, /<svg\b/);
    assert.match(html, /class="text-fallback"/);
    assert.match(html, /apps\/lobby_server\/include\/dxa\/lobby\/Room\.hpp/);
});

test('keeps full flow responsibilities out of SVG while preserving numbered markers and fallback text', () => {
    const diagram = createFlowDiagram();
    const responsibility = 'The host starts a ready room only after every member has reported ready.';
    diagram.edges[0].label = responsibility;

    const html = renderDiagram(diagram);
    const svg = html.match(/<svg[\s\S]*?<\/svg>/)?.[0] ?? '';
    const svgText = normalizeMarkupText(svg);

    assert.equal(svgText.includes(responsibility), false);
    assert.match(svg, />1<\/tspan>/);
    assert.equal(html.includes(responsibility), true);
});

test('keeps full sequence messages visible in the SVG', () => {
    const diagram = createSequenceDiagram();

    const html = renderDiagram(diagram);
    const svg = html.match(/<svg[\s\S]*?<\/svg>/)?.[0] ?? '';
    const svgText = normalizeMarkupText(svg);

    assert.equal(svgText.includes(diagram.steps[0].label), true);
    assert.equal(svgText.includes(diagram.steps[1].label), true);
});

test('system architecture routes protocol input to simulation state through the game server', async () => {
    const sourcePath = new URL('../docs/diagrams/source/system-architecture.json', import.meta.url);
    const diagram = JSON.parse(await readFile(sourcePath, 'utf8'));

    assert.equal(diagram.edges.some((edge) => edge.from === 'protocol' && edge.to === 'simulation'), false);
    const gameServerBoundary = diagram.edges.find((edge) => edge.from === 'game-server' && edge.to === 'simulation');
    assert.ok(gameServerBoundary);
    assert.match(gameServerBoundary.label, /protocol input/);
    assert.match(gameServerBoundary.label, /simulation state/);
});

for (const diagramName of flowDiagramNames) {
    test(`${diagramName} committed flow output keeps markers clear and responsibilities in fallback`, async () => {
        const sourcePath = new URL(`../docs/diagrams/source/${diagramName}.json`, import.meta.url);
        const outputPath = new URL(`../docs/diagrams/${diagramName}.html`, import.meta.url);
        const [sourceText, html] = await Promise.all([
            readFile(sourcePath, 'utf8'),
            readFile(outputPath, 'utf8')
        ]);
        const diagram = JSON.parse(sourceText);
        const svg = html.match(/<svg[\s\S]*?<\/svg>/)?.[0] ?? '';
        const fallback = html.match(/<section class="text-fallback"[\s\S]*?<\/section>/)?.[0] ?? '';
        const svgText = normalizeMarkupText(svg);
        const fallbackText = normalizeMarkupText(fallback);
        const markers = extractGroupRectangles(svg, 'connection-marker');
        const nodes = extractGroupRectangles(svg, 'node');

        assert.equal(markers.length, diagram.edges.length);
        assert.equal(nodes.length, diagram.nodes.length);
        for (const edge of diagram.edges) {
            assert.equal(svgText.includes(edge.label), false, `${diagramName} exposes a full responsibility in SVG`);
            assert.equal(fallbackText.includes(edge.label), true, `${diagramName} omits a responsibility from fallback`);
        }
        for (let markerIndex = 0; markerIndex < markers.length; markerIndex += 1) {
            for (let otherIndex = markerIndex + 1; otherIndex < markers.length; otherIndex += 1) {
                assert.equal(
                    rectanglesOverlap(markers[markerIndex], markers[otherIndex]),
                    false,
                    `${diagramName} markers ${markerIndex + 1} and ${otherIndex + 1} overlap`
                );
            }
            for (let nodeIndex = 0; nodeIndex < nodes.length; nodeIndex += 1) {
                assert.equal(
                    rectanglesOverlap(markers[markerIndex], nodes[nodeIndex]),
                    false,
                    `${diagramName} marker ${markerIndex + 1} overlaps node ${nodeIndex + 1}`
                );
            }
        }
    });
}

test('does not render network URLs or external scripts and stylesheets', () => {
    const html = renderDiagram(createSequenceDiagram());

    assert.doesNotMatch(html, /https?:\/\//i);
    assert.doesNotMatch(html, /<script\b/i);
    assert.doesNotMatch(html, /<link\b[^>]*rel=["']stylesheet["']/i);
});
