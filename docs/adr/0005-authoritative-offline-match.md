# ADR 0005: 오프라인 경기에서도 서버와 같은 권위 tick을 사용함

- 상태: 채택
- 날짜: 2026-08-24

## 상황

7주차에는 이동, 파밍, 전투, zone 피해와 결과 화면을 한 경기로 연결해야 했다. DX11 demo 안에서 actor 체력을 직접 바꾸면 화면은 빨리 만들 수 있지만, 9주차 게임 서버가 같은 규칙을 다시 구현하게 된다. client와 server의 처리 순서가 달라지면 같은 command로 다른 사망자와 winner가 나올 수 있다.

참가자와 중립 AI가 같은 tick에 여러 공격을 보내는 문제도 있었다. actor ID 순으로 피해를 바로 적용하면 앞 ID가 target을 먼저 죽이고 뒤 공격을 없애는 편향이 생긴다. 마지막 두 참가자가 서로를 죽이거나 같은 zone tick에 모두 죽는 경우에는 살아 있는 actor가 0명이 될 수 있다.

## 결정

플랫폼 중립 `dxa_simulation`에 `OfflineMatch` aggregate를 둔다. 외부는 `MatchCommand`를 제출하고 `Step()`을 한 번 호출한 뒤 `MatchSnapshot`과 `MatchEvent`를 읽는다. match는 NavMesh 사본과 NavAgent를 소유한다. caller의 임시 NavMesh가 사라져도 agent reference가 유효하도록 NavMesh를 먼저 만들고 agent를 나중에 만든다.

한 tick의 순서는 다음으로 고정한다.

1. 외부 command 검증과 actor별 마지막 유효 command 선택
2. 5Hz 내부 bot 판단
3. 30Hz NavAgent 이동
4. 자동 loot pickup
5. cooldown 감소와 combat batch
6. whole-second safe-zone 피해
7. 사망과 winner 판정
8. event 정렬과 checksum 누적

공격은 pre-damage 상태에서 모두 검증한다. target별 피해 기여분을 모아 한 번에 적용하고, 처치자는 기여 피해가 큰 actor, 동률이면 작은 ActorId로 정한다. combat 또는 zone 처리로 남은 참가자가 모두 죽으면 피해 직전 체력, 기존 처치 수, 작은 ActorId 순으로 한 명을 체력 1에 남긴다. 보존된 사망에 잘못 더해진 처치 수만 되돌린다.

ID 0은 사용자 actor다. 내부 경쟁 bot은 ID 1부터 23만 조작한다. WARP와 benchmark는 공개 `DecideContender`를 외부 controller로 호출해 ID 0 command를 같은 `Submit()` 경계에 넣는다. 수동 회귀 fixture는 `enableInternalBots=false`로 내부 AI 개입을 끈다.

600초에도 둘 이상 살아 있으면 alive, health, eliminations, ActorId 순으로 한 명을 선택하고 `TimeLimit`으로 끝낸다. 이는 true combat victory가 아니라 v1 종료 보장 규칙이다.

## 비교한 대안

ECS를 도입하면 component별 system을 나눌 수 있다. 첫 버전은 actor 역할 두 종류, 무기 세 종류, 맵 한 개다. registry와 scheduler를 먼저 만드는 비용보다 한 aggregate에서 tick 순서를 읽고 테스트하는 편이 작았다.

Windows demo가 simulation을 직접 소유하는 방법도 제외했다. renderer가 `MatchSnapshot`을 include하면 Linux server build에 DirectX 경계가 새어 들어간다. app adapter가 snapshot을 일반 `SceneCharacterState`로 바꾸고 renderer는 위치와 active flag만 받도록 했다.

피해를 actor ID 순으로 즉시 적용하는 방식은 구현량이 적지만 같은 tick 입력 순서에 결과가 달라졌다. `ResolveAttacks`는 accepted intent와 damage, death record를 분리하고 결과 ID를 정렬한다.

경기를 8분까지 강제로 끝내지 않는 방법도 제외했다. 마지막 생존자가 나오면 즉시 끝난다는 규칙을 유지하고, 실제 encounter density와 zone 곡선을 바꿔 canonical 종료 시간을 맞췄다.

## 결과

첫 64×64 arena 자동 경기는 tick 91, 약 3초에 끝났다. 중립 AI를 모두 빼도 tick 1,207, 약 40초였다. arena와 zone을 4배와 5배로 같은 비율로 키운 실행은 각각 tick 10,561과 10,560으로 차이가 없었다. 상대적인 zone 수렴 시점이 그대로였기 때문이다.

256×256 arena를 유지하고 8분까지 zone을 128, 112, 96, 80, 64로 천천히 줄인 뒤 sudden death에서 0으로 수렴하도록 바꿨다. 병합 전 검토 수정 뒤의 공식 Release 실행은 tick 16,147, 약 538.2초에 winner 2로 끝났다. repeat mismatch는 0건이고 tick P95는 0.2292ms였다.

원본은 [오프라인 경기 Release 측정](../benchmarks/offline-match/20260824-023134-1ede6a23-seed20260823/RESULT.md)에 있다.

## 결과에 따른 제약

`OfflineMatch::Step()`만 권위 규칙을 변경한다. renderer와 input adapter는 actor health, death, winner를 자체 판정하지 않는다.

event는 문자열 로그가 아니라 type과 ID, amount로 기록한다. 한 tick 안에서는 type, actor, subject, loot 순으로 정렬한 뒤 FNV-1a checksum에 누적한다. `DrainEvents()`가 history를 비워도 checksum은 유지한다.

timeout ranking과 wipe 보정은 무승부를 표현하지 못한다. 다음 버전에서 draw를 허용하거나 서버 reconnect를 지원하면 결과 protocol과 함께 다시 결정해야 한다.

이번 NavMesh는 높이 없는 256×256 평면이고 actor body blocking이 없다. 이 ADR은 tick ownership과 판정 순서를 채택한 것이며 실제 지형과 복잡한 물리를 검증한 결정은 아니다.
