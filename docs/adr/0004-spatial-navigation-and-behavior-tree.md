# ADR 0004: 공간 질의 기준선을 보존하고 가속 구조를 분리함

- 상태: 채택
- 날짜: 2026-08-23

## 상황

6주차 전까지 `simulation` 모듈에는 이동 가능한 영역, 충돌 후보 탐색, AI 의사결정 규칙이 없었다. 클라이언트에서만 좌표를 바꾸면 화면은 움직일 수 있지만, 9주차 권위형 서버가 같은 입력을 다시 검증할 방법이 없다. 플랫폼 중립 모듈에서 결과 계약을 먼저 정하고 Windows 클라이언트는 그 결과만 보여줘야 했다.

공간 구조를 바로 최적화 구현으로 시작하면 결과가 맞는지와 빨라졌는지를 한 번에 판단해야 한다. 그래서 NavMesh point query와 AABB 충돌 및 picking 모두 전수 탐색을 먼저 남기고, 같은 입력에 같은 ID를 반환하는 가속 구조를 따로 두기로 했다.

AI도 비슷했다. 근접형과 원거리형의 처음 세 행동은 작은 조건문으로 충분했다. 다만 원거리 후퇴 행동을 넣으면서 조건 우선순위가 늘었고, 이후 전투 행동을 계속 추가할 때 하나의 전이문을 고치는 방식이 얼마나 넓게 번지는지 확인할 기준이 필요했다.

## 결정

NavMesh는 빌드 뒤 바뀌지 않는 삼각형 그래프로 둔다. 입력 순서가 `TriangleId`가 되며 공유 vertex index로 인접 삼각형을 만든다. 같은 변을 세 삼각형이 소유하거나 면적이 0인 삼각형은 빌드 단계에서 거부한다. 경계점이 둘 이상의 삼각형에 포함되면 가장 작은 ID를 선택한다.

point query는 두 경로를 유지한다.

1. `FindContainingTriangleLinear`는 모든 삼각형을 확인하는 정답 기준선이다.
2. `FindContainingTriangleGrid`는 삼각형 AABB가 겹치는 희소 cell에 ID를 넣고 해당 cell의 정렬된 후보만 확인한다.

경로 탐색은 삼각형 중심 사이 거리를 비용으로 쓰는 A*로 구현한다. 우선순위는 f 비용, h 비용, 삼각형 ID 순이다. 같은 입력에서 서버와 클라이언트가 다른 경로를 고르지 않도록 이 순서를 계약으로 둔다. `NavAgent`는 남은 이동량을 여러 경유점에 이어서 소비해 frame delta가 커도 경유점 앞에서 멈추지 않는다.

일반 공간 질의도 전수 탐색인 `LinearSpatialIndex`와 `LooseQuadtree`를 함께 둔다. quadtree node의 loose bounds가 객체를 완전히 포함하는 자식이 정확히 하나일 때만 그 자식으로 내린다. 여러 자식과 겹치거나 너무 큰 객체는 부모가 소유한다. node bounds로 가지치기한 뒤에는 객체 AABB로 다시 정확 판정하며 결과 ID는 정렬한다.

AI는 FSM을 삭제하지 않고 회귀 기준으로 남긴다. 실행 경로는 생성 시 한 번 조립한 동기식 behavior tree를 사용할 수 있게 한다. `Selector`, `Sequence`, `Condition`, `Action`은 상태를 기억하지 않으며 tick 중 node를 만들지 않는다. 근접형은 공격, 추적, 대기 순서이고 원거리형은 후퇴를 가장 앞에 둔다. 두 구현은 같은 blackboard 검증 함수를 사용한다.

## 비교한 대안

NavMesh를 매 frame 수정하는 방식은 장애물 변화에 대응할 수 있다. 첫 맵은 평면 한 장이고 동적 장애물 회피가 범위에 없으므로 rebuild와 tile streaming 비용을 지금 넣지 않았다.

point query와 일반 충돌을 하나의 uniform grid로 통일할 수도 있다. NavMesh는 정적 삼각형이고 일반 객체는 크기가 제각각이다. 큰 객체가 많은 경우 grid cell 중복이 늘어날 수 있어 일반 객체에는 loose quadtree를 별도 비교했다.

A* 뒤에 funnel algorithm을 바로 붙이면 삼각형 중심을 지나는 꺾인 경로를 줄일 수 있다. 이번 질문은 경로 선택의 결정성과 이동량 소비였기 때문에 portal 계산과 smoothing은 남겼다.

FSM만 유지하는 방법은 현재 행동 수에서는 가장 빠르고 단순하다. 실제 Release 측정에서도 100,000개 명령 중앙값이 FSM 3.0411ms, behavior tree 6.1247ms였다. behavior tree는 성능 최적화로 채택한 것이 아니다. 조건과 행동을 node 단위로 분리하고 기존 명령 동등성을 자동 검증할 수 있다는 구조적 이유로 추가했다.

## 결과

깨끗한 commit `e1d0ef0fe99b4b0ff36ff8e85fd8da25ab9d25d9`에서 Windows 11, Ryzen 7 6800HS, MSVC Release, seed `20260823`으로 실행했다. 각 case는 1회 준비 뒤 5회 측정했고 중앙값을 사용했다. 측정 전에 NavMesh 100,000건, AABB 20,000건, picking 20,000건, AI 100,000건의 결과를 비교했다. mismatch는 0건이고 네 비교 쌍의 checksum이 각각 같았다.

| 비교 | 기준선 중앙값 | 가속 또는 대체 중앙값 | 검사량 변화 |
| --- | ---: | ---: | ---: |
| Nav point query | 6531.8967ms | 31.7562ms | 819,200,000에서 118,612, -99.985521% |
| AABB query | 105.3028ms | 66.7800ms | 22,480,000에서 13,323,264, -40.732811% |
| Point picking | 239.1861ms | 117.9218ms | 22,480,000에서 13,138,396, -41.555178% |
| AI command | FSM 3.0411ms | Behavior tree 6.1247ms | 둘 다 100,000회 |

공간 그리드는 이 고정 workload에서 전수 탐색보다 99.513829% 짧았다. loose quadtree는 AABB query 36.582883%, picking 50.698724% 짧았다. 반면 behavior tree는 FSM보다 101.397521% 오래 걸렸다. AI 결과는 구조 전환의 비용으로 기록하며 성능 개선 사례로 분류하지 않는다.

## 결과에 따른 제약

두 기준 구현을 지우지 않는다. 테스트와 benchmark가 가속 경로의 결과를 계속 비교할 수 있어야 한다. 성능 숫자보다 checksum과 mismatch 0건을 먼저 통과해야 한다.

NavMesh 경계의 최저 ID 규칙과 A* 동률 순서는 네트워크 결정성 계약으로 취급한다. 이후 알고리즘을 바꾸면 기존 테스트와 protocol 관점에서 의도한 변경인지 먼저 확인한다.

behavior tree는 현재 memoryless synchronous tree다. 여러 tick에 걸친 행동, 중단 후 재개, blackboard 수명 관리가 필요해지면 `Running` 상태 저장 방식을 별도 ADR로 결정한다.

측정 원본은 [공간 탐색 실행](../benchmarks/spatial-navigation/20260823-180321-e1d0ef0f-seed20260823/RESULT.md)에 둔다.
