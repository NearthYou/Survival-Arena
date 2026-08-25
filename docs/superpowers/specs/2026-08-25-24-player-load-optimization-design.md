# 10주차 24인 부하와 snapshot 최적화 설계

- 상태: 검수 대기
- 날짜: 2026-08-25
- 기준 branch: `feat/24-player-load-optimization`
- 기준 main: `38547c89b751a54256fea245b5f68ead1a48e547`

## 목표

실제 DX11 client 1개와 같은 protocol을 사용하는 bot session 23개가 방 생성, 입장, 준비, 경기 시작, 전투와 결과까지 production 설정의 세 경기를 연속 완주한다. 기존 full-state snapshot을 기준선으로 남기고 관심 영역, 양자화와 ACK 기반 delta snapshot을 같은 seed와 입력 일정에서 비교한다.

최종 목표는 24명과 중립 AI 100개가 있는 경기에서 game server tick P95 33.3ms 이하, 경기 중 client 평균 수신량 64KiB/s 이하이다. 목표에 미달하면 실제 수치, 병목과 제한을 기록하고 통과한 것처럼 표현하지 않는다.

## 범위

이번 주차에 포함한다.

1. 하나의 bot executable에서 shared network runtime과 game session 23개 실행
2. production 설정의 세 경기 연속 자동 실행
3. 기존 full-state snapshot 기준선 계측
4. recipient별 관심 영역 grid와 hysteresis
5. 위치와 zone 반지름 16비트 양자화, 체력 8비트 양자화
6. field bitset과 visibility enter, leave 표현
7. SnapshotId ACK에 기반한 keyframe과 delta snapshot
8. 50ms 편도 지연, 2% 손실, 10ms jitter의 결정적 UDP impairment
9. server tick, encoding, fragment, client traffic, recovery와 correction 계측
10. 30분 soak와 메모리 증가 검사

이번 주차에서 제외한다.

1. reconnect와 session resume
2. UDP endpoint rebind
3. 여러 game server worker의 동시 match
4. account DB와 영구 전적
5. 여러 map과 character
6. transport 암호화
7. Linux container 배포와 외부 cloud 접속

마지막 두 항목은 11주차 범위다.

## 현재 상태

`BotCoordinator`는 lobby bot을 최대 23개 만들 수 있지만 `--play`에서는 count 1만 허용한다. play bot은 ticket을 받은 뒤 `GameSession` 하나를 만들고 30Hz input을 보낸다. `GameSession`은 각 instance가 자기 `io_context` thread를 소유한다.

`AuthoritativeMatch::EmitSnapshot`은 15Hz마다 simulation 전체를 `GameSnapshot`으로 바꾸고 한 번 encode한다. 모든 recipient는 같은 actor 124개와 loot 60개 payload를 받으며 recipient마다 다른 값은 input ACK뿐이다. payload는 1,158바이트 fragment data와 최대 32 fragment 제한을 사용한다.

현재 `ClientInput`은 input sequence와 지속 destination, attack target을 가진다. client가 적용한 SnapshotId를 server에 알리는 field는 없다. 따라서 packet 손실에 안전한 cross-snapshot delta 기준을 선택할 수 없다.

## 확정된 선택

사용자 검수로 다음을 확정했다.

1. bot process 하나가 shared `io_context`에서 실제 TCP와 UDP game session 23개를 실행한다.
2. client가 마지막으로 완성하고 적용한 SnapshotId를 ACK한다.
3. server는 ACK baseline이 있을 때만 delta를 만들고, 없으면 recipient keyframe으로 복구한다.
4. client가 base를 잃으면 world를 변경하지 않고 keyframe을 요청한다.
5. 2초마다 recipient keyframe을 보내 장시간 delta chain을 끊는다.
6. UDP impairment는 application transport seam에서 결정적으로 주입한다.
7. bot 하나의 실패도 24인 실행 전체 실패로 기록한다.

## 전체 구조

