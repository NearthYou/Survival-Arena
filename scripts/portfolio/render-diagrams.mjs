import { access, mkdir, readFile, readdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

const SHA_PATTERN = /^[0-9a-f]{40}$/;
const CANONICAL_BASIS_COMMIT_SHA = '884e5e70d68d9fcf9dfe5638d97e06623da154c2';
const EXPECTED_SOURCE_FILES = [
    'game-start-sequence.json',
    'room-lifecycle.json',
    'snapshot-data-flow.json',
    'system-architecture.json'
];
const EXPECTED_CLASS_SOURCE_FILES = ['engine.json', 'network.json'];
const CLASS_DIAGRAM_CONTRACTS = {
    engine: {
        title: `Engine AST class diagram | code basis ${CANONICAL_BASIS_COMMIT_SHA}`,
        minimumClasses: 9,
        minimumRelationships: 5,
        requiredClasses: [
            'AssetSceneRenderer',
            'EngineApp',
            'ForwardRenderer',
            'RuntimeSceneFrame',
            'IRuntimeSceneController',
            'HybridDeferredRenderer',
            'ResourceHandle',
            'ResourcePool',
            'ResourcePool##Slot'
        ],
        sourcePrefixes: ['engine/'],
        translationUnitPrefixes: ['engine/src/'],
        translationUnitFiles: ['tests/engine_resource_pool_test.cpp']
    },
    network: {
        title: `Network AST class diagram | code basis ${CANONICAL_BASIS_COMMIT_SHA}`,
        minimumClasses: 9,
        minimumRelationships: 1,
        requiredClasses: [
            'LobbyService',
            'WorkerRegistry',
            'WorkerControlServer',
            'LobbyTcpServer',
            'SnapshotReplicator',
            'AuthoritativeMatch',
            'GameServer',
            'GameSession',
            'SnapshotReassembler'
        ],
        sourcePrefixes: [
            'apps/lobby_server/',
            'apps/game_server/',
            'apps/game_client/'
        ],
        translationUnitPrefixes: [
            'apps/lobby_server/src/',
            'apps/game_server/src/',
            'apps/game_client/src/'
        ],
        translationUnitFiles: []
    }
};
const CLASS_INDEX_ENTRIES = [
    {
        title: 'Engine AST 클래스 구조',
        file: 'class/engine.html',
        basisCommitSha: CANONICAL_BASIS_COMMIT_SHA
    },
    {
        title: 'Network AST 클래스 구조',
        file: 'class/network.html',
        basisCommitSha: CANONICAL_BASIS_COMMIT_SHA
    }
];

function escapeHtml(value) {
    return String(value)
        .replaceAll('&', '&amp;')
        .replaceAll('<', '&lt;')
        .replaceAll('>', '&gt;')
        .replaceAll('"', '&quot;')
        .replaceAll("'", '&#39;');
}

function renderArrowMarker() {
    return `<defs>
        <marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse">
          <path d="M 0 0 L 10 5 L 0 10 z" class="arrow-head" />
        </marker>
      </defs>`;
}

function wrapLabel(value, maximumCharacters) {
    const words = String(value).split(/\s+/).filter(Boolean);
    const lines = [];
    let line = '';
    for (const word of words) {
        if (word.length > maximumCharacters) {
            if (line) {
                lines.push(line);
                line = '';
            }
            for (let offset = 0; offset < word.length; offset += maximumCharacters) {
                lines.push(word.slice(offset, offset + maximumCharacters));
            }
            continue;
        }
        const candidate = line ? `${line} ${word}` : word;
        if (candidate.length > maximumCharacters) {
            lines.push(line);
            line = word;
        } else {
            line = candidate;
        }
    }
    if (line) {
        lines.push(line);
    }
    return lines.length > 0 ? lines : [''];
}

function renderMultilineText(value, x, y, className, maximumCharacters = 20) {
    const lines = wrapLabel(value, maximumCharacters);
    const lineHeight = 17;
    const firstY = y - ((lines.length - 1) * lineHeight) / 2;
    const spans = lines.map((line, index) => `<tspan x="${x}" y="${firstY + index * lineHeight}">${escapeHtml(line)}</tspan>`).join('');
    return `<text class="${className}">${spans}</text>`;
}

function renderConnectionMarker(number, x, y) {
    return `<g class="connection-marker" aria-hidden="true">
          <rect x="${x - 14}" y="${y - 12}" width="28" height="24" rx="8" />
          ${renderMultilineText(number, x, y + 5, 'connection-marker-label', 3)}
        </g>`;
}

function rectangleConnection(from, to) {
    const fromCenter = { x: from.x + from.width / 2, y: from.y + from.height / 2 };
    const toCenter = { x: to.x + to.width / 2, y: to.y + to.height / 2 };
    const dx = toCenter.x - fromCenter.x;
    const dy = toCenter.y - fromCenter.y;
    const distance = Math.hypot(dx, dy) || 1;
    const unitX = dx / distance;
    const unitY = dy / distance;
    const boundaryDistance = (node) => Math.min(
        unitX === 0 ? Number.POSITIVE_INFINITY : (node.width / 2) / Math.abs(unitX),
        unitY === 0 ? Number.POSITIVE_INFINITY : (node.height / 2) / Math.abs(unitY)
    );

    const fromDistance = boundaryDistance(from);
    const toDistance = boundaryDistance(to);
    return {
        x1: fromCenter.x + unitX * fromDistance,
        y1: fromCenter.y + unitY * fromDistance,
        x2: toCenter.x - unitX * toDistance,
        y2: toCenter.y - unitY * toDistance
    };
}

function renderFlowSvg(diagram) {
    const nodesById = new Map(diagram.nodes.map((node) => [node.id, node]));
    const width = Math.max(640, ...diagram.nodes.map((node) => node.x + node.width + 40));
    const selfEdgeCounts = new Map();
    for (const edge of diagram.edges) {
        if (edge.from === edge.to) {
            selfEdgeCounts.set(edge.from, (selfEdgeCounts.get(edge.from) ?? 0) + 1);
        }
    }
    const selfEdgeBottom = diagram.nodes.map((node) => node.y + node.height + (selfEdgeCounts.get(node.id) ?? 0) * 55 + 70);
    const height = Math.max(320, ...diagram.nodes.map((node) => node.y + node.height + 64), ...selfEdgeBottom);
    const selfEdgeOrdinals = new Map();
    const edges = diagram.edges.map((edge, index) => {
        const fromNode = nodesById.get(edge.from);
        if (edge.from === edge.to) {
            const ordinal = selfEdgeOrdinals.get(edge.from) ?? 0;
            selfEdgeOrdinals.set(edge.from, ordinal + 1);
            const offset = 30 + ordinal * 55;
            const right = fromNode.x + fromNode.width;
            const centerX = fromNode.x + fromNode.width / 2;
            const centerY = fromNode.y + fromNode.height / 2;
            const bottom = fromNode.y + fromNode.height;
            return `      <g class="connection">
        <line x1="${right}" y1="${centerY}" x2="${right + offset}" y2="${centerY}" />
        <line x1="${right + offset}" y1="${centerY}" x2="${right + offset}" y2="${bottom + offset}" />
        <line x1="${right + offset}" y1="${bottom + offset}" x2="${centerX}" y2="${bottom + offset}" />
        <line x1="${centerX}" y1="${bottom + offset}" x2="${centerX}" y2="${bottom}" marker-end="url(#arrow)" />
        ${renderConnectionMarker(index + 1, right + offset, bottom + offset)}
      </g>`;
        }
        const points = rectangleConnection(fromNode, nodesById.get(edge.to));
        const markerFractionFromSource = 0.4;
        const markerX = points.x1 + (points.x2 - points.x1) * markerFractionFromSource;
        const markerY = points.y1 + (points.y2 - points.y1) * markerFractionFromSource - 10;
        return `      <g class="connection">
        <line x1="${points.x1}" y1="${points.y1}" x2="${points.x2}" y2="${points.y2}" marker-end="url(#arrow)" />
        ${renderConnectionMarker(index + 1, markerX, markerY)}
      </g>`;
    }).join('\n');
    const nodes = diagram.nodes.map((node) => `      <g class="node">
        <rect x="${node.x}" y="${node.y}" width="${node.width}" height="${node.height}" rx="14" />
        ${renderMultilineText(node.label, node.x + node.width / 2, node.y + node.height / 2 + 5, 'node-label', 18)}
      </g>`).join('\n');

    return `<svg viewBox="0 0 ${width} ${height}" role="img" aria-labelledby="diagram-svg-title diagram-svg-description">
      <title id="diagram-svg-title">${escapeHtml(diagram.title)}</title>
      <desc id="diagram-svg-description">코드 경계를 번호 화살표로 연결하며 번호별 전체 책임은 아래 텍스트 목록에 있다.</desc>
      ${renderArrowMarker()}
${edges}
${nodes}
    </svg>`;
}

function renderSequenceSvg(diagram) {
    const margin = 100;
    const spacing = 220;
    const firstStepY = 146;
    const stepSpacing = 94;
    const width = Math.max(760, margin * 2 + (diagram.nodes.length - 1) * spacing);
    const height = Math.max(360, firstStepY + diagram.steps.length * stepSpacing + 36);
    const positions = new Map(diagram.nodes.map((node, index) => [node.id, margin + index * spacing]));
    const participants = diagram.nodes.map((node) => {
        const x = positions.get(node.id);
        return `      <g class="participant">
        <rect x="${x - 80}" y="32" width="160" height="56" rx="14" />
        ${renderMultilineText(node.label, x, 65, 'participant-label', 16)}
        <line x1="${x}" y1="88" x2="${x}" y2="${height - 28}" class="lifeline" />
      </g>`;
    }).join('\n');
    const steps = diagram.steps.map((step, index) => {
        const x1 = positions.get(step.from);
        const x2 = positions.get(step.to);
        const y = firstStepY + index * stepSpacing;
        return `      <g class="connection">
        <line x1="${x1}" y1="${y}" x2="${x2}" y2="${y}" marker-end="url(#arrow)" />
        ${renderMultilineText(`${index + 1}. ${step.label}`, (x1 + x2) / 2, y - 18, 'connection-label', 22)}
      </g>`;
    }).join('\n');

    return `<svg viewBox="0 0 ${width} ${height}" role="img" aria-labelledby="diagram-svg-title diagram-svg-description">
      <title id="diagram-svg-title">${escapeHtml(diagram.title)}</title>
      <desc id="diagram-svg-description">참여자 사이의 호출 순서를 위에서 아래로 읽는 시퀀스 다이어그램</desc>
      ${renderArrowMarker()}
${participants}
${steps}
    </svg>`;
}

function renderTextFallback(diagram) {
    const nodeItems = diagram.nodes.map((node) => `<li tabindex="0"><strong>${escapeHtml(node.label)}</strong> <code>${escapeHtml(node.id)}</code></li>`).join('\n          ');
    const connections = diagram.kind === 'sequence' ? diagram.steps : diagram.edges;
    const connectionItems = connections.map((connection, index) => {
        const prefix = diagram.kind === 'sequence' ? `${index + 1}. ` : '';
        return `<li tabindex="0">${prefix}<code>${escapeHtml(connection.from)}</code> → <code>${escapeHtml(connection.to)}</code>: ${escapeHtml(connection.label)}</li>`;
    }).join('\n          ');

    return `<section class="text-fallback" aria-labelledby="text-fallback-title">
      <h2 id="text-fallback-title">텍스트 설명</h2>
      <h3>구성 요소</h3>
      <ul>
          ${nodeItems}
      </ul>
      <h3>${diagram.kind === 'sequence' ? '호출 순서' : '연결 책임'}</h3>
      <ol>
          ${connectionItems}
      </ol>
    </section>`;
}

export function renderDiagram(diagram) {
    const svg = diagram.kind === 'sequence' ? renderSequenceSvg(diagram) : renderFlowSvg(diagram);
    const sources = diagram.sources.map((source) => `<li><span>${escapeHtml(source.label)}</span><code>${escapeHtml(source.path)}</code></li>`).join('\n          ');
    return `<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>${escapeHtml(diagram.title)}</title>
  <style>
    :root { color-scheme: light dark; --page: #f6f8fb; --panel: #ffffff; --ink: #172033; --muted: #53627a; --line: #335eea; --node: #e9efff; --node-stroke: #2449bd; --focus: #d33a7c; }
    @media (prefers-color-scheme: dark) { :root { --page: #111827; --panel: #192235; --ink: #f5f7ff; --muted: #b9c5d8; --line: #8eabff; --node: #26375d; --node-stroke: #a9bdff; --focus: #ff80b6; } }
    * { box-sizing: border-box; }
    body { margin: 0; background: var(--page); color: var(--ink); font-family: system-ui, sans-serif; line-height: 1.55; }
    main { width: min(1180px, calc(100% - 32px)); margin: 0 auto; padding: 40px 0 64px; }
    h1 { margin: 0 0 8px; font-size: clamp(1.7rem, 4vw, 2.5rem); }
    h2 { margin-top: 0; }
    .basis { color: var(--muted); overflow-wrap: anywhere; }
    .diagram-frame, .text-fallback, .sources { margin-top: 24px; padding: clamp(16px, 3vw, 28px); border: 1px solid color-mix(in srgb, var(--muted) 35%, transparent); border-radius: 18px; background: var(--panel); }
    .diagram-frame { overflow-x: auto; }
    svg { display: block; width: 100%; min-width: 900px; height: auto; color: var(--ink); }
    .node rect, .participant rect { fill: var(--node); stroke: var(--node-stroke); stroke-width: 2; }
    .node text, .participant text { fill: var(--ink); text-anchor: middle; font-size: 17px; font-weight: 700; }
    .connection line { stroke: var(--line); stroke-width: 2.5; }
    .arrow-head { fill: var(--line); }
    .connection-label { fill: var(--ink); stroke: var(--panel); stroke-width: 5px; paint-order: stroke; text-anchor: middle; font-size: 14px; font-weight: 600; }
    .connection-marker rect { fill: var(--panel); stroke: var(--line); stroke-width: 2; }
    .connection-marker text { fill: var(--ink); text-anchor: middle; font-size: 13px; font-weight: 800; }
    .lifeline { stroke: var(--muted); stroke-width: 1.5; stroke-dasharray: 7 7; }
    li { margin: 8px 0; }
    li:focus { outline: 3px solid var(--focus); outline-offset: 3px; }
    code { overflow-wrap: anywhere; }
    .sources ul { padding-left: 20px; }
    .sources li { display: grid; gap: 2px; }
    @media (max-width: 800px) { main { width: min(100% - 20px, 1180px); padding-top: 24px; } .diagram-frame, .text-fallback, .sources { padding: 16px; } }
  </style>
</head>
<body>
  <main>
    <h1>${escapeHtml(diagram.title)}</h1>
    <p class="basis">코드 기준 commit: <code>${escapeHtml(diagram.basisCommitSha)}</code></p>
    <div class="diagram-frame">
      ${svg}
    </div>
    ${renderTextFallback(diagram)}
    <section class="sources" aria-labelledby="sources-title">
      <h2 id="sources-title">코드 근거</h2>
      <ul>
          ${sources}
      </ul>
    </section>
  </main>
</body>
</html>
`;
}

export async function validateDiagram(diagram, options = {}) {
    const errors = [];
    const ids = new Set();
    const root = path.resolve(options.root ?? process.cwd());
    const verifyBasisCommit = options.verifyBasisCommit ?? true;

    if (!diagram || typeof diagram !== 'object' || Array.isArray(diagram)) {
        return ['diagram must be an object'];
    }
    if (diagram.schemaVersion !== 1) {
        errors.push('schemaVersion must be 1');
    }
    if (!SHA_PATTERN.test(diagram.basisCommitSha ?? '')) {
        errors.push('basisCommitSha must be a 40-character lowercase hexadecimal SHA');
    } else if (diagram.basisCommitSha !== CANONICAL_BASIS_COMMIT_SHA) {
        errors.push(`basisCommitSha must match canonical basis commit: ${CANONICAL_BASIS_COMMIT_SHA}`);
    }
    if (typeof diagram.title !== 'string' || diagram.title.length === 0) {
        errors.push('title must be a non-empty string');
    }
    if (diagram.kind !== 'flow' && diagram.kind !== 'sequence') {
        errors.push('kind must be flow or sequence');
    }

    if (!Array.isArray(diagram?.sources) || diagram.sources.length === 0) {
        errors.push('sources must not be empty');
    } else {
        for (const [index, source] of diagram.sources.entries()) {
            const label = source?.label ?? `source[${index}]`;
            const relativePath = source?.path;
            if (typeof source?.label !== 'string' || source.label.length === 0) {
                errors.push(`source[${index}] label must be a non-empty string`);
            }
            if (typeof relativePath !== 'string' || relativePath.length === 0) {
                errors.push(`${label}: source path must be a non-empty string`);
                continue;
            }

            const resolvedPath = path.resolve(root, relativePath);
            if (!resolvedPath.startsWith(`${root}${path.sep}`)) {
                errors.push(`${label}: source path escapes root: ${relativePath}`);
                continue;
            }

            try {
                await access(resolvedPath);
            } catch {
                errors.push(`${label}: source file is missing from current checkout: ${relativePath}`);
                continue;
            }

            if (verifyBasisCommit) {
                const result = spawnSync('git', ['cat-file', '-e', `${diagram.basisCommitSha}:${relativePath}`], {
                    cwd: root,
                    encoding: 'utf8'
                });
                if (result.status !== 0) {
                    errors.push(`${label}: source file is missing from basis commit: ${relativePath}`);
                }
            }
        }
    }

    if (!Array.isArray(diagram.nodes) || diagram.nodes.length === 0) {
        errors.push('nodes must not be empty');
    }
    for (const [index, node] of (diagram.nodes ?? []).entries()) {
        if (typeof node?.id !== 'string' || node.id.length === 0) {
            errors.push(`node[${index}] id must be a non-empty string`);
            continue;
        }
        if (typeof node.label !== 'string' || node.label.length === 0) {
            errors.push(`node ${node.id} label must be a non-empty string`);
        }
        if (diagram.kind === 'flow' && !['x', 'y', 'width', 'height'].every((field) => Number.isFinite(node[field]))) {
            errors.push(`flow node ${node.id} requires finite x, y, width and height`);
        } else if (diagram.kind === 'flow' && (node.width <= 0 || node.height <= 0)) {
            errors.push(`flow node ${node.id} requires positive width and height`);
        }
        if (ids.has(node.id)) {
            errors.push(`duplicate node id: ${node.id}`);
        }
        ids.add(node.id);
    }

    const connectionName = diagram.kind === 'sequence' ? 'step' : 'edge';
    const connections = diagram.kind === 'sequence' ? diagram.steps : diagram.edges;
    if (!Array.isArray(connections) || connections.length === 0) {
        errors.push(`${connectionName}s must not be empty`);
    }
    for (const [index, connection] of (connections ?? []).entries()) {
        if (!ids.has(connection?.from) || !ids.has(connection?.to)) {
            errors.push(`${connectionName}[${index}] has an unknown ${connectionName} endpoint`);
        }
        if (typeof connection?.label !== 'string' || connection.label.length === 0) {
            errors.push(`${connectionName}[${index}] label must be a non-empty string`);
        }
    }

    return errors;
}

function flattenClassElements(elements) {
    const flattened = [];
    for (const element of elements ?? []) {
        if (Array.isArray(element?.elements)) {
            flattened.push(...flattenClassElements(element.elements));
        } else {
            flattened.push(element);
        }
    }
    return flattened;
}

function classDiagramTitle(diagram) {
    return diagram.name === 'engine' ? 'Engine AST 클래스 구조' : 'Network AST 클래스 구조';
}

function displayLlvmVersion(value) {
    return String(value).replace(/\s+\(https?:\/\/[^)]+\)/gu, '');
}

