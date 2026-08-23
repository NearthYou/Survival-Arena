# 7주차 오프라인 경기 루프 설계

## 목적

네트워크 없이 한 대의 PC에서 배틀로얄 한 판을 끝까지 실행한다. 플레이어 1명과 경쟁 봇 23명이 참가하고, 맵에는 중립 AI 100마리가 존재한다. 참가자는 무기를 줍고 싸우며 축소 구역 밖에서 피해를 받는다. 마지막 생존자가 결정되면 결과를 표시한다.

게임 규칙은 `simulation` 모듈에 둔다. Windows와 DX11을 참조하지 않으며 9주차 게임 서버가 같은 코드를 사용할 수 있어야 한다. DX11 앱은 입력을 명령으로 바꾸고 simulation snapshot을 화면에 옮기는 역할만 맡는다.

## 확정 범위

- 맵 1개, 256×256 XZ 평면 NavMesh
- 경쟁 참가자 24명
- 사용자 조작 참가자 1명과 자동 참가자 23명
- 중립 AI 100마리, 근접형 50마리와 원거리형 50마리
- 무기 3종
- 무기와 회복 아이템 파밍
- 체력, 피해, cooldown, 사망
- 4단계 축소 구역과 sudden death
- 최후 생존자와 결과 상태
- 실제 시간 기준 8분에서 10분 사이를 목표로 한 canonical match
- 보이는 DX11 실행과 빠른 WARP 자동 검증

계정, 전적 저장, 채팅, 네트워크, 탄약, 방어구, 제작, 여러 inventory slot, 실제 projectile, actor끼리의 body blocking, 복잡한 물리는 제외한다.

## 선택한 구조

### 선택: 권위형 OfflineMatch aggregate

`OfflineMatch` 하나가 경기 시간, actor, loot, safe zone, command, event 순서를 소유한다. 외부에서는 명령을 제출하고 고정 tick을 한 번 진행한 뒤 snapshot과 event를 읽는다.

```text
Player input / Bot decision
          |
          v
     MatchCommand
          |
          v
 OfflineMatch::Step()
   | movement
   | loot pickup
   | combat
   | safe-zone damage
   | death and result
          |
          v
 MatchSnapshot + MatchEvent
```

이 구조는 게임 서버가 나중에 `Step()`을 30Hz로 호출하기 쉽고, 처리 순서를 한 곳에서 검증할 수 있다.

### 제외: ECS 도입

ECS는 actor 종류와 component 조합이 많을 때 유리하다. 첫 버전은 actor 역할 두 종류, 맵 한 개, 무기 세 개뿐이다. registry, component storage, system scheduler를 먼저 만드는 비용이 현재 범위를 넘는다.

### 제외: Windows 데모 안에서 규칙 실행

가장 빠르게 화면을 만들 수 있지만 simulation을 서버에서 재사용할 수 없다. 입력과 렌더링 코드가 경기 판정 순서를 소유하게 되어 9주차 권위 서버와 결과가 달라질 위험이 있다.

## 공개 타입과 책임

### MatchTypes

```cpp
using ActorId = std::uint32_t;
using LootId = std::uint32_t;

enum class ActorRole { Contender, Neutral };
enum class WeaponType { Blade, Rifle, ArcPulse };
enum class MatchPhase { Waiting, Running, SuddenDeath, Finished };
enum class MatchEndReason { LastSurvivor, TimeLimit };
```

ID는 match 안에서만 유효하며 입력 순서와 무관하게 증가한다. actor와 loot snapshot은 ID 순으로 정렬한다.

### MatchCommand

외부 입력은 다음 tick에 적용할 command로 제출한다.

```cpp
struct MatchCommand
{
    ActorId actor;
    std::optional<Vec2> moveDestination;
    std::optional<ActorId> attackTarget;
};
```

존재하지 않거나 죽은 actor의 명령, NavMesh 밖 목적지, 존재하지 않거나 죽은 공격 대상은 거부한다. 같은 actor가 한 tick에 여러 명령을 제출하면 마지막 유효 명령 하나를 사용한다.

### OfflineMatch

```cpp
class OfflineMatch
{
public:
    static OfflineMatch Create(const NavMesh&, MatchConfig);
    void Start();
    void Submit(MatchCommand);
    void Step();
    [[nodiscard]] MatchSnapshot Snapshot() const;
    [[nodiscard]] std::vector<MatchEvent> DrainEvents();
};
```

`Step()`은 정확히 `1 / 30`초만 진행한다. 호출자가 임의 delta를 넣지 않게 해 경기 결과를 frame rate에서 분리한다.