```text
dxa_client
  NetworkClientController
  GameSession
       │
       │ game TCP와 UDP
       ▼
dxa_game_server
  AuthoritativeMatch
  SnapshotReplicator
    InterestGrid
    Quantizer
    recipient baseline ring
       │
       ▼
  SnapshotFragment

dxa_bot_client
  BotCoordinator
  GameNetworkRuntime 1개
  GameSession 23개

scripts/run_network_load.ps1
  lobby server 1개
  game server 1개
  DX11 client 1개
  bot process 1개
  세 경기 순차 실행과 evidence 수집
```

## module과 seam

### GameNetworkRuntime module

`dxa_game_client`에 `GameNetworkRuntime`을 둔다. 이 module은 Boost.Asio thread, work guard와 shutdown 순서를 숨긴다. caller는 runtime을 공유할지 독점할지만 선택한다.

```cpp
class GameNetworkRuntime
{
public:
    GameNetworkRuntime();
    ~GameNetworkRuntime();
    void Start();
    void Stop();
};
```

`GameSession`은 다음 생성자를 사용한다.

```cpp
GameSession(
    dxa::simulation::NavMesh navMesh,
    std::shared_ptr<GameNetworkRuntime> runtime = {});
```

빈 runtime은 기존 DX11 client를 위해 독점 runtime을 만든다. `BotCoordinator`는 runtime 하나를 만들고 23개 `GameSession`에 같은 pointer를 넘긴다. Asio executor와 socket은 `GameNetworkRuntime`의 interface에 노출하지 않는다. `GameSession` implementation만 internal seam을 사용한다.

### SnapshotReplicator module

`dxa_game_server`에 recipient별 복제 정책을 숨기는 `SnapshotReplicator`를 둔다. `AuthoritativeMatch`는 grid, 양자화, baseline ring과 field mask를 알지 않는다.

```cpp
enum class ReplicationMode
{
    FullState,
    InterestFullPrecision,
    InterestQuantized,
    InterestDelta
};

struct ReplicationConfig
{
    ReplicationMode mode = ReplicationMode::FullState;
    float cellSize = 32.0F;
    float enterRadius = 80.0F;
    float leaveRadius = 88.0F;
    std::uint32_t keyframeIntervalSnapshots = 30U;
    std::size_t maximumBaselinesPerRecipient = 32U;
};

struct ReplicationBuild
{
    dxa::protocol::SnapshotPayload payload;
    std::uint32_t visibleActorCount = 0U;
    std::uint32_t visibleLootCount = 0U;
    bool keyframe = false;
    bool fallbackKeyframe = false;
};

class SnapshotReplicator
{
public:
    explicit SnapshotReplicator(
        const dxa::simulation::ArenaMapDefinition& arena,
        ReplicationConfig config);

    void RegisterRecipient(
        dxa::protocol::PlayerId player,
        dxa::protocol::EntityId controlledActor);
    [[nodiscard]] bool AcceptAcknowledgement(
        dxa::protocol::PlayerId player,
        std::uint32_t snapshotId);
    void RequestKeyframe(dxa::protocol::PlayerId player);
    [[nodiscard]] ReplicationBuild Build(
        dxa::protocol::PlayerId player,
        std::uint32_t snapshotId,
        const dxa::protocol::GameSnapshot& world);
    void RemoveRecipient(dxa::protocol::PlayerId player);
};
```

`Build`가 recipient의 visible set, keyframe 여부, quantized state와 delta를 한 번에 결정한다. caller는 반환 payload를 기존 fragment codec에 넘긴다. FullState mode도 같은 interface를 사용해 기준선과 최적화 경로의 timer, socket과 metrics 경계를 동일하게 유지한다. InterestFullPrecision은 관심 영역만, InterestQuantized는 관심 영역과 양자화만 적용해 단계별 효과를 분리한다.

### ClientSnapshotStream module

`dxa_game_client`에 reassembled payload를 적용하는 `ClientSnapshotStream`을 둔다. `GameSession`은 base lookup, remove ID와 delta patch 순서를 알지 않는다.

```cpp
struct SnapshotApplyResult
{
    std::optional<dxa::protocol::GameSnapshot> world;
    std::uint32_t acknowledgedSnapshotId = 0U;
    bool requestKeyframe = false;
};

class ClientSnapshotStream
{
public:
    explicit ClientSnapshotStream(std::size_t maximumBaselines = 32U);
    [[nodiscard]] SnapshotApplyResult Apply(
        std::uint32_t snapshotId,
        const dxa::protocol::SnapshotPayload& payload);
    void Reset();
};
```

