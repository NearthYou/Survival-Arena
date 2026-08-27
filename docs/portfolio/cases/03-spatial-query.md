# 전수 탐색의 정답을 보존한 공간 질의 최적화

## 상황

6주차 시작 시 simulation에는 걸을 수 있는 좌표 판정이 없었다. NavMesh와 일반 공간 질의의 전수 탐색을 먼저 정답으로 남기고, 같은 ID를 반환한 뒤 후보와 시간이 줄었을 때만 최적화로 분류했다.

## 재현

8,192개 triangle과 객체 1,124개에서 seed `20260823`으로 Nav point 100,000건, AABB와 picking 각 20,000건을 실행했다. commit `5d318dea`의 MSVC Release에서 5회 중앙값을 썼다. 조건은 [개발 기록](../../devlog/2026-08-23-spatial-navigation-ai.md)에 있다.

## 관찰

Nav linear는 819,200,000개 후보와 `7308.5022ms`를 기록했다. 첫 공식 실행은 grid가 AABB 전필터 통과분만 세고 linear는 모든 순회를 세어 단위가 달랐다. [비채택 원본](../../benchmarks/spatial-navigation/20260823-180321-e1d0ef0f-seed20260823/RESULT.md)은 삭제하지 않고 제외했다.

## 가설과 비교한 대안

정적 triangle은 겹치는 cell에 넣으면 해당 cell만 볼 수 있다. 큰 객체까지 같은 grid에 넣으면 중복이 늘어 일반 질의에는 loose quadtree를 비교했다. 동적 rebuild와 A* funnel은 첫 평면 맵 범위를 넘었다. 대신 정적 build와 설정값 의존, 기준 구현 유지 비용을 받아들였다.

## 선택

Nav point에는 희소 grid, 일반 객체에는 loose quadtree를 썼다. 경계점은 가장 작은 triangle ID를 고르고 결과 ID를 정렬했다. FSM도 behavior tree의 회귀 기준으로 남겼다. 결정은 [공간 탐색 ADR](../../adr/0004-spatial-navigation-and-behavior-tree.md)에 고정했다.

## 구현

NavMesh build에서 면적과 non-manifold edge를 검사하고 입력 순서를 ID로 보존했다. grid는 정렬된 cell 후보만 판정한다. quadtree는 객체를 완전히 포함하는 자식이 하나일 때만 내리고, 경계 객체는 부모에 남겼다.

## 검증

[채택 원본](../../benchmarks/spatial-navigation/20260823-182453-5d318dea-seed20260823/RESULT.md)에서 Nav linear와 grid checksum은 같고 mismatch는 `0`이었다. 순회 후보는 819,200,000개에서 2,499,032개로 `99.694942%` 줄었다. AABB와 picking도 mismatch 0과 검사량 감소를 함께 확인했다.

## 남은 한계

평면 synthetic workload의 대량 query 중앙값이지 경기 한 프레임 시간이 아니다. 높이, portal, 동적 장애물과 quadtree reinsertion은 검증하지 않았다. Behavior tree는 FSM보다 `137.687442%` 오래 걸려 성능 개선과 묶지 않는다.