### MatchSnapshot

snapshot은 renderer와 이후 protocol이 소비하는 읽기 전용 결과다.

- tick과 경과 시간
- 현재 phase
- safe zone center와 radius
- actor별 role, position, health, alive, weapon
- 남은 참가자 수
- winner와 end reason
- 누적 event checksum

snapshot은 내부 container나 `NavAgent`를 노출하지 않는다.

## 경기 초기화

seed `20260823`이 기본 canonical seed다. 별도 seed도 허용하지만 테스트와 공식 기록은 canonical seed를 사용한다.

- 참가자 ID 0은 사용자 조작 actor다.
- 참가자 24명은 중심에서 반지름 80에서 104 사이의 고리에 배치한다.
- 중립 AI 100마리는 이동 가능한 영역 안에 분산한다.
- 참가자는 체력 100으로 시작한다.
- 근접 중립 AI는 체력 60, 원거리 중립 AI는 체력 45로 시작한다.
- 경쟁 참가자는 모두 Blade를 들고 시작한다.
- 근접 중립 AI는 Blade, 원거리 중립 AI는 Rifle을 사용한다.
- Rifle pickup 24개, ArcPulse pickup 12개, MedKit 24개를 배치한다.
- spawn과 loot 위치는 NavMesh point query를 통과해야 한다.
- 최소 spawn 간격을 만족하지 못하면 정해진 시도 횟수 뒤 생성 실패를 반환한다.

고정 seed에서 같은 위치와 ID가 나와야 한다. 난수 엔진의 결과를 직접 float로 바꾸는 기존 24-bit 변환 규칙을 재사용한다.

## 전투 규칙

### Blade

- 단일 대상
- 사거리 2.2
- 피해 24
- cooldown 21 tick, 0.70초

### Rifle

- 단일 대상 hitscan
- 사거리 18
- 피해 12
- cooldown 12 tick, 0.40초

### ArcPulse

- 공격 대상을 중심으로 범위 피해
- 사거리 10
- 폭발 반경 5
- 피해 18
- cooldown 90 tick, 3초

cooldown은 float 시간이 아니라 남은 tick 수로 저장한다. 실제 projectile과 rigid-body 충돌은 만들지 않는다. tick 시점의 XZ 거리와 alive 상태로 판정한다. 참가자는 다른 참가자와 중립 AI를 공격할 수 있다. 중립 AI는 참가자만 공격하며 중립 AI끼리는 싸우지 않는다.

공격 처리 순서는 attacker ID 순이다. 같은 tick에 들어온 공격은 먼저 모두 유효성을 판정하고 피해를 모은 뒤 한 번에 적용한다. 앞 ID가 먼저 대상을 죽여 뒤 ID의 공격을 없애는 편향을 피한다.

같은 tick에 여러 attacker의 피해로 대상이 죽으면 그 tick에 가장 큰 피해를 준 actor가 처치자로 기록된다. 피해가 같으면 작은 ActorId를 선택한다. safe-zone 피해로 죽은 경우에는 처치자를 기록하지 않는다.

combat batch로 남은 경쟁 참가자가 모두 죽는 경우에는 피해 적용 직전 체력, 기존 처치 수, 작은 ActorId 순으로 한 명을 선택해 체력 1로 남긴다. 보존된 actor의 `ActorDied` event는 만들지 않으며, 그 actor를 죽인 것으로 계산된 상대의 처치 수도 되돌린다. 같은 tick에 새로 얻은 다른 처치 기록은 유지한다.

체력이 0 이하가 되면 한 번만 `ActorDied` event를 낸다. 죽은 actor는 이동, pickup, 공격, safe-zone 피해 대상에서 제외한다.

## 파밍 규칙

actor 중심에서 반경 1 안에 있는 가장 작은 `LootId` 하나를 자동 pickup한다.

- Rifle 또는 ArcPulse pickup은 현재 무기를 교체한다.
- MedKit은 체력 35를 회복하며 최대 체력 100을 넘지 않는다.
- 한 번 주운 loot는 다시 주울 수 없다.
- 중립 AI는 loot를 줍지 않는다.
- 죽은 actor는 loot를 떨어뜨리지 않는다.

inventory 여러 칸과 탄약을 제외해 파밍의 첫 질문을 위치 선택과 무기 교체로 제한한다.

## AI 규칙

### 경쟁 봇 23명

decision은 5Hz, 이동과 combat resolution은 30Hz로 수행한다.

