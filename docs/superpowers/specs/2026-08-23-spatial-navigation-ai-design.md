# 6주차 공간 탐색, NavMesh, AI 설계

## 목표

플랫폼 중립 `simulation` 모듈을 새로 만들고 같은 입력에서 기준 검색과 공간 가속 검색을 비교한다. NavMesh point query는 전체 triangle 순회와 spatial grid를, collision과 picking broad phase는 전체 entity 순회와 loose quadtree를 각각 제공한다.

NavMesh A*와 agent 이동을 연결하고 근접형과 원거리형 AI를 만든다. AI는 먼저 FSM으로 동작을 고정한 뒤 후퇴 행동을 추가할 때 바뀐 파일과 줄을 기록한다. 같은 시나리오를 behavior tree로 옮기고 회귀 테스트로 command sequence가 유지되는지 확인한다.

Windows에서는 별도 navigation demo app이 오른쪽 mouse 입력을 ground destination으로 바꾸고 agent 위치를 하이브리드 renderer의 첫 player에 반영한다. benchmark client와 5주차 raw 경로는 수정하지 않는다.

## 참고 범위

[NavMesh 글](https://tobrother.tistory.com/158)에서 triangle adjacency, spatial grid, A*와 agent 흐름을 참고한다. [Behavior Tree 글](https://tobrother.tistory.com/164)에서는 FSM state 증가 뒤 tree로 옮긴 문제 설명 순서를 참고한다. 코드, 이름, 상수, 자료구조는 복사하지 않고 이 저장소의 simulation 계약과 테스트에 맞게 작성한다.

## 범위 밖

- 3D 경사와 서로 다른 floor 높이
- NavMesh asset cooker와 editor
- funnel algorithm과 완전한 path smoothing
- dynamic obstacle 재구축
- reciprocal velocity obstacle과 군집 회피
- animation state와 attack damage 적용
- server tick 연결
- 여러 map과 여러 navigation layer

6주차 NavMesh는 XZ 평면만 다룬다. y 좌표와 실제 전투 판정은 7주차 offline match에서 연결한다.

## 모듈 구조

```text
simulation/
  include/dxa/simulation/
    Math2.hpp
    NavMesh.hpp
    NavAgent.hpp
    SpatialIndex.hpp
    AiDecision.hpp
    BehaviorTree.hpp
  src/
    NavMesh.cpp
    NavAgent.cpp
    SpatialIndex.cpp
    AiDecision.cpp
    BehaviorTree.cpp

apps/
  simulation_benchmark/
  navigation_demo/
```

`dxa_simulation`은 STL 외의 platform library를 링크하지 않는다. Windows와 Linux에서 같은 source를 빌드한다. engine은 simulation을 참조하지 않는다. `dxa_navigation_demo`만 engine과 simulation을 함께 링크한다.

## 수학과 ID 계약

`Vec2`는 XZ 좌표를 `x`, `y` 두 float로 저장한다. simulation 문서와 API에서는 두 번째 축을 `z`가 아니라 `y`로 부르지 않고 `Vec2{x, z}`의 의미를 명시한다. 혼동을 줄이기 위해 field 이름은 `x`, `z`로 둔다.

`Aabb2`는 inclusive minimum과 maximum을 가지며 생성 시 finite 값과 `minimum <= maximum`을 검증한다. 경계에 닿은 point와 box는 교차로 본다.

`TriangleId`와 `SpatialEntityId`는 `std::uint32_t` 별칭이다. invalid sentinel은 숫자 최대값을 재사용하지 않고 `std::optional`로 표현한다.

## NavMesh 계약

`NavMesh` 입력은 vertex 배열과 세 vertex index로 구성된 triangle 배열이다. 생성 시 다음을 검증한다.

- vertex와 query point는 finite
- triangle index는 vertex 범위 안
- 같은 vertex를 두 번 참조하지 않음
- XZ 면적이 epsilon보다 큼
- 동일 edge를 세 triangle 이상이 공유하지 않음

adjacency는 정렬된 vertex index pair를 edge key로 사용해 자동 생성한다. 입력 triangle 순서는 stable ID가 되며 query와 A*의 tie break에 사용한다.

`FindContainingTriangleLinear`는 모든 triangle을 ID 순서로 검사한다. `FindContainingTriangleGrid`는 기본 cell size `4.0F`를 사용하고 triangle AABB가 겹치는 모든 cell에 ID를 넣어 query cell의 후보만 검사한다. 생성자가 양수 cell size를 받을 수 있어 benchmark가 다른 크기도 비교한다. cell 안 후보는 정렬하고 중복을 제거한다. 경계에서 여러 triangle이 point를 포함하면 가장 작은 ID를 반환한다.

두 query는 `NavQueryResult`를 반환한다.

```cpp
struct NavQueryResult
{
    std::optional<TriangleId> triangle;
    std::uint32_t candidatesTested = 0;
};
```

benchmark는 결과 ID가 전부 같을 때만 시간과 candidate 수를 기록한다.

## A*와 NavAgent

`FindPath`는 시작점과 목적지가 포함된 triangle을 grid로 찾은 뒤 triangle adjacency graph에서 A*를 실행한다. edge cost와 heuristic은 triangle center 사이 Euclidean distance다. 같은 f cost에서는 h cost, triangle ID 순으로 선택해 결과를 재현한다.

path는 시작점, 중간 triangle center, 목적지 순으로 반환한다. 시작과 목적지가 같은 triangle이면 두 point만 반환한다. 목적지가 NavMesh 밖이면 실패하며 이전 agent path를 유지하지 않고 `InvalidDestination` 상태가 된다.

`NavAgent`는 `Idle`, `Moving`, `Arrived`, `InvalidDestination` 네 상태를 가진다. `SetDestination`은 path를 새로 계산하고 `Tick`은 fixed delta 안에서 waypoint를 넘지 않게 이동한다. 한 tick 거리가 여러 waypoint를 넘을 수 있으면 남은 거리를 다음 segment에 계속 사용한다. agent는 항상 계산된 segment 위에 머문다.

## Spatial index 계약

`LinearSpatialIndex`와 `LooseQuadtree`는 동일한 entity 목록을 받는다. entity는 stable ID와 `Aabb2`를 가진다.

지원 query는 두 가지다.

- `QueryAabb`: query box와 실제로 겹치는 ID
- `PickPoint`: point를 포함하는 ID

결과는 오름차순 ID이며 중복이 없다. loose quadtree는 node bounds에 looseness `1.5`, capacity `8`, max depth `6`을 기본으로 사용한다. child 한 곳의 loose bounds에 완전히 들어가는 entity만 내려보내고 나머지는 parent에 둔다. 최종 반환 전 narrow phase AABB 검사를 반드시 수행한다.

benchmark는 linear과 quadtree 결과가 모든 query에서 같은지 확인한 뒤 wall time과 `boundsTested`를 기록한다.

## AI command와 FSM 기준

AI는 world를 직접 수정하지 않고 다음 command 하나를 반환한다.

```cpp
enum class AiCommandType
{
    Idle,
    MoveToTarget,
    MoveAwayFromTarget,
    Attack
};
```

blackboard는 self와 target position, target 존재 여부, attack range, preferred range, retreat range, cooldown ready를 가진다.

근접형 FSM은 target 없음에서 Idle, 사거리 밖에서 MoveToTarget, 사거리 안과 cooldown ready에서 Attack을 반환한다. 원거리형 기준도 처음에는 같은 세 command만 사용한다. 별도 commit에서 target이 retreat range 안으로 들어오면 `MoveAwayFromTarget`을 추가하고 실제 diff의 파일 수와 added 및 deleted line을 기록한다.

## Behavior Tree

tree node status는 `Success`, `Failure`, `Running`이다. 첫 버전은 memoryless synchronous tick만 지원한다.

- `Condition`: blackboard predicate
- `Action`: `AiCommand`를 기록
- `Sequence`: child가 실패할 때까지 순서대로 실행
- `Selector`: child가 성공할 때까지 순서대로 실행

근접형 tree는 attack sequence, chase sequence, idle 순서다. 원거리형 tree는 retreat sequence, attack sequence, chase sequence, idle 순서다. 같은 blackboard scenario table을 FSM과 tree 양쪽에 실행해 command 결과가 같아야 한다.

tree는 init 때 한 번 만들고 tick 중 heap allocation을 하지 않는다. node ownership은 `std::unique_ptr`로 한 곳에 둔다.

## 오른쪽 mouse 이동 demo

`InputState`에 client mouse position과 right button transition을 추가한다. `Window`는 `WM_MOUSEMOVE`, `WM_RBUTTONDOWN`, `WM_RBUTTONUP`을 전달한다.

`dxa_navigation_demo`는 camera ray와 y=0 ground plane의 교점을 구한다. 오른쪽 button press가 있고 교점이 NavMesh 안이면 agent destination을 갱신한다. agent position은 `HybridDeferredRenderer::SetControlledPlayerPosition`으로 첫 player instance에 반영한다.

headless smoke는 `--auto-destination x z`를 사용해 실제 mouse 없이 같은 path와 renderer를 검증한다. Window test는 `WM_RBUTTONDOWN` message로 button transition과 좌표를 별도로 확인한다.

## 재현 가능한 benchmark

`dxa_simulation_benchmark`는 seed `20260823`으로 다음 입력을 만든다.

- 64×64 cell, 8,192 triangle 평면 NavMesh
- arena 안 point query 100,000개
- stress scene population과 같은 1,124개 entity AABB
- AABB query 20,000개와 point pick 20,000개

Release 실행은 각 case를 warmup 뒤 5회 반복하고 median wall time을 기록한다. 시간보다 먼저 result checksum과 mismatch count를 저장한다. mismatch가 0이 아니면 process는 실패하고 output directory를 만들지 않는다.

raw JSON은 commit SHA, CPU, compiler, seed, command, query count, candidate 검사 수와 wall time을 포함한다. 결과가 빨라지지 않아도 실제 수치를 기록한다.

## 테스트와 완료 조건

- invalid NavMesh와 AABB 입력이 거부됨
- linear과 grid point query가 경계 포함 100,000개에서 동일함
- linear과 loose quadtree query가 40,000개에서 동일함
- A*가 단절 영역 실패와 deterministic 최단 path를 처리함
- agent가 large delta에서도 waypoint를 overshoot하지 않음
- right button transition과 navigation demo smoke가 Windows에서 통과함
- 근접형과 원거리형 FSM scenario가 통과함
- behavior tree가 같은 scenario에서 FSM과 동일 command를 반환함
- Windows Debug와 Release, Linux GCC `-Werror`가 통과함
- benchmark raw가 clean commit에서 생성되고 기존 run을 덮어쓰지 않음

## 문서화 원칙

linear 기준은 의도한 비교 구현으로 기록하며 장애처럼 표현하지 않는다. 최적화 사례는 result equivalence가 100%이고 실제 candidate 수 또는 wall time이 줄어든 항목만 채택한다.

NavMesh 글에 적힌 일부 path가 벽에 막히는 한계처럼, 이번 구현도 center waypoint 방식의 corner path가 최적이 아님을 명시한다. funnel과 obstacle avoidance는 구현한 것처럼 쓰지 않는다.