function classNodeDisplayName(element) {
    const displayName = typeof element.display_name === 'string' && element.display_name.length > 0
        ? element.display_name
        : element.name;
    const namespacePrefix = element.namespace ? `${element.namespace}::` : '';
    return namespacePrefix && displayName.startsWith(namespacePrefix)
        ? displayName.slice(namespacePrefix.length)
        : displayName;
}

function renderClassSvg(diagram) {
    const classes = flattenClassElements(diagram.elements).filter((element) => element.type === 'class');
    const columns = Math.min(3, classes.length);
    const rows = Math.ceil(classes.length / columns);
    const margin = 52;
    const cardWidth = 258;
    const cardHeight = 122;
    const horizontalGap = 132;
    const verticalGap = 78;
    const width = margin * 2 + columns * cardWidth + (columns - 1) * horizontalGap;
    const height = margin * 2 + rows * cardHeight + (rows - 1) * verticalGap;
    const positions = new Map(classes.map((element, index) => [element.id, {
        x: margin + (index % columns) * (cardWidth + horizontalGap),
        y: margin + Math.floor(index / columns) * (cardHeight + verticalGap),
        width: cardWidth,
        height: cardHeight
    }]));
    const reciprocalPairs = new Set(diagram.relationships
        .filter((relationship) => diagram.relationships.some((candidate) => (
            candidate.source === relationship.destination
            && candidate.destination === relationship.source
        )))
        .map((relationship) => [relationship.source, relationship.destination].sort().join(':')));

    const relationships = diagram.relationships.map((relationship) => {
        const from = positions.get(relationship.source);
        const to = positions.get(relationship.destination);
        const points = rectangleConnection(from, to);
        const pairKey = [relationship.source, relationship.destination].sort().join(':');
        const hasReciprocal = reciprocalPairs.has(pairKey);
        const dx = points.x2 - points.x1;
        const dy = points.y2 - points.y1;
        const distance = Math.hypot(dx, dy) || 1;
        const offset = hasReciprocal ? 18 : 0;
        const offsetX = (-dy / distance) * offset;
        const offsetY = (dx / distance) * offset;
        const x1 = points.x1 + offsetX;
        const y1 = points.y1 + offsetY;
        const x2 = points.x2 + offsetX;
        const y2 = points.y2 + offsetY;
        const labelX = (x1 + x2) / 2;
        const labelY = (y1 + y2) / 2 - 8;
        const labelWidth = Math.max(86, relationship.type.length * 8 + 20);
        return `      <g class="class-relationship" data-source="${escapeHtml(relationship.source)}" data-destination="${escapeHtml(relationship.destination)}">
        <path d="M ${x1} ${y1} L ${x2} ${y2}" marker-end="url(#arrow)" />
        <rect x="${labelX - labelWidth / 2}" y="${labelY - 14}" width="${labelWidth}" height="24" rx="8" />
        <text x="${labelX}" y="${labelY + 3}">${escapeHtml(relationship.type)}</text>
      </g>`;
    }).join('\n');
    const nodes = classes.map((element) => {
        const position = positions.get(element.id);
        const namespaceLabel = element.namespace || 'global namespace';
        const counts = `${element.members.length} members / ${element.methods.length} methods`;
        const sourceLabel = path.basename(element.source_location.file);
        return `      <g class="class-node" data-class-id="${escapeHtml(element.id)}">
        <rect x="${position.x}" y="${position.y}" width="${position.width}" height="${position.height}" rx="14" />
        ${renderMultilineText(classNodeDisplayName(element), position.x + position.width / 2, position.y + 28, 'class-name', 28)}
        ${renderMultilineText(namespaceLabel, position.x + position.width / 2, position.y + 58, 'class-namespace', 34)}
        ${renderMultilineText(counts, position.x + position.width / 2, position.y + 83, 'class-counts', 34)}
        ${renderMultilineText(sourceLabel, position.x + position.width / 2, position.y + 106, 'class-source', 34)}
      </g>`;
    }).join('\n');

    return `<svg viewBox="0 0 ${width} ${height}" role="img" aria-labelledby="class-svg-title class-svg-description">
      <title id="class-svg-title">${escapeHtml(classDiagramTitle(diagram))}</title>
      <desc id="class-svg-description">clang-uml AST에서 추출한 클래스와 방향성 관계를 읽는 다이어그램</desc>
      ${renderArrowMarker()}
${relationships}
${nodes}
    </svg>`;
}

