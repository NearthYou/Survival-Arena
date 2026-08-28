# 12주차 포트폴리오 기반 설계

## 목표

12주차 전체 범위 중 공개 전에 먼저 고정해야 하는 근거 문서와 구조 그림을 만든다. 기존 코드와 benchmark 원본을 다시 해석하되 새 성능 수치를 만들지 않는다. 산출물은 README, 증거 목록, 네 개 구조 다이어그램, 다섯 문제 해결 사례, 한계와 공개 체크리스트다.

PDF, 실제 데모 영상, AWS 외부 접속, 저장소 공개와 `v0.1.0` 태그는 이 기반 작업 뒤의 별도 실행 계획으로 분리한다. 이 항목들은 누락된 상태를 숨기지 않고 공개 체크리스트에서 `missing` 또는 `blocked`로 유지한다.

## 기준 상태

- 코드 기준 commit: `884e5e70d68d9fcf9dfe5638d97e06623da154c2`
- 기준 branch: `main`에서 분기한 `docs/portfolio-finalization`
- 기준 tree SHA: `a3d167d7ddb3fadfe5ce9a2dfea6f5a58b170890`
- 같은 tree의 Windows Debug CTest: 580개 중 580개 통과
- hosted CI: Windows와 Ubuntu job 통과
- AWS resource: 생성하지 않음
- 저장소 공개 상태: 이 설계에서는 변경하거나 완료로 판단하지 않음

문서 commit이 추가돼도 구조 그림은 위 코드 기준 commit을 계속 표시한다. 문서 작업이 코드 구조를 바꾸지 않았다는 전제다. 이후 코드가 바뀌면 그림의 기준 SHA와 source 검증을 다시 실행해야 한다.

## 작업 분리

### 이번 계획

1. 증거 계약과 자동 검증
2. 시스템 구조, 방 생명주기, 게임 시작, 스냅샷 흐름 그림
3. 다섯 문제 해결 사례와 통합 한계
4. README, 프로젝트 상태와 공개 체크리스트
5. clang-uml 기반 클래스 다이어그램 생성 경계

### 다음 계획

1. 실제 게임 화면과 다이어그램을 사용한 18쪽에서 22쪽 한국어 PDF
2. 3분 내외 실제 로컬 데모 영상
3. 현재 HEAD Windows 및 Linux 재현 검증
4. AWS 외부 접속을 진행한다면 계정, 가격, 보안 그룹과 종료 확인
5. 공개 승인 뒤 저장소 공개와 `v0.1.0` 태그

## 파일 책임

```text
docs/portfolio/
  evidence.json             기존 원본과 채택 수치의 기계 판독 계약
  EVIDENCE_MATRIX.md        사람이 읽는 근거 및 상태 표
  LIMITATIONS.md            현재 한계와 다음 검증
  RELEASE_CHECKLIST.md      공개 승인 상태
  cases/
    01-gpu-query-pool.md
    02-hybrid-deferred.md
    03-spatial-query.md
    04-offline-match-duration.md
    05-network-replication.md

docs/diagrams/
  source/
    system-architecture.json
    room-lifecycle.json
    game-start-sequence.json
    snapshot-data-flow.json
  system-architecture.html
  room-lifecycle.html
  game-start-sequence.html
  snapshot-data-flow.html
  index.html
  class/
    engine.json
    network.json
    engine.html
    network.html

scripts/portfolio/
  evidence.mjs              evidence schema 및 경로 검증
  render-diagrams.mjs       외부 CDN 없는 inline SVG HTML 생성
  verify-all.mjs            공개 전 문서 계약 집계

tests/
  portfolio_evidence_test.mjs
  portfolio_diagram_test.mjs
```

## 증거 계약

`docs/portfolio/evidence.json`의 최상위 필드는 다음과 같다.

```json
{
  "schemaVersion": 1,
  "basisCommitSha": "884e5e70d68d9fcf9dfe5638d97e06623da154c2",
  "cases": []
}
```

각 case는 다음 필드를 가진다.

- `id`: 파일명과 연결되는 고정 ID
- `question`: 당시 해결하려던 한 문장 질문
- `devlog`: 실제 개발 기록 경로
- `adr`: 선택 근거 경로
- `evidence`: 원본 `RESULT.md`, JSON 또는 비교 문서 경로 배열
- `metrics`: 원본에 있는 이름, 값, 단위 배열
- `limits`: 사례를 재사용할 때 반드시 함께 적을 한계 배열
- `caseDocument`: 포트폴리오 사례 문서 경로

검증기는 모든 경로가 현재 checkout과 기준 commit에 존재하는지 확인한다. 수치가 원본과 다른지 자동으로 재계산할 수 없는 경우에는 `sourceText`를 함께 두고 원본 문서에 해당 문자열이 있는지 확인한다. 이 검증은 성능 계산을 다시 구현하지 않는다.

## 채택하는 다섯 사례

### GPU query pool

- devlog: `docs/devlog/2026-08-23-forward-baseline.md`
- evidence: `docs/benchmarks/forward-baseline/20260823-033736-80988ef7-seed20260823/RESULT.md`
- 사실: 600 frame 중 GPU timestamp 302개 누락 문제를 실행별 query slot로 수정
- 채택 수치: GPU P95 `3.885056ms`, draw `2240`, GPU sample `600/600`

### Hybrid deferred

- devlog: `docs/devlog/2026-08-23-hybrid-deferred.md`
- ADR: `docs/adr/0003-hybrid-deferred-rendering.md`
- evidence: `docs/benchmarks/hybrid-deferred/20260823-145749-54a54e5c-seed20260823/RESULT.md`
- 채택 수치: GPU P95 `44.02%` 감소
- 한계: prototype mesh 중심의 synthetic stress scene

