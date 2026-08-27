import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';

import { renderDiagram, validateDiagram } from '../scripts/portfolio/render-diagrams.mjs';

const basisCommitSha = '884e5e70d68d9fcf9dfe5638d97e06623da154c2';

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
    const svgText = svg.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();

    assert.equal(svgText.includes(responsibility), false);
    assert.match(svg, />1<\/tspan>/);
    assert.equal(html.includes(responsibility), true);
});

test('keeps full sequence messages visible in the SVG', () => {
    const diagram = createSequenceDiagram();

    const html = renderDiagram(diagram);
    const svg = html.match(/<svg[\s\S]*?<\/svg>/)?.[0] ?? '';
    const svgText = svg.replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();

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

test('room lifecycle renders non-overlapping numbered flow markers', async () => {
    const sourcePath = new URL('../docs/diagrams/source/room-lifecycle.json', import.meta.url);
    const diagram = JSON.parse(await readFile(sourcePath, 'utf8'));

    const html = renderDiagram(diagram);
    const markers = [...html.matchAll(/<g class="connection-marker"[\s\S]*?<rect x="([^"]+)" y="([^"]+)" width="([^"]+)" height="([^"]+)"/g)]
        .map((match) => ({
            x: Number(match[1]),
            y: Number(match[2]),
            width: Number(match[3]),
            height: Number(match[4])
        }));

    for (let leftIndex = 0; leftIndex < markers.length; leftIndex += 1) {
        for (let rightIndex = leftIndex + 1; rightIndex < markers.length; rightIndex += 1) {
            const left = markers[leftIndex];
            const right = markers[rightIndex];
            const overlaps = left.x < right.x + right.width
                && left.x + left.width > right.x
                && left.y < right.y + right.height
                && left.y + left.height > right.y;
            assert.equal(overlaps, false, `flow markers ${leftIndex + 1} and ${rightIndex + 1} overlap`);
        }
    }
});

test('does not render network URLs or external scripts and stylesheets', () => {
    const html = renderDiagram(createSequenceDiagram());

    assert.doesNotMatch(html, /https?:\/\//i);
    assert.doesNotMatch(html, /<script\b/i);
    assert.doesNotMatch(html, /<link\b[^>]*rel=["']stylesheet["']/i);
});