base가 없거나 payload가 현재 stream과 일치하지 않으면 기존 world를 반환하지 않는다. 마지막으로 정상 적용한 SnapshotId는 input에 반복해 보낸다.

### DatagramShaper module

`dxa_protocol`의 UDP adapter 앞에 `DatagramShaper`를 둔다. production 기본 config는 disabled이고 byte를 즉시 전달한다. load run만 다음 config를 사용한다.

```cpp
struct DatagramShaperConfig
{
    std::chrono::milliseconds oneWayLatency{0};
    std::chrono::milliseconds jitter{0};
    std::uint32_t lossBasisPoints = 0U;
    std::uint32_t seed = 0U;
};
```

100 basis points는 1%다. 최종 impairment config는 latency 50ms, jitter 10ms, loss 200 basis points다. client outbound와 server outbound에 같은 config와 서로 다른 direction seed를 사용한다. TCP에는 impairment를 주입하지 않는다.

같은 seed와 datagram ordinal은 항상 같은 drop 결정과 delay를 만든다. raw bytes, ticket과 UDP token은 metrics와 log에 남기지 않는다.

## protocol version과 message 변경

wire layout이 바뀌므로 `ProtocolVersion`을 2로 올린다. version 1 binary와 version 2 binary는 연결 초기에 명시적으로 거부하며 조용히 잘못 decode하지 않는다.

### ClientInput

`ClientInput`에 다음 field를 추가한다.

```cpp
std::uint32_t acknowledgedSnapshotId = 0U;
bool requestKeyframe = false;
```

0은 아직 적용한 snapshot이 없다는 뜻이다. 정상 ACK는 단조 증가한다. 같은 값과 더 작은 값은 state를 바꾸지 않는다. server가 해당 recipient에게 발급하지 않은 미래 SnapshotId는 protocol violation이다.

client가 delta base를 찾지 못하면 기존 ACK를 감소시키지 않고 `requestKeyframe=true`를 보낸다. server는 다음 15Hz 전송에서 keyframe을 만들고 flag를 소비한다.

### SnapshotPayload

fragment metadata는 기존 SnapshotId, server tick, input ACK, fragment index, count, 전체 길이와 CRC32를 유지한다. delta metadata는 reassembled payload header에 둬 fragment data 상한을 바꾸지 않는다.

```cpp
enum class SnapshotPayloadKind : std::uint8_t
{
    FullState = 1,
    Keyframe = 2,
    Delta = 3
};

struct SnapshotPayloadHeader
{
    SnapshotPayloadKind kind;
    std::uint32_t baseSnapshotId = 0U;
    std::uint32_t payloadSnapshotId = 0U;
};
```

FullState와 Keyframe은 baseSnapshotId 0만 허용한다. Delta는 1 이상이며 recipient가 ACK한 baseline과 정확히 같아야 한다. payloadSnapshotId는 fragment SnapshotId와 같아야 한다.

## 관심 영역

map 1은 256×256 평면이다. `InterestGrid`는 32m cell의 8×8 grid를 사용한다. recipient가 조작하는 actor 위치를 중심으로 enter radius 80m 안에 들어온 actor와 active loot를 보낸다. 이미 visible인 객체는 leave radius 88m를 벗어날 때까지 유지해 cell과 radius 경계에서 enter, leave가 반복되는 현상을 막는다.

항상 relevant인 값은 다음과 같다.

1. recipient의 controlled actor
2. match phase와 safe-zone state
3. alive contender count
4. match result
5. event checksum

다른 contender, neutral AI와 loot는 관심 영역을 적용한다. dead actor가 visible set 안에서 사망하면 사망 state를 한 번 전송한 뒤 leave 조건까지 유지한다. 관심 영역에 새로 들어온 객체는 모든 immutable field와 mutable field를 보낸다. 영역에서 나간 객체는 removed ID에 넣는다.

grid 결과는 actor와 loot ID 오름차순으로 정렬한다. insertion order와 unordered container 순서가 wire checksum을 바꾸지 않는다.