function renderClassTextFallback(diagram) {
    const classes = flattenClassElements(diagram.elements).filter((element) => element.type === 'class');
    const classById = new Map(classes.map((element) => [element.id, element]));
    const classItems = classes.map((element) => `<li tabindex="0">
            <strong>${escapeHtml(element.display_name)}</strong>
            <span>${element.members.length} members / ${element.methods.length} methods</span>
            <code>${escapeHtml(element.source_location.file)}</code>
          </li>`).join('\n          ');
    const relationshipItems = diagram.relationships.map((relationship) => {
        const source = classById.get(relationship.source);
        const destination = classById.get(relationship.destination);
        return `<li tabindex="0"><code>${escapeHtml(source.display_name)}</code> → <code>${escapeHtml(destination.display_name)}</code>: ${escapeHtml(relationship.type)}</li>`;
    }).join('\n          ');

    return `<section class="text-fallback" aria-labelledby="class-text-title">
      <h2 id="class-text-title">AST 텍스트 설명</h2>
      <h3>클래스</h3>
      <ul class="class-list">
          ${classItems}
      </ul>
      <h3>관계 방향</h3>
      <ol>
          ${relationshipItems}
      </ol>
    </section>`;
}

export function renderClassDiagram(diagram) {
    const classes = flattenClassElements(diagram.elements).filter((element) => element.type === 'class');
    const uniqueSources = [...new Set(classes.map((element) => element.source_location.file))];
    const sources = uniqueSources.map((source) => `<li><code>${escapeHtml(source)}</code></li>`).join('\n          ');
    return `<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>${escapeHtml(classDiagramTitle(diagram))}</title>
  <style>
    :root { color-scheme: light dark; --page: #f6f8fb; --panel: #ffffff; --ink: #172033; --muted: #53627a; --line: #335eea; --node: #e9efff; --node-stroke: #2449bd; --focus: #d33a7c; }
    @media (prefers-color-scheme: dark) { :root { --page: #111827; --panel: #192235; --ink: #f5f7ff; --muted: #b9c5d8; --line: #8eabff; --node: #26375d; --node-stroke: #a9bdff; --focus: #ff80b6; } }
    * { box-sizing: border-box; }
    body { margin: 0; background: var(--page); color: var(--ink); font-family: system-ui, sans-serif; line-height: 1.55; }
    main { width: min(1180px, calc(100% - 32px)); margin: 0 auto; padding: 40px 0 64px; }
    h1 { margin: 0 0 8px; font-size: clamp(1.7rem, 4vw, 2.5rem); }
    h2 { margin-top: 0; }
    .basis { color: var(--muted); overflow-wrap: anywhere; }
    .diagram-frame, .text-fallback, .sources { margin-top: 24px; padding: clamp(16px, 3vw, 28px); border: 1px solid color-mix(in srgb, var(--muted) 35%, transparent); border-radius: 18px; background: var(--panel); }
    .diagram-frame { overflow-x: auto; }
    svg { display: block; width: 100%; min-width: 1040px; height: auto; color: var(--ink); }
    .class-node rect { fill: var(--node); stroke: var(--node-stroke); stroke-width: 2; }
    .class-node text { fill: var(--ink); text-anchor: middle; }
    .class-name { font-size: 16px; font-weight: 800; }
    .class-namespace, .class-counts, .class-source { fill: var(--muted) !important; font-size: 12px; }
    .class-relationship path { fill: none; stroke: var(--line); stroke-width: 2.5; }
    .class-relationship rect { fill: var(--panel); stroke: var(--line); stroke-width: 1.5; }
    .class-relationship text { fill: var(--ink); text-anchor: middle; font-size: 12px; font-weight: 700; }
    .arrow-head { fill: var(--line); }
    li { margin: 8px 0; }
    li:focus { outline: 3px solid var(--focus); outline-offset: 3px; }
    code { overflow-wrap: anywhere; }
    .class-list { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(100%, 280px), 1fr)); gap: 12px; padding: 0; list-style: none; }
    .class-list li { display: grid; gap: 3px; padding: 12px; border: 1px solid color-mix(in srgb, var(--muted) 25%, transparent); border-radius: 12px; }
    .sources ul { padding-left: 20px; }
    @media (max-width: 800px) { main { width: min(100% - 20px, 1180px); padding-top: 24px; } .diagram-frame, .text-fallback, .sources { padding: 16px; } }
  </style>
</head>
<body>
  <main>
    <h1>${escapeHtml(classDiagramTitle(diagram))}</h1>
    <p class="basis">코드 기준 commit: <code>${CANONICAL_BASIS_COMMIT_SHA}</code></p>
    <p class="basis">생성 도구: <code>clang-uml ${escapeHtml(diagram.metadata.clang_uml_version)}</code> / ${escapeHtml(displayLlvmVersion(diagram.metadata.llvm_version))}</p>
    <div class="diagram-frame">
      ${renderClassSvg(diagram)}
    </div>
    ${renderClassTextFallback(diagram)}
    <section class="sources" aria-labelledby="class-sources-title">
      <h2 id="class-sources-title">AST source</h2>
      <ul>
          ${sources}
      </ul>
    </section>
  </main>
</body>
</html>
`;
}

