# 12주차 포트폴리오 기반 구현 계획

> For agentic workers: REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

Goal: 기존 raw evidence에서 검증 가능한 근거만 연결해 README, 네 개 구조 그림, 다섯 문제 해결 사례와 공개 체크리스트를 만든다.

Architecture: `evidence.json`을 사실 경계로 사용하고 Node 표준 라이브러리만으로 경로, SHA, 원본 문자열과 다이어그램 source를 검증한다. 구조 그림은 JSON에서 inline SVG HTML을 생성하며 class diagram만 실제 clang-uml AST 결과를 요구한다.

Tech Stack: Markdown, JSON, Node.js 24 표준 라이브러리, Git, clang-uml 0.6.3, CMake CTest

Spec: `docs/superpowers/specs/2026-08-27-portfolio-foundation-design.md`

## Global Constraints

- 코드 기준 commit은 `884e5e70d68d9fcf9dfe5638d97e06623da154c2`다.
- 새 성능 수치를 만들거나 기존 raw evidence를 덮어쓰지 않는다.
- 문서는 `docs/WRITING_GUIDE.md`의 상황부터 남은 한계 순서를 따른다.
- 외부 CDN과 새 Node package를 추가하지 않는다.
- 수동 class 목록을 clang-uml AST 결과라고 표현하지 않는다.
- AWS 외부 접속, PDF, 실제 영상, 저장소 공개와 `v0.1.0`은 이번 계획의 완료 범위가 아니다.
- branch와 commit 제목에 `codex`를 넣지 않는다.
- commit은 한국어 명사형 Conventional Commit이며 본문에 이유, 핵심 변경, 검증을 기록한다.

---

### Task 1: 증거 schema와 경로 검증

Files:

- Create: `scripts/portfolio/evidence.mjs`
- Create: `tests/portfolio_evidence_test.mjs`
- Create: `docs/portfolio/evidence.json`
- Create: `docs/portfolio/EVIDENCE_MATRIX.md`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Git checkout, 기준 commit, 기존 devlog, ADR와 benchmark 원본
- Produces: `loadEvidence(path)`, `validateEvidence(document, options)`, `formatValidationErrors(errors)`
- Produces: case ID `gpu-query-pool`, `hybrid-deferred`, `spatial-query`, `offline-match-duration`, `network-replication`

- [ ] Step 1: Write the failing Node test

`tests/portfolio_evidence_test.mjs`는 Node `node:test`와 `assert/strict`만 사용한다. 임시 root와 다음 최소 document를 만든다.

```js
const document = {
  schemaVersion: 1,
  basisCommitSha: '884e5e70d68d9fcf9dfe5638d97e06623da154c2',
  cases: [{
    id: 'sample',
    devlog: 'docs/devlog/sample.md',
    adr: 'docs/adr/sample.md',
    evidence: ['docs/benchmarks/sample/RESULT.md'],
    metrics: [{ name: 'sample_count', value: 600, unit: 'frames', sourceText: '600/600' }],
    limits: ['synthetic fixture'],
    caseDocument: 'docs/portfolio/cases/sample.md'
  }]
};
```

다음 동작을 각각 검사한다.

1. 같은 ID 중복을 거부한다.
2. 40자리 lowercase SHA가 아니면 거부한다.
3. source path가 root 밖으로 나가면 거부한다.
4. 원본 파일이 없으면 거부한다.
5. `sourceText`가 원본에 없으면 거부한다.
6. 모든 파일과 문자열이 있으면 오류 배열이 비어 있다.

- [ ] Step 2: Run RED

Run:

```powershell
node --test tests/portfolio_evidence_test.mjs
```

Expected: `ERR_MODULE_NOT_FOUND` for `scripts/portfolio/evidence.mjs`.

- [ ] Step 3: Implement the validator

`scripts/portfolio/evidence.mjs`는 `fs/promises`, `path`, `url`만 사용한다. `validateEvidence()`는 throw하지 않고 문자열 오류 배열을 반환한다. CLI 실행 시 `--root`, `--evidence`를 받고 오류가 있으면 각 오류를 stderr에 출력한 뒤 exit 1을 반환한다.