## 양자화

position은 arena minimum과 maximum을 기준으로 각 축을 uint16 범위에 mapping한다. map 1의 256m span에서 한 step은 약 0.00391m다. decode 오차는 반 step 이하여야 한다. arena 밖 position과 non-finite float는 encode 전에 거부한다.

safe-zone radius는 0m부터 arena width 절반까지 uint16으로 mapping한다. health는 0부터 100까지 uint8로 저장한다. cooldown tick은 uint16, eliminations는 uint8로 저장하며 범위를 넘으면 encode를 실패시킨다.

ID, match phase, role, archetype, weapon과 result reason은 기존 정수 폭을 유지한다. 양자화는 위치와 bounded gameplay 수치에만 적용한다.

## field bitset

actor keyframe과 enter record는 전체 field를 가진다. 기존 visible actor의 delta는 다음 bit를 사용한다.

```text
bit 0  position
bit 1  health와 alive
bit 2  weapon과 cooldown
bit 3  eliminations
```

role과 neutral archetype은 entity lifetime 동안 immutable이다. delta에서 바뀌면 protocol error다.

loot keyframe과 enter record는 ID, type, position과 active를 가진다. 기존 visible loot는 active bit만 delta로 보낸다. removed actor와 removed loot는 별도 정렬된 ID vector로 보낸다.

global state는 다음 bit를 사용한다.

```text
bit 0  phase
bit 1  safe-zone stage, center와 radius
bit 2  alive contender count
bit 3  result
bit 4  event checksum
```

field mask가 0인 record는 encode하지 않는다. 동일 quantized state는 변경으로 보지 않는다.

## baseline lifecycle

server는 recipient마다 적용 후 world view를 SnapshotId와 함께 최대 32개 보관한다. client ACK가 증가하면 그보다 오래된 baseline을 제거하되 ACK 대상은 유지한다.

다음 조건에서는 keyframe을 만든다.

1. 첫 snapshot
2. client의 keyframe request
3. ACK가 0
4. ACK baseline이 ring에 없음
5. 이전 keyframe 이후 snapshot 30개 경과
6. visible set 또는 encode state가 내부 invariant를 위반해 안전한 delta를 만들 수 없음

마지막 조건은 `fallbackKeyframe` metric을 증가시킨다. keyframe도 encode할 수 없으면 match를 일부 state로 진행하지 않고 load run을 실패 처리한다.

client는 적용한 keyframe과 delta 결과를 최대 32개 보관한다. Delta base가 없으면 payload를 폐기하고 keyframe을 요청한다. CRC 실패와 불완전 fragment는 `ClientSnapshotStream`에 전달하지 않으므로 ACK가 전진하지 않는다.

## server tick 흐름

30Hz tick 순서는 다음을 유지한다.

1. 새 input sequence와 SnapshotId ACK 반영
2. keyframe request 반영
3. disconnect command 반영
4. `OfflineMatch::Step`
5. result 판정
6. 짝수 tick에서 world snapshot 한 번 생성
7. InterestGrid rebuild
8. connected recipient별 `SnapshotReplicator::Build`
9. payload fragment와 UDP enqueue
10. metrics 누적

world snapshot과 grid는 tick마다 한 번만 만든다. quantized recipient view와 delta encode만 recipient별로 수행한다. 24명 기준 복제 encode 비용은 server tick metric과 별도 metric으로 기록한다.

## client 적용 흐름

`GameSession`은 30Hz마다 last applied SnapshotId와 keyframe request를 `ClientInput`에 넣는다. reassembler가 complete payload를 만들면 `ClientSnapshotStream::Apply`를 호출한다.

정상 world가 반환된 경우에만 predictor reconciliation과 remote interpolation buffer를 갱신한다. local actor가 interest filter에서 빠지는 것은 server invariant violation이다. remote actor leave는 interpolation buffer에서도 제거한다. 다시 enter하면 새 identity sample처럼 시작하고 leave 이전 velocity를 사용하지 않는다.

## 23 bot 실행

`BotClientOptions`는 play mode에서도 count 1부터 23을 허용한다. `BotCoordinator`는 lobby connection 23개를 유지하고 ticket마다 shared `GameNetworkRuntime`을 사용하는 `GameSession`을 만든다.

