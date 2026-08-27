import assert from 'node:assert/strict';
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

test('does not render network URLs or external scripts and stylesheets', () => {
    const html = renderDiagram(createSequenceDiagram());

    assert.doesNotMatch(html, /https?:\/\//i);
    assert.doesNotMatch(html, /<script\b/i);
    assert.doesNotMatch(html, /<link\b[^>]*rel=["']stylesheet["']/i);
});