경로 검증은 `path.resolve(root, relative)`가 `root + path.sep`로 시작하는지 확인한다. current checkout 존재 검증 뒤 `git cat-file -e <basisSha>:<path>`를 `child_process.spawnSync`로 실행해 기준 commit에도 파일이 있는지 확인한다. `caseDocument`는 이번 task에서 아직 생성되지 않으므로 존재 검사 대상에서 제외하고 경로 안전성만 검사한다.

- [ ] Step 4: Create the real evidence document

Spec의 다섯 사례와 정확한 source path를 `docs/portfolio/evidence.json`에 넣는다. `metrics`의 `sourceText`는 원본 파일에 실제로 있는 짧은 값으로 선택한다. 실패 원본은 `excludedEvidence` 배열로 분리하고 이유를 적는다.

- [ ] Step 5: Create the human-readable matrix

`EVIDENCE_MATRIX.md`는 사례, 질문, 채택 원본, 기준 SHA, 채택 수치, 제외 원본, 남은 한계를 표로 정리한다. 자동 검증 범위와 자동으로 증명하지 못하는 의미 해석을 구분한다.

- [ ] Step 6: Register the Node test when Node exists

`tests/CMakeLists.txt`에서 `find_program(DXA_NODE_EXECUTABLE NAMES node)`를 실행하고 찾았을 때만 다음 CTest를 등록한다.

```cmake
add_test(
    NAME PortfolioEvidence.ValidatesSources
    COMMAND ${DXA_NODE_EXECUTABLE} --test
            "${PROJECT_SOURCE_DIR}/tests/portfolio_evidence_test.mjs"
)
```

- [ ] Step 7: Run GREEN

Run:

```powershell
node --test tests/portfolio_evidence_test.mjs
node scripts/portfolio/evidence.mjs --root . --evidence docs/portfolio/evidence.json
```

Expected: Node tests pass and CLI prints the five validated case IDs with exit 0.

- [ ] Step 8: Commit

```powershell
git add -- scripts/portfolio/evidence.mjs tests/portfolio_evidence_test.mjs docs/portfolio/evidence.json docs/portfolio/EVIDENCE_MATRIX.md tests/CMakeLists.txt
git commit -m "docs(portfolio): 검증 근거 목록 정리" -m "이유: 포트폴리오 수치가 기존 raw evidence와 분리되면 과장이나 링크 drift를 찾기 어렵다." -m "핵심 변경: 다섯 사례의 SHA, 원본, 수치 문자열과 한계를 schema 및 자동 검증으로 연결했다." -m "검증: Node RED 뒤 schema, 경로 탈출, 누락 원본과 source text 검증을 통과했다."
```

---

### Task 2: JSON 기반 구조 다이어그램과 HTML

Files:

- Create: `scripts/portfolio/render-diagrams.mjs`
- Create: `tests/portfolio_diagram_test.mjs`
- Create: `docs/diagrams/source/system-architecture.json`
- Create: `docs/diagrams/source/room-lifecycle.json`
- Create: `docs/diagrams/source/game-start-sequence.json`
- Create: `docs/diagrams/source/snapshot-data-flow.json`
- Generate: `docs/diagrams/system-architecture.html`
- Generate: `docs/diagrams/room-lifecycle.html`
- Generate: `docs/diagrams/game-start-sequence.html`
- Generate: `docs/diagrams/snapshot-data-flow.html`
- Generate: `docs/diagrams/index.html`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Task 1의 기준 SHA와 source path 규칙
- Produces: `validateDiagram(diagram, options)`, `renderDiagram(diagram)`, `renderIndex(entries)`
- Produces: `node scripts/portfolio/render-diagrams.mjs --root . --write`

- [ ] Step 1: Write renderer tests

`tests/portfolio_diagram_test.mjs`는 작은 flow fixture와 sequence fixture를 사용한다. 다음을 검사한다.

