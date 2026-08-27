# 포트폴리오 근거 매트릭스

코드 기준 SHA는 `884e5e70d68d9fcf9dfe5638d97e06623da154c2`이다. 문서 commit이 추가돼도 아래 수치와 원본 경로는 이 기준을 유지한다.

| 사례 | 질문 | 채택 원본 | 기준 SHA | 채택 수치 | 제외 원본 | 남은 한계 |
| --- | --- | --- | --- | --- | --- | --- |
| GPU query pool | 비동기 DX11 timestamp query의 누락을 어떻게 막았는가? | `docs/benchmarks/forward-baseline/20260823-033736-80988ef7-seed20260823/RESULT.md` | `884e5e70d68d9fcf9dfe5638d97e06623da154c2` | GPU P95 `3.885056ms`, draw `2240`, GPU sample `600` | `docs/benchmarks/forward-baseline/20260823-032931-36163138-seed20260823/FAILED.md`: 600개 중 302개 GPU timestamp 누락 | 짧은 렌더 기준선이며 게임 로직, 네트워크, UI, 그림자와 투명 패스는 포함하지 않는다. |
| Hybrid deferred | 동일 조건에서 GPU 시간을 얼마나 줄였는가? | `docs/benchmarks/hybrid-deferred/20260823-145749-54a54e5c-seed20260823/RESULT.md` | `884e5e70d68d9fcf9dfe5638d97e06623da154c2` | GPU total P95 `44.016869%` 감소 | 없음 | prototype mesh 중심의 synthetic stress scene이며 pass별 P95를 합산할 수 없다. |
| Spatial query | Nav grid가 결과를 유지하며 후보 수를 줄이는가? | `docs/benchmarks/spatial-navigation/20260823-182453-5d318dea-seed20260823/RESULT.md` | `884e5e70d68d9fcf9dfe5638d97e06623da154c2` | 후보 `99.694942%` 감소, mismatch `0` | `docs/benchmarks/spatial-navigation/20260823-180321-e1d0ef0f-seed20260823/RESULT.md`: linear와 grid 후보 계수 단위 불일치 | synthetic benchmark 중앙값이며 behavior tree 시간 증가는 별도로 남긴다. |
| Offline match duration | 24명과 중립 AI 100마리 경기가 결정적으로 끝나는가? | `docs/benchmarks/offline-match/20260824-023134-1ede6a23-seed20260823/RESULT.md` | `884e5e70d68d9fcf9dfe5638d97e06623da154c2` | tick `16147`, `538.233s`, repeat mismatch `0`, P95 `0.2292ms` | 없음 | RTX visible play와 network 비용은 이 원본이 검증하지 않으며 Step 외 작업은 측정에서 제외했다. |
| Network replication | interest-delta가 24인 수신량을 줄이고 impairment를 견디는가? | `docs/benchmarks/network-load/20260826-01ae1278-COMPARISON.md` | `884e5e70d68d9fcf9dfe5638d97e06623da154c2` | 평균 수신 `66.216564KiB/s` → `4.123043KiB/s`, drop `12579`, keyframe request `3875`, protocol error `0`, queue overflow `0` | 없음 | WARP 기반이며 Windows와 Linux evidence commit이 다르고 외부 cloud 수치가 아니다. |

## 자동 검증 범위

`scripts/portfolio/evidence.mjs`는 case ID 중복, 기준 SHA 형식, checkout 밖 경로, 현재 checkout과 기준 commit에서의 원본 존재, 그리고 채택 수치의 `sourceText`가 evidence 원본에 남아 있는지를 확인한다. 사례 문서는 아직 뒤 작업에서 만들므로 `caseDocument`는 경로 안전성만 확인한다.

## 자동으로 증명하지 못하는 것

검증기는 수치를 다시 계산하거나 서로 다른 benchmark의 의미를 동등하다고 판정하지 않는다. 예를 들어 GPU P95 감소가 실제 플레이 전체 경험을 개선했다는 주장, offline tick 시간이 network server 비용까지 포함한다는 주장, WARP 결과가 RTX 환경을 대표한다는 주장은 이 계약만으로 증명되지 않는다. 각 사례와 한계 문서는 원본 조건과 해석 범위를 함께 읽어야 한다.
