# 2,240 draw를 줄였는데 lighting pass가 가장 비쌌다

## 상황

4주차 포워드 장면은 이미 GPU P95 3.885056ms였다. 목표인 16.7ms보다 훨씬 낮았기 때문에 디퍼드 렌더링을 넣고 빨라졌다고 말하기 쉬운 조건은 아니었다. 이번 작업에서 확인할 질문은 두 가지였다.

첫째, 정적 객체 1,000개를 개별 draw하지 않으면 CPU와 draw call이 실제로 줄어드는가. 둘째, 그림자와 투명 pass까지 추가한 뒤에도 GPU 전체 시간이 줄어드는가.

포워드 코드는 그대로 두고 새 렌더러를 만들었다. 같은 seed와 camera path를 쓰지 않으면 비교하지 않기로 했다.

## 재현

비교 대상은 4주차의 성공 원본이다.

```powershell
./scripts/run_benchmark.ps1
```

- commit: `80988ef71f21430fc6baabea7b3d5cb4beae71f0`
- adapter: NVIDIA GeForce RTX 3050 Ti Laptop GPU
- 해상도: 1920×1080
- seed: `20260823`
- 준비 120프레임, 측정 600프레임
- 객체 1,124개, 동적 광원 32개

5주차 경로는 깨끗한 commit에서 다음 명령으로 실행했다.

```powershell
./scripts/run_benchmark.ps1 `
  -RenderPath hybrid-deferred `
  -OutputRoot docs/benchmarks/hybrid-deferred
```

비교 파일은 seed, 해상도, adapter, warmup과 measured frame이 모두 같고 GPU sample이 하나도 빠지지 않았을 때만 생성했다.

```powershell
./scripts/compare_benchmarks.ps1 `
  -ForwardRun docs/benchmarks/forward-baseline/20260823-033736-80988ef7-seed20260823 `
  -HybridRun docs/benchmarks/hybrid-deferred/20260823-145749-54a54e5c-seed20260823
```

## 관찰

G-Buffer와 lighting만 연결한 첫 WARP 수직 기능은 화면을 만들었지만 최적화는 전혀 되지 않은 상태였다. 최적화 assertion을 추가했을 때 실제 값은 다음과 같았다.

- shadow draw calls: 1,124, 기대값 125
- culled objects: 0
- G-Buffer draw calls: 2,240, 기대값 2,240 미만
- transparent draw calls: 0, 기대값 1

그림자까지 추가했는데 정적 객체를 다시 1,000번 제출하고 있었다. 디퍼드로 바꾼 사실만으로 draw call은 줄지 않았다.

## 가설과 비교한 대안

정적 객체의 mesh와 material은 같고 world matrix만 다르다. world matrix 네 row를 instance vertex data로 보내면 mesh part마다 한 번의 `DrawIndexedInstanced`로 묶을 수 있다. 반면 캐릭터는 animation 시작점이 달라 palette도 다르다. 캐릭터까지 같은 변경에 묶으면 GPU skinning buffer 구조를 다시 설계해야 했다.

따라서 정적 객체만 먼저 instancing하고 캐릭터는 camera frustum을 통과한 경우에만 기존 draw를 유지했다. shadow는 camera가 아니라 방향광 기준이므로 정적 객체 1,000개를 전부 한 instanced draw로 보냈다.

G-Buffer에 world position을 한 장 더 저장하는 방법도 검토했다. lighting pass는 단순해지지만 render target bandwidth가 늘어난다. depth를 SRV로 만들고 inverse view-projection으로 복원하는 쪽을 선택했다.

## 선택

렌더 순서는 shadow, G-Buffer, deferred lighting, transparent forward로 고정했다. opaque와 light 계산만 deferred에 넣고 축소 구역 marker는 마지막 forward pass로 남겼다.

정적 instance buffer는 최대 1,000개 크기로 한 번 만든 뒤 `WRITE_DISCARD`로 갱신한다. shadow 직전에는 전체 matrix를 올리고 G-Buffer 직전에는 frustum을 통과한 matrix만 다시 올린다. marker 64개도 별도 재사용 buffer 한 장을 쓴다.

## 구현

G-Buffer는 다음 세 정보를 가진다.

- RT0: albedo RGB, roughness A
- RT1: octahedral normal 2채널
- depth: `R24G8_TYPELESS` texture의 DSV와 SRV

lighting pass는 `SV_VertexID`로 full-screen triangle을 만들고 depth에서 world position을 복원한다. 방향광 shadow는 2048×2048 `R32_TYPELESS` texture에 기록한 뒤 comparison sampler로 3×3 PCF를 적용한다.