1. node ID 중복을 거부한다.
2. 존재하지 않는 edge endpoint를 거부한다.
3. 빈 source 목록을 거부한다.
4. 기준 commit에 없는 source를 거부한다.
5. 같은 입력을 두 번 render하면 byte가 같다.
6. HTML에 title, full basis SHA, inline SVG, text fallback과 source path가 있다.
7. HTML에 `http://`, `https://`, 외부 script 또는 stylesheet가 없다.

- [ ] Step 2: Run RED

Run:

```powershell
node --test tests/portfolio_diagram_test.mjs
```

Expected: `ERR_MODULE_NOT_FOUND` for `scripts/portfolio/render-diagrams.mjs`.

- [ ] Step 3: Implement deterministic SVG rendering

flow diagram은 JSON의 고정 `x`, `y`, `width`, `height`를 사용한다. sequence diagram은 participant 순서와 step 순서로 x, y를 계산한다. renderer는 rounded rectangle, line, arrow marker와 text만 사용한다. 색상은 CSS custom property로 정의하고 light 및 dark 배경 모두에서 대비가 유지되게 한다.

CLI는 source directory의 네 JSON을 이름순으로 읽어 개별 HTML과 index를 쓴다. `--check`에서는 다시 render한 문자열과 committed HTML을 비교하고 다르면 exit 1을 반환한다.

- [ ] Step 4: Write the four source diagrams

각 JSON은 spec의 source path를 그대로 사용한다.

- system architecture: client, engine, lobby client, lobby server, worker control, game server, protocol, simulation
- room lifecycle: Waiting, Starting, InMatch와 create, join, ready, start, worker ready, disconnect 전이
- game start sequence: host, lobby, worker registry, game server, game client
- snapshot data flow: authoritative match, replicator, codec, UDP fragments, reassembler, snapshot stream, predictor, interpolator

source path마다 `label`과 `path`를 둔다. edge 또는 step에는 해당 책임을 한 문장으로 적는다.

- [ ] Step 5: Generate and inspect HTML

Run:

```powershell
node scripts/portfolio/render-diagrams.mjs --root . --write
node scripts/portfolio/render-diagrams.mjs --root . --check
```

브라우저에서 `docs/diagrams/index.html`을 열어 1280px와 768px 폭에서 잘림, 겹침, 읽기 순서와 dark mode를 확인한다. 확인하지 않은 경우 문서에 시각 검증 완료를 적지 않는다.

- [ ] Step 6: Register the test

Node가 있으면 `PortfolioDiagrams.RendersDeterministically` CTest를 추가한다.

- [ ] Step 7: Run GREEN

Run:

```powershell
node --test tests/portfolio_diagram_test.mjs
node scripts/portfolio/render-diagrams.mjs --root . --check
```

Expected: test pass, four source files validated, five HTML files unchanged.

- [ ] Step 8: Commit

```powershell
git add -- scripts/portfolio/render-diagrams.mjs tests/portfolio_diagram_test.mjs docs/diagrams tests/CMakeLists.txt
git commit -m "docs(diagram): 코드 근거 구조 흐름 시각화" -m "이유: 구조 그림이 코드와 분리된 설명이 되지 않도록 기준 SHA와 source path를 원본에 남겨야 했다." -m "핵심 변경: 네 JSON 원본에서 외부 의존성 없는 inline SVG HTML과 index를 생성했다." -m "검증: renderer RED 뒤 schema, source, deterministic output와 브라우저 layout을 확인했다."
```

---

### Task 3: 다섯 문제 해결 사례와 통합 한계

Files:

- Create: `docs/portfolio/cases/01-gpu-query-pool.md`
- Create: `docs/portfolio/cases/02-hybrid-deferred.md`
- Create: `docs/portfolio/cases/03-spatial-query.md`
- Create: `docs/portfolio/cases/04-offline-match-duration.md`
- Create: `docs/portfolio/cases/05-network-replication.md`
- Create: `docs/portfolio/LIMITATIONS.md`
- Modify: `docs/portfolio/evidence.json`
- Modify: `tests/portfolio_evidence_test.mjs`

