# 8억 번 확인하던 NavMesh query를 먼저 정답으로 남겼다

## 상황

6주차를 시작할 때 `simulation` 모듈에는 XZ 좌표조차 없었다. 렌더러의 캐릭터 위치는 바꿀 수 있었지만 그 좌표가 걸을 수 있는 곳인지 판단하는 코드는 Windows 클라이언트에도 서버에도 없었다.

이번 주 질문은 빠른 탐색 구조를 만드는가가 아니었다. 전수 탐색과 가속 탐색이 같은 답을 내는지 먼저 증명하고, 실제 후보 수와 실행 시간이 줄었을 때만 최적화라고 부를 수 있는가였다. AI도 처음부터 behavior tree를 만드는 대신 작은 FSM으로 명령 기준을 먼저 고정했다.

## 재현

공식 측정은 benchmark 코드가 들어간 깨끗한 commit에서 실행했다.

```powershell
./scripts/run_simulation_benchmark.ps1
```

- commit: `5d318deab1b23e050d846b5321def30380d65adb`
- Windows 11 Home 10.0.26200
- AMD Ryzen 7 6800HS Creator Edition, 8 core, 16 logical processor
- MSVC `194435228`, Release
- seed: `20260823`
- 64×64 cell, 8,192 triangle NavMesh
- 공간 객체 1,124개
- Nav point query 100,000건
- AABB query 20,000건, point picking 20,000건
- AI decision 100,000건
- case마다 준비 1회, 측정 5회, 중앙값 사용

원본은 [20260823-182453 실행](../benchmarks/spatial-navigation/20260823-182453-5d318dea-seed20260823/RESULT.md)에 있다. `samples.csv`에는 8개 case의 40개 시간 샘플이 있고 `result.json`에는 workload, checksum, 후보 수가 들어 있다.

## 관찰

첫 기준선은 모든 NavMesh 삼각형을 끝까지 확인했다. 경계에 걸린 점에서 가장 작은 ID를 선택해야 했기 때문에 첫 삼각형을 찾았다고 바로 끝내지도 않았다. 100,000개 point를 8,192개 삼각형에 대입한 후보 수는 819,200,000회였다. Release 중앙값은 7308.5022ms였다.

일반 충돌과 picking도 객체 1,124개를 매 query마다 확인했다. AABB와 point query 모두 정확 판정 횟수가 22,480,000회였다. 중앙값은 각각 127.7082ms와 237.3660ms였다.

이 숫자는 한 frame 시간이 아니다. 고정된 대량 query 묶음을 한 번 처리한 시간이다. 실제 게임 frame과 혼동하지 않도록 원본에도 query 수를 같이 기록했다.

## 가설과 비교한 대안

NavMesh 삼각형은 경기 중 바뀌지 않는다. 삼각형 AABB를 일정 크기 cell에 한 번 넣어 두면 point가 속한 cell의 후보만 확인할 수 있다고 봤다. 음수 좌표를 unsigned bit shift로 합치면 구현은 짧아지지만 좌표 경계가 불분명해진다. `int32 x, z` key와 `floor(position / cellSize)`를 그대로 사용했다.

일반 객체에는 같은 grid를 재사용하지 않았다. 객체 크기가 다르고 사분면 경계에 걸친 객체가 많을 수 있기 때문이다. tight bounds보다 1.5배 큰 loose bounds를 가진 quadtree를 만들고, 한 자식만 객체 전체를 포함할 때만 아래로 내렸다. 여러 자식에 걸친 객체는 부모에 남겨 누락을 막았다.

AI는 FSM을 먼저 만들었다. `Idle`, `MoveToTarget`, `Attack` 세 명령을 근접형과 원거리형이 같이 썼다. 그 다음 원거리형에 `MoveAwayFromTarget`을 공격보다 앞에 추가했다. 이 커밋을 분리해 실제 변경 범위를 볼 수 있게 했다.

## 선택과 구현

NavMesh build 단계에서 vertex index, 면적, non-manifold edge를 검사했다. 입력 triangle 순서를 ID로 보존하고 공유 edge에서 정렬된 adjacency를 만들었다. 선형 query와 grid query는 경계에서 가장 작은 triangle ID를 반환한다.

A* open queue의 우선순위는 f 비용, h 비용, triangle ID 순으로 고정했다. `NavAgent`는 `speed * deltaSeconds`를 한 경유점에서 다 쓰지 못하면 남은 양을 다음 경유점으로 넘긴다. 1초 delta 한 번으로 여러 경유점을 통과하는 테스트를 먼저 통과시켰다.

Windows 데모에서는 우클릭 좌표를 곧바로 이동 좌표로 쓰지 않는다. 현재 stress camera의 view와 projection으로 screen ray를 만들고 y=0 지면과 교차시킨다. 그 점이 NavMesh query를 통과했을 때만 `SetDestination`을 호출한다. WARP 자동 경로도 같은 `NavAgent`를 사용하며 120 frame을 정확히 60Hz로 진행한다.

행동 트리는 `Selector`, `Sequence`, `Condition`, `Action`만 가진 동기식 구조다. 트리는 controller 생성 시 한 번 만들고 tick 중에는 node를 추가하지 않는다. 근접형과 원거리형 각각에 FSM과 behavior tree를 같은 시나리오 표로 실행해 명령을 비교했다.

## 동등성 문턱

측정 전에 다음 비교를 전부 실행했다.