match 내부의 경쟁 봇 판단은 참가자 ID 1부터 23까지만 담당한다. 사용자 actor인 ID 0의 command는 외부에서 제출한다. visible mode에서는 실제 입력과 app의 자동 공격 선택을 사용하고, WARP 자동 검증과 benchmark에서는 같은 `DecideContender` 함수를 외부 controller로 호출해 ID 0도 자동 조작한다. 따라서 내부 AI가 사용자 이동 command를 덮어쓰지 않는다.

1. safe zone 밖이면 zone center로 이동
2. 체력이 45 이하이고 가까운 MedKit이 있으면 회복 아이템으로 이동
3. Blade만 들고 있고 가까운 Rifle 또는 ArcPulse가 있으면 weapon loot로 이동
4. 공격 가능한 가장 가까운 적이 있으면 공격
5. 적이 사거리 밖이면 그 적에게 이동

거리와 점수가 같으면 작은 ID를 선택한다.

### 중립 AI 100마리

기존 `BehaviorTreeAiController`를 사용한다. 가장 가까운 살아 있는 참가자를 target으로 삼는다.

- 근접형은 추적 후 공격
- 원거리형은 너무 가까우면 후퇴하고 사거리 안에서 공격
- safe zone 피해는 받지만 경기 생존자 수에는 포함하지 않는다.

## 축소 구역

center는 첫 버전에서 `(0, 0)`으로 고정한다. phase별 반경은 tick에서 선형 보간한다.

| 구간 | 시간 | 반경 | 바깥 피해 |
| --- | ---: | ---: | ---: |
| 1단계 | 0초에서 120초 | 128에서 112 | 초당 2 |
| 2단계 | 120초에서 240초 | 112에서 96 | 초당 4 |
| 3단계 | 240초에서 360초 | 96에서 80 | 초당 8 |
| 4단계 | 360초에서 480초 | 80에서 64 | 초당 16 |
| Sudden death | 480초에서 600초 | 64에서 0 | 초당 32 |

zone 피해는 30 tick마다 한 번 정수 피해로 적용한다. 한 tick의 zone 피해로 남은 참가자가 모두 죽는 경우, 피해 직전 체력, 처치 수, 작은 ActorId 순으로 한 명을 체력 1에 남긴다.

마지막 참가자 한 명이 남으면 즉시 경기를 끝낸다. canonical seed의 자동 경기는 480초에서 600초 사이에 끝나야 한다. 600초에 여러 참가자가 남으면 같은 순위 규칙으로 한 명만 남기고 `TimeLimit`으로 종료한다. 이 timeout 판정은 v1 한계로 개발 기록에 남긴다.

## canonical 규모 재설계

첫 AI 연결 결과 64×64 맵에서는 canonical 경기가 tick 91, 약 3초에 끝났다. 중립 AI 100마리를 제외해도 참가자 24명만으로 tick 1,207, 약 40초에 끝났다. 첫 사망은 tick 19였고, 전체 구성에서 경쟁 참가자 사망 23명, 중립 AI 사망 1명, 경쟁 참가자 발 피해 event 30건, 중립 AI 발 피해 event 120건이 확인됐다. zone 사망은 없었다.

원인은 이동이나 cooldown 순서 오류가 아니라 좁은 공간에서 모든 actor가 초기 perception 안에 들어와 동시에 추적을 시작한 밀도였다. 8분에서 10분 목표와 즉시 LastSurvivor 판정을 모두 유지하기 위해 사용자 승인 후 공간 규모만 4배로 확대했다.

- arena 범위는 XZ 각각 -128에서 128
- 참가자 spawn 반경은 80에서 104
- neutral과 loot 생성 범위는 -128에서 128
- 첫 4배 zone 반경 실험은 128, 96, 64, 32, 8, 0
- 시간, 이동 속도, perception, 체력, 무기 피해와 cooldown은 유지

4배 확대 결과는 tick 10,561이었고 5배 확대 결과도 tick 10,560이었다. arena와 zone을 같은 비율로 키우면 초기 교전만 줄고 상대적인 zone 수렴 시점은 유지되어 추가 확대 효과가 없었다. 사용자 승인 후 arena는 256×256으로 유지하고 zone 수렴 곡선을 128, 112, 96, 80, 64, 0으로 변경했다. 8분까지 전장을 넓게 유지하고 sudden death에서만 64에서 0으로 집중한다.

종료 tick acceptance는 바꾸지 않는다. 새 곡선에서 같은 seed를 다시 실행해 14,400에서 18,000 tick을 실제로 통과하는지 검증한다.

## 이벤트 순서