각 bot은 PlayerId를 seed에 섞은 deterministic destination schedule을 사용한다. 같은 match seed와 PlayerId는 같은 input sequence와 destination을 만든다. bot별 실패를 하나의 global atomic error로 덮지 않고 `BotSessionReport`에 보존한다.

```cpp
struct BotSessionReport
{
    dxa::protocol::PlayerId player;
    std::optional<dxa::protocol::MatchId> match;
    std::uint64_t snapshotsApplied = 0U;
    std::uint64_t keyframesApplied = 0U;
    std::uint64_t deltasApplied = 0U;
    std::uint64_t receivedTcpBytes = 0U;
    std::uint64_t receivedUdpBytes = 0U;
    std::uint64_t discardedSnapshots = 0U;
    std::uint64_t keyframeRequests = 0U;
    int exitCode = 0;
};
```

`BotCoordinator`는 전체 session report와 공통 `GameMatchResult`를 반환한다. 23개 result의 MatchId, winner, reason과 finished tick이 다르면 load run을 실패시킨다.

## 세 경기 자동화

Windows PowerShell script `scripts/run_network_load.ps1`이 실행을 소유한다. 한 실행은 다음 process를 사용한다.

1. lobby server 1개
2. game server 1개
3. DX11 client 1개
4. bot client 1개, 내부 game session 23개

server process는 세 경기 동안 유지한다. 각 경기마다 DX11 client와 bot process를 새로 시작해 client-side state가 다음 경기에 남지 않게 한다. fresh client가 출력한 실제 RoomId를 script가 읽고 bot에 전달한다. RoomId를 1로 가정하지 않는다.

자동 gate의 DX11 client는 WARP hidden hybrid-deferred mode를 사용하지만 실제 `EngineApp`, `NetworkClientController`와 render scene adapter를 통과한다. RTX 3050 Ti hardware 검수는 최적화 mode의 대표 경기 한 번을 별도로 수행한다.

production match config와 중립 AI 100개를 사용한다. 빠른 회귀 fixture만 생성자 주입으로 짧은 timeout을 사용하며 load script에 short-match CLI를 추가하지 않는다.

## replication mode

game server CLI에 다음 option을 추가한다.

```text
--replication-mode full-state
--replication-mode interest-full
--replication-mode interest-quantized
--replication-mode interest-delta
--metrics-output-root <directory>
```

기준선은 full-state, 단계별 비교는 interest-full과 interest-quantized, 최종 최적화는 interest-delta다. `GameServerWelcome`은 선택한 mode를 전달하고 client가 지원하지 않으면 UDP bind 전에 종료한다.

기준선과 최적화 run은 다음 값을 같게 유지한다.

1. commit SHA
2. match seed 세 개
3. bot input schedule
4. production match config
5. process topology
6. render resolution과 path
7. warm-up과 measurement 구간

mode만 다르게 한다.

## impairment 실행

CLI는 공통 impairment profile을 받는다.

```text
--udp-latency-ms 50
--udp-jitter-ms 10
--udp-loss-basis-points 200
--network-seed 20260825
```

모든 값이 0이면 shaper는 allocation과 timer를 만들지 않고 즉시 전달한다. load script는 no-impairment 기준 run 뒤 위 값을 적용한 run을 실행한다.

drop은 datagram ordinal 기준으로 결정하며 fragment 단위로 적용한다. jitter 결과가 음수가 되면 0ms로 clamp한다. 같은 peer와 direction 안에서는 delivery time이 같을 때 ordinal 순서를 유지한다. delayed datagram 총량은 peer당 256개로 제한한다. 초과하면 오래된 packet을 숨기지 않고 load run failure와 counter로 기록한다.

## metrics

경기 traffic 측정 구간은 `GameServerWelcome` 수신부터 `GameMatchResult` 수신까지다. lobby room list와 ticket traffic은 client 평균 수신량에서 제외하고 별도 byte counter로 남긴다.

server는 match마다 다음 값을 JSON과 raw CSV에 기록한다.