Interfaces:

- Consumes: Task 1의 case ID, 원본, 채택 수치와 한계
- Produces: evidence의 모든 `caseDocument`에 존재하는 한국어 사례 문서

- [ ] Step 1: Tighten the evidence test

실제 evidence 문서를 load해 모든 `caseDocument`가 존재하고 각 문서에 다음 heading이 정확히 한 번씩 있는지 검사한다.

```text
## 상황
## 재현
## 관찰
## 가설과 비교한 대안
## 선택
## 구현
## 검증
## 남은 한계
```

각 문서는 자신의 devlog, ADR와 evidence path를 Markdown link로 포함해야 한다.

- [ ] Step 2: Run RED

Run:

```powershell
node --test tests/portfolio_evidence_test.mjs
```

Expected: five `caseDocument` files missing.

- [ ] Step 3: Write the five cases

각 문서는 900자에서 1600자 사이로 쓴다. devlog를 요약하되 다음을 지킨다.

- 당시의 실패 또는 기준선을 먼저 쓴다.
- 선택하지 않은 대안과 선택 비용을 한 문단에 남긴다.
- 검증에는 spec의 수치와 원본 링크를 같이 둔다.
- `LIMITATIONS.md`와 겹치는 일반론을 반복하지 않는다.
- 실제로 없던 장애, 감정, 대화와 일정 압박을 만들지 않는다.

- [ ] Step 4: Write integrated limitations

`LIMITATIONS.md`는 graphics, simulation, network, deployment, evidence와 product scope로 나눈다. WARP, synthetic scene, evidence SHA 차이, plaintext transport, worker mutual authentication 부재, AWS 미실행, map과 character 한 종, reconnect 제외를 포함한다.

- [ ] Step 5: Run GREEN and prose checks

Run:

```powershell
node --test tests/portfolio_evidence_test.mjs
node scripts/portfolio/evidence.mjs --root . --evidence docs/portfolio/evidence.json
rg -n "TBD|TODO|나중에 작성|성장할 수|큰 도움이" docs/portfolio
```

Expected: tests and evidence validation pass. `rg` output is empty.

- [ ] Step 6: Commit

```powershell
git add -- docs/portfolio/cases docs/portfolio/LIMITATIONS.md docs/portfolio/evidence.json tests/portfolio_evidence_test.mjs
git commit -m "docs(portfolio): 다섯 문제 해결 사례 정리" -m "이유: 개발 기록의 긴 흐름을 면접에서 원본까지 추적 가능한 질문 단위로 다시 구성해야 했다." -m "핵심 변경: graphics, 공간 탐색, 경기 규칙과 network 사례를 상황부터 한계 순서로 연결했다." -m "검증: 사례 heading, 원본 link, 채택 수치 문자열과 placeholder 부재를 검사했다."
```

---

### Task 4: README와 공개 상태 계약

Files:

- Create: `docs/portfolio/RELEASE_CHECKLIST.md`
- Create: `docs/portfolio/release-status.json`
- Create: `scripts/portfolio/verify-all.mjs`
- Modify: `README.md`
- Modify: `docs/PROJECT_PLAN.md`
- Modify: `tests/portfolio_evidence_test.mjs`

Interfaces:

- Consumes: Task 1에서 Task 3까지의 evidence, diagrams와 case documents
- Produces: 공개 상태 `verified`, `partial`, `missing`, `blocked`
- Produces: `node scripts/portfolio/verify-all.mjs --root .`

- [ ] Step 1: Write release status tests

다음 규칙을 검사한다.

1. 허용하지 않은 status를 거부한다.
2. `verified` 항목은 evidence path가 하나 이상 있어야 한다.
3. PDF, demo video, class diagrams와 repository visibility가 현재 `verified`이면 실패한다.
4. AWS resource 미생성 상태는 cleanup 완료로 표현하지 않는다.
5. README의 local link target이 모두 존재한다.

