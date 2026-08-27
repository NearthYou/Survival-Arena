# 이미 빠른 포워드 기준선에서 렌더 경로를 비교한 방법

## 상황

포워드 장면은 GPU P95 `3.885056ms`로 이미 16.7ms 목표 안이었다. 같은 seed와 카메라 경로에서 그림자와 투명 패스를 추가한 뒤에도 전체 GPU 시간과 2,240회 draw가 줄어드는지를 질문으로 잡았다.

## 재현

RTX 3050 Ti, 1920×1080, seed `20260823`, 준비 120프레임과 측정 600프레임을 고정했다. 포워드와 commit `54a54e5c`의 hybrid 실행은 해상도, adapter, 객체 1,124개와 GPU 표본 수가 모두 같을 때만 비교했다. 명령은 [개발 기록](../../devlog/2026-08-23-hybrid-deferred.md)에 있다.

## 관찰

첫 WARP 수직 기능은 화면은 만들었지만 shadow draw 1,124회, culled object 0개, G-Buffer draw 2,240회였다. 그림자에서도 정적 객체 1,000개를 개별 제출해 경로만 바뀌었을 뿐 draw는 줄지 않았다.

## 가설과 비교한 대안

정적 객체는 world matrix만 달라 instancing할 수 있었다. 캐릭터까지 묶으려면 animation palette 배치를 바꿔야 했다. world position target을 추가하면 lighting은 단순하지만 bandwidth가 늘어난다. 정적 객체만 묶고 depth에서 위치를 복원하는 대신 캐릭터 draw와 복원 연산을 남겼다.

## 선택

기존 포워드 경로는 보존하고 hybrid를 shadow, G-Buffer, lighting, transparent forward 순으로 고정했다. 투명 marker는 마지막 패스에 남겼다. 선택과 자원 비용은 [렌더링 ADR](../../adr/0003-hybrid-deferred-rendering.md)에 기록했다.

## 구현

정적 객체와 marker는 재사용 instance buffer로 묶었다. G-Buffer에는 albedo, roughness와 octahedral normal을 쓰고 depth에서 위치를 복원했다. 패스 뒤 SRV를 해제해 DX11 hazard를 막고, 전체와 네 패스 timestamp가 모두 있어야 결과를 해석했다.

## 검증

[채택 원본](../../benchmarks/hybrid-deferred/20260823-145749-54a54e5c-seed20260823/RESULT.md)은 전체와 네 패스 표본이 모두 600개였다. GPU total P95는 `3.885056ms`에서 `2.174976ms`로 `44.016869%` 감소했고 draw P50은 2,240회에서 1,148회로 줄었다. 이 전체 값만 최적화 수치로 채택했다.

## 남은 한계

한 종류 prototype mesh를 반복한 synthetic scene이다. 패스별 P95는 서로 다른 프레임에서 나와 합산하지 않고, working set 감소도 메모리 절감으로 해석하지 않는다. 모든 pixel이 광원 32개를 순회한 lighting P95가 가장 컸다.