한 tick은 다음 순서로 처리한다.

1. command 검증과 덮어쓰기
2. bot decision
3. movement
4. loot pickup
5. 공격 유효성 판정과 피해 batch
6. safe-zone 피해
7. 사망 event
8. 생존자와 결과 판정
9. snapshot checksum 갱신

event는 해당 tick 안에서 종류와 actor ID 순으로 안정적으로 정렬한다. 외부는 `DrainEvents()`로 가져가며 match가 전체 event history를 계속 보관하지 않는다. 누적 checksum만 match에 남긴다.

## DX11 데모

`apps/offline_match_demo`를 새로 만든다. 기존 navigation demo는 6주차 회귀 기준으로 유지한다.

- visible mode는 실제 시간 accumulator로 simulation을 30Hz 진행한다.
- 사용자는 지면 우클릭으로 이동한다.
- app adapter는 snapshot에서 가장 가까운 적을 골라 사용자의 attack command로 제출한다. match가 사거리와 alive 상태를 다시 검증한다.
- 경쟁 봇과 중립 AI의 위치와 alive 상태를 renderer에 전달한다.
- dead actor는 renderer에서 비활성 처리한다.
- safe zone marker는 snapshot radius를 사용한다.
- window title에 경과 시간, 생존자 수, 현재 무기를 표시한다.
- 경기가 끝나면 winner와 종료 이유를 title과 console에 표시하고 마지막 장면을 유지한다.

renderer는 `simulation` 타입을 include하지 않는다. app adapter가 `MatchSnapshot`을 engine의 일반 character instance 입력으로 변환한다.

WARP 자동 검증은 18,000 simulation tick을 렌더링하지 않고 빠르게 진행한 뒤 시작 장면과 결과 장면만 렌더한다. 다음을 만족하지 않으면 process가 실패한다.

- 결과가 480초에서 600초 사이
- 살아 있는 참가자 정확히 한 명
- winner가 그 참가자와 일치
- 모든 actor 위치와 체력이 유효
- event checksum이 0이 아님
- 결과 장면에 non-clear pixel 존재
- DX11 debug error 없음

## 오류 처리

- 잘못된 config, 중복 ID, 잘못된 무기 수치, 유효하지 않은 위치는 생성 단계에서 예외
- 잘못된 외부 command는 match를 중단하지 않고 `CommandRejected` event로 기록
- 내부 invariant 위반은 예외로 중단해 WARP와 테스트가 실패하도록 처리
- output과 benchmark 원본은 기존 디렉터리를 덮어쓰지 않음

## 테스트 전략

### 단위 테스트

- 무기 사거리, cooldown, 단일 및 범위 피해
- 체력 상한, 중복 사망 방지
- loot 한 번만 pickup, 무기 교체, MedKit 회복
- safe zone phase 경계, 보간, 정수 피해
- command 검증과 같은 tick 마지막 명령 선택
- 공격 batch가 ActorId 처리 순서에 따라 결과를 바꾸지 않음
- 경쟁 봇 우선순위와 동률 ID
- 중립 AI가 참가자만 공격

### 통합 테스트

- 같은 seed 두 경기가 같은 spawn, winner, 종료 tick, checksum을 생성
- canonical match에 참가자 24명과 중립 AI 100마리가 존재
- canonical match가 14,400에서 18,000 tick 사이 종료
- 결과에 살아 있는 참가자 한 명만 존재
- 모든 snapshot과 event ID가 유효
- simulation이 Win32와 DirectX header를 include하지 않음
- Windows WARP 자동 경기와 최종 결과 렌더 통과

## 성능과 기록

canonical match의 tick 시간과 최대 event 수를 Release에서 측정한다. 30Hz 목표에 대해 tick P95 33.3ms 이하여야 한다. 목표를 넘으면 수치를 숨기지 않고 병목과 임시 제한을 기록한다.

원본에는 commit SHA, seed, tick 수, winner, 종료 이유, actor 수, event checksum, tick P50과 P95, CPU와 compiler를 저장한다. 기존 run을 덮어쓰지 않는다.

## 완료 조건

- Windows Debug 전체 테스트 통과
- Linux GCC build와 simulation 테스트 통과
- MSVC Release build 통과
- WARP 자동 경기 결과 검증 통과
- canonical match가 8분에서 10분 사이 종료
- raw JSON과 CSV, ADR, 개발 기록의 수치가 일치
- 코드 리뷰 finding 해결
- PR Windows와 Ubuntu CI 통과
- 사용자 지시 전에는 merge하지 않음