- [ ] Step 2: Run RED

Run:

```powershell
node --test tests/portfolio_evidence_test.mjs
```

Expected: `release-status.json` 또는 release validator가 없음.

- [ ] Step 3: Create the status JSON and checklist

초기 상태는 다음을 유지한다.

- historical 24-player metrics: `verified`
- historical 30-minute soak: `verified`
- licenses/assets manifest: `verified`
- current HEAD fresh Windows/Linux build: `partial`
- WARP and RTX visual artifacts: `partial`
- four JSON/HTML architecture diagrams: `verified` after Task 2
- clang-uml class diagrams: `missing`
- portfolio PDF: `missing`
- demo video: `missing`
- AWS external test: `blocked`
- repository visibility: `blocked`
- `v0.1.0`: `missing`

`RELEASE_CHECKLIST.md`는 status JSON을 사람이 읽는 순서로 설명하고 완료 조건을 임의로 낮추지 않는다.

- [ ] Step 4: Update README and project status

README 첫 화면에 다음 링크를 추가한다.

- diagrams index
- evidence matrix
- five cases index 또는 portfolio directory
- limitations
- release checklist

기존 실행 명령을 삭제하지 않는다. `PROJECT_PLAN.md`는 11주차 local packaging 완료와 AWS 외부 접속 미실행을 분리하고, 12주차 기반 문서를 진행 중으로 표시한다. GitHub billing 관련 과거 문구는 현재 CI가 성공한 사실과 충돌하지 않게 역사적 설명으로 고친다.

- [ ] Step 5: Implement verify-all

`verify-all.mjs`는 evidence CLI와 diagram `--check`를 함수로 호출하고 release status 및 Markdown local link를 검사한다. 실패 원인을 파일과 field 단위로 출력한다.

- [ ] Step 6: Run GREEN

Run:

```powershell
node --test tests/portfolio_evidence_test.mjs tests/portfolio_diagram_test.mjs
node scripts/portfolio/verify-all.mjs --root .
git diff --check
```

Expected: all checks pass and no whitespace errors.

- [ ] Step 7: Commit

```powershell
git add -- README.md docs/PROJECT_PLAN.md docs/portfolio/RELEASE_CHECKLIST.md docs/portfolio/release-status.json scripts/portfolio/verify-all.mjs tests/portfolio_evidence_test.mjs
git commit -m "docs(release): 공개 준비 상태 명시" -m "이유: 과거 검증과 현재 HEAD, 누락 산출물과 외부 승인을 같은 완료 표현으로 섞지 않아야 했다." -m "핵심 변경: README 진입점, 공개 status 계약과 전체 문서 verifier를 추가했다." -m "검증: status enum, evidence path, local link와 미완료 gate 표현을 자동 검사했다."
```

---

### Task 5: clang-uml AST 클래스 다이어그램

Files:

- Create: `.clang-uml`
- Create: `scripts/portfolio/generate-class-diagrams.ps1`
- Create: `tests/portfolio_class_diagram_test.mjs`
- Generate: `docs/diagrams/class/engine.json`
- Generate: `docs/diagrams/class/network.json`
- Generate: `docs/diagrams/class/engine.html`
- Generate: `docs/diagrams/class/network.html`
- Modify: `docs/portfolio/release-status.json`
- Modify: `docs/diagrams/index.html`

Interfaces:

- Consumes: clang-uml `0.6.3`, current compile database, Task 2 HTML renderer
- Produces: 실제 AST engine 및 network class JSON과 HTML

- [ ] Step 1: Stop at the external tool gate

Run:

```powershell
clang-uml --version
```

Expected in the current environment: command not found. Record the missing tool without changing release status. Ask for approval before running the official Windows installer.

- [ ] Step 2: Verify the official installer

After approval, use the official `0.6.3` release asset and verify SHA-256 before execution.

```text
9e4f3881ac1b003bf587a56e433e440e7e60a28712129ead2af83e41ec2e2886
```