export async function validateClassDiagram(diagram, options = {}) {
    const errors = [];
    const root = path.resolve(options.root ?? process.cwd());
    const verifyBasisCommit = options.verifyBasisCommit ?? true;
    if (!diagram || typeof diagram !== 'object' || Array.isArray(diagram)) {
        return ['class diagram must be an object'];
    }
    const contract = CLASS_DIAGRAM_CONTRACTS[diagram.name];
    if (diagram.diagram_type !== 'class') {
        errors.push('diagram_type must be class');
    }
    if (!contract || !EXPECTED_CLASS_SOURCE_FILES.includes(`${diagram.name}.json`)) {
        errors.push('name must be engine or network');
    }
    if (diagram.package_type !== 'namespace') {
        errors.push('package_type must be namespace');
    }
    if (!diagram.metadata || typeof diagram.metadata !== 'object' || Array.isArray(diagram.metadata)) {
        errors.push('metadata must be a clang-uml metadata object');
    }
    if (diagram.metadata?.clang_uml_version !== '0.6.3') {
        errors.push('metadata.clang_uml_version must be 0.6.3');
    }
    if (diagram.metadata?.schema_version !== 3) {
        errors.push('metadata.schema_version must be clang-uml JSON schema 3');
    }
    if (typeof diagram.metadata?.llvm_version !== 'string'
        || !/^clang version 22\.1\.8\b/u.test(diagram.metadata.llvm_version)) {
        errors.push('metadata.llvm_version must identify clang 22.1.8');
    }
    if (!contract || diagram.title !== contract.title) {
        errors.push(`title must exactly identify ${diagram.name ?? 'the diagram'} and the canonical basis commit`);
    }
    for (const field of ['schemaVersion', 'basisCommitSha', 'nodes', 'edges']) {
        if (Object.hasOwn(diagram, field)) {
            errors.push(`${field} is a transformed field; raw clang-uml JSON is required`);
        }
    }

    if (!Array.isArray(diagram.elements)) {
        errors.push('elements must be a clang-uml array');
    } else if (diagram.elements.some((element) => element?.type !== 'class')) {
        errors.push('elements must contain only raw clang-uml class elements');
    }
    const classes = flattenClassElements(diagram.elements).filter((element) => element?.type === 'class');
    const ids = new Set();
    const names = new Set(classes.map((element) => element?.name));
    if (contract && classes.length < contract.minimumClasses) {
        errors.push(`elements must contain minimum ${contract.minimumClasses} classes for ${diagram.name}`);
    } else if (classes.length === 0) {
        errors.push('elements must contain at least one class');
    }
    for (const requiredClass of contract?.requiredClasses ?? []) {
        if (!names.has(requiredClass)) {
            errors.push(`missing required class: ${requiredClass}`);
        }
    }
    for (const [index, element] of classes.entries()) {
        if (typeof element.id !== 'string' || !/^[0-9]+$/u.test(element.id) || ids.has(element.id)) {
            errors.push(`class[${index}] requires a unique numeric clang-uml id`);
        } else {
            ids.add(element.id);
        }
        if (typeof element.name !== 'string' || element.name.length === 0
            || typeof element.display_name !== 'string' || element.display_name.length === 0
            || typeof element.namespace !== 'string') {
            errors.push(`class[${index}] must retain clang-uml name, display_name and namespace`);
        }
        for (const field of ['is_abstract', 'is_nested', 'is_struct', 'is_template', 'is_union']) {
            if (typeof element[field] !== 'boolean') {
                errors.push(`class[${index}].${field} must be a clang-uml boolean`);
            }
        }
        for (const field of ['bases', 'members', 'methods', 'template_parameters']) {
            if (!Array.isArray(element[field])) {
                errors.push(`class[${index}].${field} must be a clang-uml array`);
            }
        }
        if (!element.source_location || typeof element.source_location !== 'object') {
            errors.push(`class[${index}] requires clang-uml source_location`);
        }
        const sourcePath = String(element.source_location?.file ?? '').replaceAll('\\', '/');
        const translationUnit = String(element.source_location?.translation_unit ?? '').replaceAll('\\', '/');
        if (!sourcePath || path.isAbsolute(sourcePath) || /^(?:tests?|third_party)\//iu.test(sourcePath)) {
            errors.push(`class[${index}] requires a production repository-relative source path without test or third-party leakage`);
            continue;
        }
        if (contract && !contract.sourcePrefixes.some((prefix) => sourcePath.startsWith(prefix))) {
            errors.push(`class[${index}] source is outside the allowed source prefix for ${diagram.name}: ${sourcePath}`);
            continue;
        }
        if (!Number.isInteger(element.source_location?.line) || element.source_location.line <= 0
            || !Number.isInteger(element.source_location?.column) || element.source_location.column <= 0
            || !translationUnit
            || (contract
                && !contract.translationUnitPrefixes.some((prefix) => translationUnit.startsWith(prefix))
                && !contract.translationUnitFiles.includes(translationUnit))) {
            errors.push(`class[${index}] requires a valid diagram-specific clang-uml translation unit and location`);
        }
        const resolvedPath = path.resolve(root, sourcePath);
        if (!resolvedPath.startsWith(`${root}${path.sep}`)) {
            errors.push(`class[${index}] source path escapes root: ${sourcePath}`);
            continue;
        }
        try {
            await access(resolvedPath);
        } catch {
            errors.push(`class[${index}] source is missing from current checkout: ${sourcePath}`);
            continue;
        }
        if (translationUnit) {
            const resolvedTranslationUnit = path.resolve(root, translationUnit);
            if (!resolvedTranslationUnit.startsWith(`${root}${path.sep}`)) {
                errors.push(`class[${index}] translation unit escapes root: ${translationUnit}`);
            } else {
                try {
                    await access(resolvedTranslationUnit);
                } catch {
                    errors.push(`class[${index}] translation unit is missing from current checkout: ${translationUnit}`);
                }
            }
        }
        if (verifyBasisCommit) {
            const result = spawnSync('git', ['cat-file', '-e', `${CANONICAL_BASIS_COMMIT_SHA}:${sourcePath}`], {
                cwd: root,
                encoding: 'utf8'
            });
            if (result.status !== 0) {
                errors.push(`class[${index}] source is missing from basis commit: ${sourcePath}`);
            }
            if (translationUnit) {
                const translationUnitResult = spawnSync(
                    'git',
                    ['cat-file', '-e', `${CANONICAL_BASIS_COMMIT_SHA}:${translationUnit}`],
                    { cwd: root, encoding: 'utf8' }
                );
                if (translationUnitResult.status !== 0) {
                    errors.push(`class[${index}] translation unit is missing from basis commit: ${translationUnit}`);
                }
            }
        }
    }

    if (!Array.isArray(diagram.relationships)) {
        errors.push('relationships must be a clang-uml array');
    } else if (contract && diagram.relationships.length < contract.minimumRelationships) {
        errors.push(`relationships must contain minimum ${contract.minimumRelationships} entries for ${diagram.name}`);
    } else if (diagram.relationships.length === 0) {
        errors.push('relationships must not be empty');
    }
    for (const [index, relationship] of (diagram.relationships ?? []).entries()) {
        if (!ids.has(relationship?.source) || !ids.has(relationship?.destination)) {
            errors.push(`relationship[${index}] has an unknown endpoint`);
        }
        if (typeof relationship?.type !== 'string' || relationship.type.length === 0) {
            errors.push(`relationship[${index}] type must be a non-empty string`);
        }
        if (relationship?.source === relationship?.destination) {
            errors.push(`relationship[${index}] must not be a self relationship`);
        }
    }
    return errors;
}