1. tick duration sample과 평균, P95, maximum
2. replication encode duration sample과 평균, P95, maximum
3. recipient별 payload bytes와 UDP datagram bytes
4. snapshot, keyframe, delta와 fallback keyframe count
5. fragment count histogram
6. visible actor와 loot count
7. scheduler overrun count와 lateness
8. impairment drop, delay와 queue overflow count
9. process working set sample

client와 bot은 다음 값을 기록한다.

1. game TCP와 UDP 수신 bytes
2. 적용한 keyframe과 delta count
3. CRC, incomplete, missing-base와 stale snapshot 폐기 count
4. keyframe request count
5. snapshot queue drop count
6. prediction correction 거리 sample과 P95
7. result와 exit code

수신량은 측정 구간의 총 game bytes를 wall time으로 나눈 KiB/s다. 평균은 24 client의 합을 24로 나누고, recipient별 P95도 별도로 기록한다.

## evidence directory

원본은 덮어쓰지 않는다.

```text
docs/benchmarks/network-load/
  <timestamp>-<commit>-<mode>-seed<seed>/
    environment.json
    command.txt
    server-ticks.csv
    replication.csv
    clients.csv
    summary.json
    RESULT.md
```

`environment.json`은 OS, CPU, RAM, GPU, build config, commit SHA, dirty status, map ID, protocol version, match seed, network seed와 impairment config를 포함한다. dirty worktree에서는 공식 run을 거부한다.

## 오류 처리

1. SnapshotId 0은 ACK할 수 없다.
2. 같은 ACK와 오래된 ACK는 무시한다.
3. server가 발급하지 않은 미래 ACK는 protocol violation이다.
4. ACK baseline이 없으면 keyframe을 보내고 connection은 유지한다.
5. client가 base를 잃으면 world를 변경하지 않고 keyframe을 요청한다.
6. payload kind와 base ID 조합이 잘못되면 session을 종료한다.
7. quantization 범위 밖 값은 clamp하지 않고 encode failure다.
8. interest query에서 local actor가 빠지면 server failure다.
9. payload가 37,056바이트 또는 32 fragment를 넘으면 일부만 보내지 않는다.
10. bot session 하나의 auth, UDP bind, snapshot, result 실패는 전체 load run failure다.
11. 세 경기 중 하나라도 result 불일치, timeout 또는 process crash가 있으면 공식 결과를 만들지 않는다.
12. secret, token과 packet raw bytes는 log와 evidence에 남기지 않는다.

## 테스트 전략

### protocol test

1. ProtocolVersion 2 exact byte
2. ClientInput SnapshotId ACK와 keyframe request round trip
3. payload kind와 base ID 조합
4. quantized coordinate, health와 radius 경계
5. actor, loot와 global field mask exact byte
6. enter, update, remove record bounded count
7. trailing byte, duplicate ID와 non-canonical order 거부
8. 1,200바이트와 32 fragment 경계 유지

### replication test

1. InterestGrid가 같은 seed의 brute-force radius query와 일치
2. 80m enter와 88m leave 경계
3. local actor와 global state always relevant
4. keyframe 뒤 변화 없는 delta가 빈 entity record를 만들지 않음
5. quantized state가 같은 float 변화는 delta를 만들지 않음
6. enter는 full record, leave는 remove ID
7. ACK baseline delta reconstruction이 keyframe quantized state와 일치
8. unknown과 expired ACK가 fallback keyframe 생성
9. 30 snapshot마다 periodic keyframe
10. recipient baseline ring 32개 상한

### client stream test

1. keyframe 적용과 ACK 전진
2. delta base 적용과 ACK 전진
3. missing base에서 무상태 폐기와 keyframe request
4. stale와 duplicate SnapshotId 무시
5. remove ID가 interpolation actor를 제거
6. re-enter actor가 이전 sample을 재사용하지 않음
7. history 32개 상한

### impairment test

1. 같은 seed와 ordinal의 drop과 delay 반복
2. 0 config 즉시 전달
3. 2% loss의 고정 sample exact count
4. jitter가 latency 범위를 지킴
5. 양방향 seed 분리
6. peer당 256 queue 상한과 overflow failure

### integration test

