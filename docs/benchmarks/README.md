# 벤치마크 원본 관리

포워드와 하이브리드 측정은 `scripts/run_benchmark.ps1`로 실행한다. 이 스크립트는 깨끗한 commit만 허용하며 기존 실행 디렉터리를 덮어쓰지 않는다.

```powershell
.\scripts\run_benchmark.ps1
.\scripts\run_benchmark.ps1 -RenderPath hybrid-deferred
```

기본 조건은 1920×1080, seed `20260823`, 준비 120프레임, 측정 600프레임이다. 장면에는 캐릭터 24명, AI 100개, 정적 객체 1,000개, 동적 포인트 광원 32개가 들어간다. 포워드는 인스턴싱과 컬링이 없는 기준선이고, 하이브리드는 shadow, G-Buffer, deferred lighting, transparent pass와 정적 인스턴싱, camera frustum culling을 사용한다.

각 실행 디렉터리는 다음 파일을 가진다.

- `frames.csv`: 프레임별 CPU와 GPU 시간, pass별 GPU 시간과 draw call, visibility, 삼각형, 객체 수, working set
- `summary.json`: 최근접 순위 방식의 P50, P95, P99, render path와 실행 인자
- `environment.json`: commit SHA, 빌드 구성, OS, CPU, GPU, 드라이버와 결과 검증 상태

하이브리드 결과는 네 pass timestamp가 측정 frame 수와 모두 일치해야 통과한다. total timestamp만 채워지고 특정 pass가 비어도 runner가 실패한다.

CPU 시간은 프레임 clear 직전부터 vsync 없는 present 반환까지 측정한다. GPU total은 장면 제출 전후, pass 시간은 shadow, G-Buffer, lighting, transparent 종료 지점의 timestamp query로 측정한다. 실행 중에는 query 결과를 기다리지 않고 오래된 슬롯만 확인하며, disjoint이거나 제한 시간 안에 해석되지 않은 값은 빈 셀로 남긴다.

GPU 누락이나 adapter 불일치로 실행이 실패해도 `environment.json`을 먼저 저장한다. configure와 build가 끝난 뒤에는 Git HEAD와 작업 트리를 다시 확인해 처음 기록한 commit과 다른 binary를 실행하지 않는다.

Debug 또는 WARP 결과는 동작 검증일 뿐 포트폴리오 수치로 사용하지 않는다. 기준선 원본을 수정하거나 좋은 프레임만 골라내지 않는다.

두 run을 비교할 때는 다음 스크립트를 사용한다.

```powershell
.\scripts\compare_benchmarks.ps1 `
  -ForwardRun docs/benchmarks/forward-baseline/20260823-033736-80988ef7-seed20260823 `
  -HybridRun docs/benchmarks/hybrid-deferred/20260823-145749-54a54e5c-seed20260823
```

seed, 해상도, adapter, warmup frame, measured frame 중 하나라도 다르면 `comparison.json`을 만들지 않는다. GPU sample이나 hybrid pass sample이 하나라도 누락된 run도 거부한다. schema 1은 포워드 원본으로만, schema 2 hybrid는 `render_path`가 `hybrid-deferred`일 때만 받는다.

현재 채택한 원본은 다음과 같다.

- [Forward 기준선](forward-baseline/20260823-033736-80988ef7-seed20260823/RESULT.md)
- [Hybrid deferred](hybrid-deferred/20260823-145749-54a54e5c-seed20260823/RESULT.md)

## 공간 탐색과 AI

플랫폼 중립 simulation 비교는 별도 Release 실행 파일로 측정한다.

```powershell
./scripts/run_simulation_benchmark.ps1
```

기본 workload는 8,192 triangle NavMesh point query 100,000건, 공간 객체 1,124개의 AABB와 picking query 각 20,000건, AI decision 100,000건이다. 전수 탐색과 가속 경로를 같은 입력에 실행해 mismatch가 0인지 먼저 확인한다. 불일치가 있으면 시간 측정과 출력 디렉터리 생성을 시작하지 않는다.

각 case는 1회 준비 뒤 5회 측정한다. `samples.csv`에 모든 시간 sample을, `result.json`에 중앙값, 후보 또는 bounds 검사 수, checksum을 기록한다. `environment.json`에는 clean commit, CPU, compiler와 runner 검증 결과를 둔다.

현재 채택한 원본은 [20260823-182453 실행](spatial-navigation/20260823-182453-5d318dea-seed20260823/RESULT.md)이다. behavior tree는 FSM보다 느렸으므로 성능 최적화 결과로 분류하지 않는다. 앞선 `20260823-180321` 실행은 grid 후보 계수 단위가 linear와 달라 비채택 원본으로 보존한다.