If the hash differs, stop and do not run the file.

- [ ] Step 3: Write config contract test

`portfolio_class_diagram_test.mjs`는 `.clang-uml`에 `engine`과 `network` diagram이 있고 output format에 JSON이 포함되는지 검사한다. 생성된 JSON에는 class가 하나 이상, relationship이 하나 이상, full basis SHA와 clang-uml version metadata가 있어야 한다.

- [ ] Step 4: Run RED

Run:

```powershell
node --test tests/portfolio_class_diagram_test.mjs
```

Expected: config와 generated JSON이 없음.

- [ ] Step 5: Configure class scopes

engine은 `EngineApp`, `RuntimeScene`, renderer와 resource 경계를 포함하고 network는 `LobbyService`, `WorkerRegistry`, `GameServer`, `AuthoritativeMatch`, `GameSession`, replicator와 reassembler를 포함한다. third-party와 test class는 제외한다.

- [ ] Step 6: Generate from the AST

`generate-class-diagrams.ps1`는 clang-uml version, compile database source root와 basis SHA를 확인한 뒤 두 diagram만 생성한다. raw clang-uml JSON은 덮어쓰지 않고 temporary output에서 validation 후 `docs/diagrams/class`로 이동한다.

- [ ] Step 7: Render and verify

Task 2 renderer로 class JSON을 HTML로 변환한다. 브라우저에서 label 겹침과 관계 방향을 확인한다.

Run:

```powershell
node --test tests/portfolio_class_diagram_test.mjs
node scripts/portfolio/verify-all.mjs --root .
```

Expected: class diagram checks pass and release status changes from `missing` to `verified` only after both JSON and HTML checks pass.

- [ ] Step 8: Commit

```powershell
git add -- .clang-uml scripts/portfolio/generate-class-diagrams.ps1 tests/portfolio_class_diagram_test.mjs docs/diagrams/class docs/diagrams/index.html docs/portfolio/release-status.json
git commit -m "docs(diagram): AST 클래스 구조 생성" -m "이유: 수동 class 그림이 실제 코드와 달라지는 문제를 기준 SHA가 있는 AST 결과로 막아야 했다." -m "핵심 변경: clang-uml engine 및 network scope, raw JSON, HTML과 검증 경계를 추가했다." -m "검증: clang-uml version, compile database, class 관계, 기준 SHA와 브라우저 layout을 확인했다."
```

---

### Task 6: 계획 범위 최종 검증

Files:

- Modify: `docs/portfolio/RELEASE_CHECKLIST.md`
- Modify: `docs/PROJECT_PLAN.md`

Interfaces:

- Consumes: Task 1부터 Task 5까지의 검증 결과
- Produces: PDF 및 영상 계획으로 넘길 정확한 open gate 목록

- [ ] Step 1: Run all document tests

```powershell
node --test tests/portfolio_evidence_test.mjs tests/portfolio_diagram_test.mjs tests/portfolio_class_diagram_test.mjs
node scripts/portfolio/verify-all.mjs --root .
```

- [ ] Step 2: Run project regression tests

Run the Windows Debug CTest suite and Linux server build using the repository commands. Record actual counts and commands. A historical result is not a current HEAD result.

- [ ] Step 3: Reconcile status

Only gates proven by Task 1 through Task 5 move to `verified`. PDF, demo, AWS external test, repository visibility and tag remain open.

- [ ] Step 4: Commit the final status

```powershell
git add -- docs/portfolio/RELEASE_CHECKLIST.md docs/PROJECT_PLAN.md
git commit -m "docs(portfolio): 기반 검증 상태 확정" -m "이유: 다음 PDF와 영상 작업이 완료된 항목을 다시 추측하지 않도록 검증 결과와 open gate를 고정해야 했다." -m "핵심 변경: 현재 HEAD 검증과 다음 단계 blocked 항목을 공개 체크리스트에 반영했다." -m "검증: 문서 test, 전체 verifier, Windows CTest와 Linux server build 결과를 대조했다."
```