### Spatial query

- devlog: `docs/devlog/2026-08-23-spatial-navigation-ai.md`
- ADR: `docs/adr/0004-spatial-navigation-and-behavior-tree.md`
- evidence: `docs/benchmarks/spatial-navigation/20260823-182453-5d318dea-seed20260823/RESULT.md`
- 채택 수치: Nav grid 후보 `99.694942%` 감소, 결과 mismatch `0`
- 제외 원본: 계수 단위 오류가 있던 `20260823-180321-e1d0ef0f`

### Offline match duration

- devlog: `docs/devlog/2026-08-24-offline-match-loop.md`
- ADR: `docs/adr/0005-authoritative-offline-match.md`
- evidence: `docs/benchmarks/offline-match/20260824-023134-1ede6a23-seed20260823/RESULT.md`
- 채택 수치: tick `16147`, 약 `538.2s`, repeat mismatch `0`, Release tick P95 `0.2292ms`
- 한계: 실제 RTX visible play와 network 비용은 이 원본에서 검증하지 않음

### Network replication

- devlog: `docs/devlog/2026-08-25-24-player-network-load.md`
- ADR: `docs/adr/0008-acked-interest-replication.md`
- evidence: `docs/benchmarks/network-load/20260826-01ae1278-COMPARISON.md`
- 채택 수치: 평균 수신량 `66.216564KiB/s`에서 `4.123043KiB/s`
- impairment: drop `12579`, keyframe request `3875`, protocol error `0`, queue overflow `0`
- 한계: WARP 기반, 일부 Windows와 Linux evidence commit 불일치, 외부 cloud 수치 아님

## 다이어그램 계약

네 개 수동 구조 그림은 코드 경계를 해석한 JSON을 원본으로 삼는다. JSON은 `basisCommitSha`, `title`, `kind`, `sources`, `nodes`, `edges` 또는 `steps`를 가진다. source path는 기준 commit에서 존재해야 한다.

HTML은 JSON에서 생성하는 inline SVG다. 외부 CDN, Mermaid runtime, 원격 font와 JavaScript package에 의존하지 않는다. 키보드로 node와 step을 읽을 수 있는 텍스트 목록을 SVG 아래에 함께 둔다.

각 그림의 근거는 다음과 같다.

- 시스템 구조: 각 모듈 `CMakeLists.txt`와 공개 include 경계
- 방 생명주기: `Room.hpp`, `Room.cpp`, `LobbyService.cpp`
- 게임 시작: `LobbyService.cpp`, `GameServer.cpp`, `AuthoritativeMatch.cpp`, `GameSession.cpp`
- 스냅샷 흐름: `AuthoritativeMatch.cpp`, `SnapshotReplicator.cpp`, protocol codec, reassembler, predictor와 interpolator

클래스 다이어그램은 예외다. engine과 network class 그림은 clang-uml이 실제 AST와 compilation database에서 생성한 JSON만 허용한다. 손으로 만든 class 목록을 clang-uml 결과로 표현하지 않는다.

## clang-uml 도구 경계

- pin version: `0.6.3`
- 공식 Windows release asset: `clang-uml-0.6.3-win64.exe`
- 공식 SHA-256: `9e4f3881ac1b003bf587a56e433e440e7e60a28712129ead2af83e41ec2e2886`
- 설치와 실행은 외부 프로그램 실행이므로 사용자 승인 뒤 진행
- compilation database가 현재 기준 commit과 같은 source tree를 가리키는지 검사
- 결과 JSON에 engine과 network source path가 실제로 포함됐는지 검사
- 결과 HTML에는 기준 commit SHA와 clang-uml version을 표시

도구가 없거나 AST parse가 실패하면 class diagram은 `blocked`다. 나머지 네 그림이 통과해도 class diagram 완료로 표시하지 않는다.

## 문서 문체

모든 사례는 `docs/WRITING_GUIDE.md`를 따른다.

1. 상황
2. 재현
3. 관찰
4. 가설과 비교한 대안
5. 선택
6. 구현
7. 검증
8. 남은 한계

실제로 겪지 않은 장애와 대화를 만들지 않는다. 이전 기록의 문장을 복사해 이어 붙이지 않고 당시 순서와 선택을 유지해 다시 쓴다. 좋은 수치만 남기지 않고 실패 원본과 회귀 수치도 연결한다.

## 상태 표현

공개 체크리스트 값은 `verified`, `partial`, `missing`, `blocked`만 사용한다.

- `verified`: 현재 요구를 직접 지지하는 원본이 있음
- `partial`: 역사적 원본은 있으나 현재 HEAD 또는 시각 검증이 아님
- `missing`: 저장소에 산출물이 없음
- `blocked`: 계정, 설치, 결제 또는 외부 승인 없이는 확인할 수 없음

11주차는 local packaging을 완료했지만 AWS 외부 접속은 하지 않았다. 따라서 프로젝트 계획은 11주차 전체를 완료로 바꾸지 않는다. 12주차 문서 작업은 병행해 진행 중으로 표시한다.

## 공개 금지 조건

다음 중 하나라도 남으면 이 계획에서 저장소를 공개하거나 `v0.1.0`을 만들지 않는다.

- class diagram이 실제 clang-uml AST 결과가 아님
- PDF가 18쪽에서 22쪽 범위로 렌더 및 링크 검수되지 않음
- 실제 데모 영상이 없음
- README 명령과 최신 build/test가 일치하지 않음
- 자산 license와 LFS 상태가 검증되지 않음
- cloud resource를 만들었다면 종료 및 잔여 비용 확인이 없음