export function renderIndex(entries) {
    const combinedEntries = [...entries];
    for (const classEntry of CLASS_INDEX_ENTRIES) {
        if (!combinedEntries.some((entry) => entry.file === classEntry.file)) {
            combinedEntries.push(classEntry);
        }
    }
    const orderedEntries = combinedEntries.sort((left, right) => left.file.localeCompare(right.file, 'en'));
    const cards = orderedEntries.map((entry) => `      <li>
        <a href="${escapeHtml(entry.file)}">${escapeHtml(entry.title)}</a>
        <code>${escapeHtml(entry.basisCommitSha)}</code>
      </li>`).join('\n');
    return `<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>코드 근거 구조 다이어그램</title>
  <style>
    :root { color-scheme: light dark; --page: #f6f8fb; --panel: #ffffff; --ink: #172033; --muted: #53627a; --accent: #2449bd; --focus: #d33a7c; }
    @media (prefers-color-scheme: dark) { :root { --page: #111827; --panel: #192235; --ink: #f5f7ff; --muted: #b9c5d8; --accent: #a9bdff; --focus: #ff80b6; } }
    * { box-sizing: border-box; }
    body { margin: 0; background: var(--page); color: var(--ink); font-family: system-ui, sans-serif; line-height: 1.55; }
    main { width: min(960px, calc(100% - 32px)); margin: 0 auto; padding: 48px 0 72px; }
    h1 { margin: 0; font-size: clamp(1.8rem, 5vw, 2.8rem); }
    p { color: var(--muted); }
    ul { display: grid; grid-template-columns: repeat(auto-fit, minmax(min(100%, 300px), 1fr)); gap: 18px; padding: 0; margin-top: 32px; list-style: none; }
    li { display: grid; gap: 12px; min-width: 0; padding: 24px; border: 1px solid color-mix(in srgb, var(--muted) 35%, transparent); border-radius: 18px; background: var(--panel); }
    a { color: var(--accent); font-size: 1.2rem; font-weight: 750; }
    a:focus { outline: 3px solid var(--focus); outline-offset: 4px; }
    code { color: var(--muted); overflow-wrap: anywhere; }
  </style>
</head>
<body>
  <main>
    <h1>코드 근거 구조 다이어그램</h1>
    <p>JSON 원본과 기준 commit의 source path를 검증한 네 개 구조 그림과 clang-uml AST 클래스 그림이다.</p>
    <ul>
${cards}
    </ul>
  </main>
</body>
</html>
`;
}