| 비교 | 입력 수 | checksum | 불일치 |
| --- | ---: | --- | ---: |
| Nav linear와 grid | 100,000 | `8995d4e2c5340501` | 0 |
| Spatial linear와 quadtree AABB | 20,000 | `aa659a673b1def5c` | 0 |
| Spatial linear와 quadtree picking | 20,000 | `2a4d6959cbae9df9` | 0 |
| FSM과 behavior tree | 100,000 | `d82152856faf9747` | 0 |

하나라도 다르면 benchmark는 종료 코드 3으로 끝나고 실행 디렉터리를 만들지 않는다. 이번 실행의 mismatch는 0건이고 전체 결과 checksum은 `8e4984d98be95814`였다.

## 리뷰에서 다시 셌다

첫 공식 실행에서는 linear가 확인한 모든 triangle을 후보로 셌지만 grid는 AABB를 통과한 triangle만 셌다. 두 숫자의 단위가 달라 후보 감소율을 그대로 비교할 수 없었다. 같은 cell에 있지만 query point와 AABB가 겹치지 않는 triangle을 넣은 회귀 테스트에서 linear 2회, grid 1회가 나와 문제를 재현했다.

`candidatesTested`를 실제 순회한 triangle 수로 통일하고 commit `5d318dea`에서 다시 측정했다. 첫 실행의 JSON과 CSV는 삭제하거나 덮어쓰지 않고 비채택 원본으로 남겼다. 아래 표와 대표 링크는 수정 뒤 실행만 사용한다.

## 측정

| case | 중앙값 | 정확 검사 또는 평가 수 | 기준선 대비 시간 변화 |
| --- | ---: | ---: | ---: |
| Nav linear | 7308.5022ms | 819,200,000 | 기준 |
| Nav grid | 36.4567ms | 2,499,032 | -99.501174% |
| Spatial linear AABB | 127.7082ms | 22,480,000 | 기준 |
| Loose quadtree AABB | 89.8673ms | 13,323,264 | -29.630752% |
| Spatial linear picking | 237.3660ms | 22,480,000 | 기준 |
| Loose quadtree picking | 132.2706ms | 13,138,396 | -44.275676% |
| FSM | 2.8142ms | 100,000 | 기준 |
| Behavior tree | 6.6890ms | 100,000 | +137.687442% |

Nav grid의 후보 수는 99.694942% 줄었다. quadtree의 정확 AABB 판정 수는 범위 query에서 40.732811%, picking에서 41.555178% 줄었다. 세 공간 질의는 결과를 유지하면서 검사량과 시간이 같이 줄어 최적화 사례로 채택했다.

behavior tree는 반대였다. 같은 명령을 내지만 FSM보다 약 두 배 오래 걸렸다. virtual dispatch와 `std::function` 호출이 있는 현재 구조에서 예상할 수 있는 비용이다. 행동 트리를 속도 개선으로 설명하지 않고, 행동 우선순위를 node 단위로 분리한 구조 변경으로만 기록한다.

## FSM 변경 범위와 behavior tree 전환

원거리 후퇴 전이를 추가한 commit은 `0bb2478287f8fb5e691d77f774a29a2ef3d34da9`다. 이력을 고치지 않고 `git show --numstat`로 확인한 값은 다음과 같다.

| 파일 | 추가 | 삭제 |
| --- | ---: | ---: |
| `simulation/src/AiDecision.cpp` | 5 | 1 |
| `tests/simulation_ai_decision_test.cpp` | 34 | 2 |

작은 FSM에서 행동 하나를 넣는 변경량은 아직 부담스럽지 않았다. 따라서 behavior tree가 더 적은 코드라고 결론내릴 근거는 없다. 이번 전환에서 얻은 것은 조건 단락 평가를 독립 테스트하고, 두 archetype의 명령 결과를 FSM과 계속 대조할 수 있는 경계다.

## 검증

공식 측정 전 Windows Debug 전체 CTest 136개와 MSVC Release 빌드를 통과했다. WARP navigation smoke는 120 frame 뒤 제어 캐릭터가 `(20, 10)`에 도착했고 non-clear pixel과 NavMesh 내부 위치를 확인했다. screen ray 계산은 중앙 클릭, 지면과 평행한 ray, 0 크기 viewport를 별도 테스트했다.

로컬에는 GCC와 Clang이 없었고 Docker Desktop 엔진도 실행 중이 아니었다. Linux 결과를 통과했다고 쓰지 않는다. PR의 Ubuntu CI를 Linux 검증 문턱으로 남겼다.

## 남은 한계

NavMesh는 균일한 평면 grid다. 실제 골목, 좁은 portal, 층이 다른 지형을 대변하지 않는다. A* waypoint는 triangle 중심을 사용하며 funnel과 path smoothing은 없다.

quadtree는 정적 build만 지원한다. 이동 객체가 매 tick bounds를 바꿀 때 reinsertion 비용과 tree 품질은 아직 측정하지 않았다. looseness 1.5와 capacity 8도 이번 workload에서만 사용한 값이다.

behavior tree는 memoryless synchronous tree다. 공격 animation처럼 여러 tick에 걸쳐 `Running`을 유지하고 중단 뒤 이어서 실행하는 기능은 없다. 7주차 전투를 연결할 때 상태 수명과 명령 실행기를 분리해야 한다.

학습 순서는 [NavMesh 기록](https://tobrother.tistory.com/158)과 [Behavior Tree 기록](https://tobrother.tistory.com/164), 참고 프로젝트의 기능 구성을 살폈다. 코드, 맵, UI, 리소스는 가져오지 않았고 이번 수치와 실패 기록은 이 저장소에서 직접 만든 실행 결과만 사용했다.