1. 실제 lobby와 game TCP, UDP에서 24 participant 인증
2. 23 bot session이 같은 shared runtime 사용
3. full-state와 interest-delta가 같은 result 생성
4. incomplete delta 뒤 keyframe 복구
5. 잘못된 미래 ACK protocol error
6. 100ms RTT, 2% loss, 10ms jitter에서 result 수신
7. 한 bot failure가 coordinator failure로 전파
8. 한 worker에서 세 match 순차 완료

Windows integration은 WARP client를 포함한다. Linux integration은 headless client 24개를 사용한다.

## 검증 gate

빠른 gate는 모든 commit에서 실행한다.

1. protocol, replication, client stream과 shaper unit test
2. 짧은 24-session integration
3. 기존 full-state regression
4. Windows WARP smoke

마일스톤 gate는 Release build의 clean commit에서 실행한다.

1. full-state 기준선 한 경기
2. interest-only 한 경기
3. interest와 quantization 한 경기
4. interest, quantization과 delta 한 경기
5. 최종 mode production 세 경기
6. impairment production 한 경기
7. Windows 30분 soak와 working set 추적
8. Linux AddressSanitizer headless 24-session 30분 soak

각 단계는 이전 단계와 같은 seed를 사용한다. 개선이 없거나 server CPU 비용이 과도한 단계도 결과에서 제거하지 않는다.

## 완료 조건

1. DX11 client 1개와 bot session 23개가 같은 MatchId로 인증한다.
2. production 설정의 세 경기를 연속 완주한다.
3. full-state와 최적화 mode의 match result가 seed별로 같다.
4. 30Hz server tick과 15Hz snapshot 계약을 유지한다.
5. client 평균 경기 수신량이 64KiB/s 이하이다.
6. server tick P95가 33.3ms 이하이다.
7. 100ms RTT, 2% loss와 10ms jitter에서 protocol error 없이 끝난다.
8. 30분 soak에서 crash, sanitizer 오류, 탐지 가능한 leak과 단조 메모리 증가가 없다.
9. secret leak count가 0이다.
10. 기준선과 최적화 raw evidence가 서로 다른 immutable directory에 있다.
11. Windows Debug, Release와 Linux server build가 통과한다.
12. README와 개발 기록이 실제 command, 수치와 제한을 반영한다.

목표 수치를 달성하지 못해도 측정과 원인 분석이 완전하면 구현 사실을 숨기지 않는다. 다만 목표 미달 상태를 최적화 성공이나 공개 준비 완료로 표시하지 않는다.

## commit과 review 경계

protocol, shared runtime, replication, impairment, load harness, metrics와 문서는 독립적인 RED와 GREEN commit으로 나눈다. commit 수를 맞추기 위한 분할은 하지 않는다.

모든 commit 제목은 한국어 명사형 Conventional Commit을 사용하고 본문에 `이유`, `핵심 변경`, `검증`을 남긴다. full Week 10 diff는 protocol allocation bound, ACK 신뢰 경계, baseline memory 상한, interest visibility, quantization error, timer shutdown, bot shared runtime lifetime와 metrics 정확성을 검토한다.

최종 PR은 billing gate가 해제돼 Windows와 Ubuntu CI가 실제로 실행된 뒤에만 merge-ready로 표시한다. billing 문제로 hosted CI가 시작되지 않으면 local Windows와 Docker Linux 증거를 분리해 기록하고 사용자의 별도 병합 지시 없이는 병합하지 않는다.

## 남은 한계

interest radius 80m는 map 1과 현재 camera에 고정된 첫 contract다. 다른 map, zoom과 장거리 weapon을 추가할 때 visibility policy를 다시 결정해야 한다.

ACK 기반 delta는 recipient별 memory와 encode 비용을 늘린다. 24명에서는 bounded지만 한 worker가 여러 match를 실행하는 구조로 확장할 때 memory budget을 다시 측정해야 한다.

TCP impairment, reconnect와 session resume는 없다. reliable result는 정상 TCP를 사용하고 gameplay UDP만 impairment를 적용한다.

이번 metrics는 한 PC에서 client와 server를 함께 실행한 결과다. 11주차 cloud test에서는 server CPU와 network RTT를 분리해 다시 측정한다.