G-Buffer와 shadow texture를 SRV로 읽은 뒤에는 전부 null로 해제하고 camera depth를 transparent DSV로 다시 묶었다. 이 경계를 놓치면 같은 resource를 읽기와 쓰기에 동시에 묶는 DX11 hazard가 생길 수 있다. WARP test에서 debug layer의 corruption과 error message를 직접 수집해 0건인지 확인했다.

GPU timer에는 frame 시작과 끝 외에 네 pass 종료 timestamp를 추가했다. marker 순서가 shadow, G-Buffer, lighting, transparent와 다르면 결과 해석을 거부한다. runner는 total만 600개여도 특정 pass가 빠지면 실패한다.

## 검증

측정 전 Debug CTest 93개, Windows Release build, Linux GCC 13 `-Werror` 검사를 통과했다. 기준 commit은 `54a54e5cac60cbc2f8d025d0500f0b7ac70be57e`다.

RTX 실행에서 query 진단은 시작 거부 0, 정상 해석 600, disjoint 폐기 0, 종료 미해석 0이었다. total과 네 pass sample count도 각각 600이었다.

| 항목 | Forward | Hybrid | 변화 |
| --- | ---: | ---: | ---: |
| CPU frame P95 | 3.9227ms | 2.1695ms | -44.693706% |
| GPU total P95 | 3.885056ms | 2.174976ms | -44.016869% |
| Draw calls P50 | 2,240 | 1,148 | -48.75% |
| Working set P95 | 238,190,592B | 166,449,152B | -30.119342% |

| Hybrid pass | P95 | Draw calls P50 |
| --- | ---: | ---: |
| Shadow | 0.733184ms | 125 |
| G-Buffer | 0.800768ms | 1,021 |
| Lighting | 0.970752ms | 1 |
| Transparent | 0.003072ms | 1 |

visible object는 최소 975개, P50 1,009개, 최대 1,079개였다. culled object P50은 114개였다. 카메라 경로에 따라 캐릭터 개별 draw 수가 달라져 전체 draw call도 frame마다 변한다.

결과는 네 지표 모두 감소했다. 그림자와 투명 pass를 추가하고도 GPU P95가 44.02% 줄었으므로 이번 변경을 실제 최적화 사례로 채택했다. pass별 P95는 서로 다른 frame의 값일 수 있어 합산하지 않았다.

원본 CSV와 실행 환경은 [20260823-145749 실행](../benchmarks/hybrid-deferred/20260823-145749-54a54e5c-seed20260823/RESULT.md)에 있다.

## 리뷰 뒤 구조 정리

실측 뒤 merge 전 리뷰에서 `HybridDeferredRenderer.cpp`가 1,318줄까지 커졌고 포워드 렌더러와 GPU model upload를 각각 구현한 점을 다시 정리했다. model asset, vertex와 index buffer, material texture 생성은 `GpuSceneModel` 한 곳으로 옮겼다. shadow, G-Buffer, transparent draw 함수는 별도 pass 구현 파일로 분리해 renderer 본체는 893줄이 됐다.

정적 객체 world matrix는 초기화 때 한 번 만들고, visible matrix와 marker matrix용 CPU vector는 capacity를 유지한 채 재사용한다. 기존에는 `Render`를 호출할 때마다 세 vector를 새로 만들고 정적 matrix 1,000개를 다시 계산했다.

이 정리는 출력 순서와 draw 통계를 바꾸지 않았고 WARP, 전체 CTest 94개, MSVC Release, RTX readback으로 확인했다. 위 성능표는 구조 정리 전 측정 commit `54a54e5`의 원본이며, 후속 정리에서 더 좋아졌다고 다시 해석하지 않는다.

## 남은 한계

lighting P95 0.970752ms가 네 pass 중 가장 컸다. 현재는 모든 pixel이 포인트 광원 32개를 순회한다. 광원 수를 늘리는 실험 전에는 tiled 또는 clustered culling이 필요한지 다시 측정해야 한다.

working set 감소는 driver와 residency 영향을 받으므로 G-Buffer가 메모리를 절약했다고 결론내리지 않는다. G-Buffer와 shadow map 자체는 포워드보다 더 많은 GPU resource를 소유한다.

같은 prototype mesh 1,000개로 만든 스트레스 장면이며 실제 맵의 material 다양성과 overdraw를 대변하지 않는다. 전투, AI, UI, 네트워크 비용도 아직 없다. 6주차부터 simulation 부하가 들어오면 CPU frame 수치를 새 기준으로 다시 보되 이번 원본을 덮어쓰지 않는다.
