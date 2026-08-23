# ADR 0003: 포워드 기준선과 하이브리드 디퍼드 경로를 분리함

- 상태: 채택
- 날짜: 2026-08-23

## 상황

4주차 포워드 기준선은 1920×1080에서 GPU P95 3.885056ms로 목표인 16.7ms 안에 들어왔다. 성능이 부족해서 렌더 경로를 바꿔야 하는 상황은 아니었다. 다만 객체 1,124개가 매 프레임 2,240번의 draw를 발생시켰고, 각 pixel에서 동적 광원 32개를 순회했다. 이후 그림자와 축소 구역 표시가 더해지면 어떤 비용이 늘었는지 포워드 전체 시간만으로는 구분하기 어렵다.

5주차 목적은 디퍼드 렌더링 자체를 넣는 데 있지 않았다. 포워드 기준선을 그대로 보존하면서 G-Buffer, 조명, 그림자, 투명 pass를 따로 측정하고 동일한 장면에서 선택의 효과를 확인하는 것이 목적이었다.

## 결정

`AssetSceneRenderer`는 포워드 기준선으로 남기고 `HybridDeferredRenderer`를 별도로 둔다. CLI의 `--render-path`가 둘 중 하나만 초기화한다.

하이브리드 경로는 다음 순서로 실행한다.

1. 방향광 shadow depth
2. 불투명 객체 G-Buffer
3. 전체 화면 deferred lighting
4. 축소 구역 marker transparent forward

G-Buffer RT0에는 albedo와 roughness, RT1에는 2채널 octahedral normal을 기록한다. depth는 typeless texture에 쓰고 SRV로 다시 읽는다. world position용 render target은 추가하지 않고 depth와 inverse view-projection matrix로 복원한다.

shadow map은 `R32_TYPELESS` texture에 `D32_FLOAT` DSV와 `R32_FLOAT` SRV를 만든다. 조명 pass에서는 comparison sampler와 3×3 PCF로 방향광 가시성을 계산한다. 투명 marker는 deferred pass에 넣지 않고 alpha blend와 depth write off 상태로 마지막에 그린다.

정적 객체 1,000개는 재사용하는 dynamic instance buffer로 묶는다. camera frustum과 보수적인 bounding sphere가 겹치는 정적 객체와 캐릭터만 G-Buffer에 보낸다. shadow에서는 정적 객체 전체를 한 번의 instanced draw로 보내고, 서로 다른 animation palette를 쓰는 캐릭터 124개는 개별 draw를 유지한다.

GPU query는 frame 전체와 네 pass 끝에 timestamp를 남긴다. frame마다 기다리지 않는 기존 규칙은 유지한다.

## 비교한 대안

포워드 렌더러에 그림자와 인스턴싱만 더하는 방법은 변경량이 적다. 하지만 광원 비용과 geometry 비용을 분리하기 어렵고 이후 광원 수가 늘 때 비교 기준이 부족하다. 4주차 원본을 같은 코드 경로에서 계속 수정하게 되는 점도 피하고 싶었다.

world position을 별도 G-Buffer에 저장하면 lighting shader는 단순해진다. 대신 1920×1080에서 매 frame 읽고 쓰는 render target이 하나 더 생긴다. 이미 depth가 필요하므로 inverse view-projection 계산 한 번과 pixel별 복원을 선택했다.

캐릭터까지 한 번에 instancing하려면 instance마다 animation palette offset을 전달하고 palette buffer 배치를 바꿔야 한다. 이번 비교의 질문은 정적 1,000개 제출 비용이었기 때문에 범위를 넓히지 않았다.

## 결과

깨끗한 commit `54a54e5cac60cbc2f8d025d0500f0b7ac70be57e`에서 RTX 3050 Ti, 1920×1080, seed `20260823`, 준비 120프레임, 측정 600프레임으로 실행했다. GPU timestamp는 total과 네 pass 모두 600개였고 누락은 없었다.

| 항목 | Forward | Hybrid | 변화 |
| --- | ---: | ---: | ---: |
| CPU frame P95 | 3.9227ms | 2.1695ms | -44.693706% |
| GPU total P95 | 3.885056ms | 2.174976ms | -44.016869% |
| Draw calls P50 | 2,240 | 1,148 | -48.75% |
| Working set P95 | 238,190,592B | 166,449,152B | -30.119342% |

Hybrid pass P95는 shadow 0.733184ms, G-Buffer 0.800768ms, lighting 0.970752ms, transparent 0.003072ms였다. 각 pass P95는 서로 다른 frame에서 나올 수 있으므로 합계를 GPU total P95로 사용하지 않는다.

같은 장면에서 CPU, GPU, draw call, working set이 모두 줄었기 때문에 하이브리드 경로를 5주차 최적화 사례로 채택한다. 포워드 경로와 4주차 원본은 회귀 기준으로 계속 보존한다.

## 남은 비용과 한계

하이브리드 경로는 G-Buffer 두 장, sampleable depth, 2048×2048 shadow map을 추가로 소유한다. working set 측정은 감소했지만 이 값은 driver의 자원 residency와 실행 시점 영향을 받으므로 render target 메모리 절감으로 해석하지 않는다.

lighting이 네 pass 중 P95가 가장 컸다. 동적 광원 32개를 full-screen pixel마다 순회하기 때문에 광원 수가 더 늘면 tiled 또는 clustered light culling을 다시 검토해야 한다.

정적 객체는 한 종류의 prototype mesh를 반복했다. 캐릭터 animation palette instancing, occlusion culling, shadow frustum culling은 구현하지 않았다. 시각 검수는 RTX 출력 확인과 DX11 debug error 0건까지 수행했지만 이미지 차이를 수치화한 golden image 검사는 없다.

측정 원본은 [hybrid 실행](../benchmarks/hybrid-deferred/20260823-145749-54a54e5c-seed20260823/RESULT.md), 비교 대상은 [forward 실행](../benchmarks/forward-baseline/20260823-033736-80988ef7-seed20260823/RESULT.md)에 둔다.