function parseArguments(argumentsList) {
    let root;
    let mode;
    for (let index = 0; index < argumentsList.length; index += 1) {
        const argument = argumentsList[index];
        if (argument === '--root' && argumentsList[index + 1] !== undefined) {
            root = argumentsList[index + 1];
            index += 1;
        } else if ((argument === '--write' || argument === '--check') && mode === undefined) {
            mode = argument.slice(2);
        } else {
            return { error: 'usage: node scripts/portfolio/render-diagrams.mjs --root <path> (--write|--check)' };
        }
    }
    if (!root || !mode) {
        return { error: 'usage: node scripts/portfolio/render-diagrams.mjs --root <path> (--write|--check)' };
    }
    return { root: path.resolve(root), mode };
}

async function writeOrCheck(filePath, expected, mode) {
    if (mode === 'write') {
        await mkdir(path.dirname(filePath), { recursive: true });
        await writeFile(filePath, expected, 'utf8');
        return null;
    }
    try {
        const actual = await readFile(filePath, 'utf8');
        return actual === expected ? null : `generated HTML is stale: ${filePath}`;
    } catch (error) {
        return `generated HTML is missing: ${filePath} (${error.message})`;
    }
}

async function main() {
    const options = parseArguments(process.argv.slice(2));
    if (options.error) {
        console.error(options.error);
        process.exitCode = 1;
        return;
    }

    const sourceDirectory = path.join(options.root, 'docs', 'diagrams', 'source');
    let sourceFiles;
    try {
        sourceFiles = (await readdir(sourceDirectory))
            .filter((file) => file.endsWith('.json'))
            .sort();
    } catch (error) {
        console.error(`failed to read diagram sources: ${error.message}`);
        process.exitCode = 1;
        return;
    }

    if (sourceFiles.length !== EXPECTED_SOURCE_FILES.length
        || sourceFiles.some((file, index) => file !== EXPECTED_SOURCE_FILES[index])) {
        console.error(`expected diagram source files: ${EXPECTED_SOURCE_FILES.join(', ')}`);
        process.exitCode = 1;
        return;
    }

    const entries = [];
    const outputErrors = [];
    for (const sourceFile of sourceFiles) {
        let diagram;
        try {
            diagram = JSON.parse(await readFile(path.join(sourceDirectory, sourceFile), 'utf8'));
        } catch (error) {
            console.error(`${sourceFile}: failed to load JSON: ${error.message}`);
            process.exitCode = 1;
            return;
        }
        const validationErrors = await validateDiagram(diagram, { root: options.root });
        if (validationErrors.length > 0) {
            console.error(`${sourceFile}:\n${validationErrors.join('\n')}`);
            process.exitCode = 1;
            return;
        }

        const outputFile = sourceFile.replace(/\.json$/, '.html');
        const outputPath = path.join(options.root, 'docs', 'diagrams', outputFile);
        const outputError = await writeOrCheck(outputPath, renderDiagram(diagram), options.mode);
        if (outputError) {
            outputErrors.push(outputError);
        }
        entries.push({ title: diagram.title, file: outputFile, basisCommitSha: diagram.basisCommitSha });
        console.log(`validated ${sourceFile}`);
    }

    const classSourceDirectory = path.join(options.root, 'docs', 'diagrams', 'class');
    let classSourceFiles;
    try {
        classSourceFiles = (await readdir(classSourceDirectory))
            .filter((file) => file.endsWith('.json'))
            .sort();
    } catch (error) {
        console.error(`failed to read class diagram sources: ${error.message}`);
        process.exitCode = 1;
        return;
    }
    if (classSourceFiles.length !== EXPECTED_CLASS_SOURCE_FILES.length
        || classSourceFiles.some((file, index) => file !== EXPECTED_CLASS_SOURCE_FILES[index])) {
        console.error(`expected class diagram source files: ${EXPECTED_CLASS_SOURCE_FILES.join(', ')}`);
        process.exitCode = 1;
        return;
    }
    for (const sourceFile of classSourceFiles) {
        let diagram;
        try {
            diagram = JSON.parse(await readFile(path.join(classSourceDirectory, sourceFile), 'utf8'));
        } catch (error) {
            console.error(`${sourceFile}: failed to load class JSON: ${error.message}`);
            process.exitCode = 1;
            return;
        }
        const validationErrors = await validateClassDiagram(diagram, { root: options.root });
        if (validationErrors.length > 0) {
            console.error(`${sourceFile}:\n${validationErrors.join('\n')}`);
            process.exitCode = 1;
            return;
        }
        const outputFile = sourceFile.replace(/\.json$/u, '.html');
        const outputError = await writeOrCheck(
            path.join(classSourceDirectory, outputFile),
            renderClassDiagram(diagram),
            options.mode
        );
        if (outputError) {
            outputErrors.push(outputError);
        }
        console.log(`validated class/${sourceFile}`);
    }

    const indexPath = path.join(options.root, 'docs', 'diagrams', 'index.html');
    const indexError = await writeOrCheck(indexPath, renderIndex(entries), options.mode);
    if (indexError) {
        outputErrors.push(indexError);
    }
    if (outputErrors.length > 0) {
        console.error(outputErrors.join('\n'));
        process.exitCode = 1;
        return;
    }
    console.log(options.mode === 'write' ? 'wrote seven HTML files' : 'seven HTML files unchanged');
}

const isCli = process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url);
if (isCli) {
    await main();
}
