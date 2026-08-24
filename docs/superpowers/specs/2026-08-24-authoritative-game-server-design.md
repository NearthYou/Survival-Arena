# 9주차 권위형 게임 서버 설계

## 문서 상태

이 문서는 9주차 구현 전 설계 계약이다. 아래 수치와 흐름은 구현 목표이며 아직 측정 결과나 완료 증거가 아니다. 실제 개발 기록에는 구현 중 재현한 실패와 검증 결과만 추가한다.

## 목적

8주차 로비가 발급한 match ticket을 실제 game server worker가 소비하게 만든다. 로비는 worker를 등록하고 한 경기를 비동기로 예약한다. game server는 `dxa_simulation`을 30Hz 고정 tick으로 실행하고, client input을 검증한 뒤 15Hz UDP snapshot을 보낸다.

이번 주차의 수직 기능은 실제 DX11 client 1개와 headless client 1개가 같은 방에서 시작해 같은 game server에 인증하고, 이동 input과 snapshot을 주고받고, 연결 종료에 따른 탈락과 경기 결과까지 확인하는 것이다. 24명 부하, 관심 영역, 양자화, 변경 bitset과 손실 주입은 10주차로 남긴다.

## 선택한 접근

### 선택: lobby, worker control, game session 분리

로비 공개 TCP는 방과 준비 상태를 처리한다. 별도 worker control TCP는 game server 등록과 match reservation만 처리한다. game TCP는 ticket 인증과 신뢰성 결과를 담당하고 game UDP는 bind, input과 snapshot을 담당한다.

```text
DX11 client ── lobby TCP 7000 ──> lobby server
             └─ game TCP 7100 ──> game server worker
             └─ game UDP 7101 ──> game server worker

headless client ── 같은 protocol과 transport ──> lobby/game server

game server worker ── control TCP 7001 ──> lobby server
```

game server process 하나는 동시에 한 match만 실행한다. 경기가 끝나면 같은 process가 Idle로 돌아가 다음 match를 받을 수 있지만 여러 match를 동시에 실행하지 않는다.

이 구조는 lobby 장애, reservation 실패와 game simulation 실패를 서로 다른 경계에서 관찰할 수 있게 한다. 11주차에 lobby와 worker를 서로 다른 host에 배치할 때도 공개 game endpoint와 내부 control endpoint를 구분할 수 있다.

### 제외: lobby process 안에서 simulation 실행

초기 연결 코드는 줄지만 room 상태와 30Hz simulation 부하가 같은 event loop를 공유한다. 한 경기의 tick 지연이나 예외가 모든 room에 전파되고, worker 배정 실패를 실제 경계로 검증할 수 없으므로 제외한다.

### 제외: worker 하나에서 여러 match 동시 실행

process 수는 줄지만 match별 timer, UDP routing과 failure isolation이 복잡해진다. v1의 목표는 한 match의 권위형 경계를 정확히 만드는 것이므로 capacity를 1로 고정한다.

### 제외: UDP만 사용하는 인증

UDP 첫 packet에 match ticket을 직접 싣는 방식은 spoofing 대응과 오류 전달이 어렵다. ticket은 game TCP에서 한 번 소비하고, 인증된 TCP session에 별도 UDP token을 발급한다.

## 디렉터리와 target

```text
protocol/
  include/dxa/protocol/
  src/

apps/
  lobby_server/
  lobby_client/
  game_server/
  game_client/
  bot_client/
  client/

simulation/
tests/
```

- `dxa_protocol`: worker control, game TCP, game UDP 값 type과 bounded codec
- `dxa_protocol_asio`: 기존 framed TCP connection 재사용
- `dxa_lobby_core`: pending reservation을 포함한 room transaction
- `dxa_lobby_server_core`: 공개 lobby acceptor, worker control acceptor와 registry
- `dxa_game_server_core`: ticket store, fixed tick runner, UDP session과 snapshot fan-out
- `dxa_game_server`: Windows와 Linux console worker executable
- `dxa_game_client`: lobby 이후 game 인증, UDP transport, prediction과 interpolation을 공유하는 플랫폼 중립 library
- `dxa_bot_client`: 기존 lobby bot에 선택적 game play mode 추가
- `dxa_client`: Windows DX11 client가 `dxa_game_client`를 조합하는 executable

`dxa_engine`은 Boost.Asio, lobby와 game protocol을 참조하지 않는다. `dxa_simulation`도 Win32, DirectX와 socket을 참조하지 않는다.

## module seam과 test surface

`LobbyService`의 외부 interface는 client event, disconnect와 worker event를 받아 `LobbyServiceResult`를 돌려주는 데 집중한다. reservation 선택, timeout과 TCP write는 worker control adapter 안에 숨긴다. 8주차의 동기 allocator 위에 비동기 wrapper를 한 겹 더 올리지 않는다.

`LobbyService`에 들어가는 worker event는 `ReservationReady`, `ReservationFailed`, `MatchFinished`, `MatchUnavailable` variant다. service가 내보내는 `LobbyRuntimeAction`은 `ReserveMatch`, `CancelReservation` variant다. register, cancel ACK와 connection close는 `WorkerRegistry`가 내부 상태로 처리하고 필요한 domain event만 service에 전달한다. pure unit test는 같은 event와 action seam을 사용하고 내부 map을 직접 검사하지 않는다. production adapter는 action을 framed TCP로 바꾸며 loopback test는 실제 adapter를 사용한다.

`dxa_game_server_core`의 match module은 reservation 하나를 받아 authentication, datagram, disconnect와 clock advance event를 처리하고 outbound TCP, UDP와 worker control action을 반환한다. socket callback마다 simulation map을 직접 수정하지 않는다. clock과 보안 난수 source만 생성자에서 받으며 protocol codec, NavMesh 검증, ticket consume와 fixed tick ordering은 module 안에 둔다.

`dxa_game_client`는 transport, `ClientPredictor`, `SnapshotReassembler`, `RemoteInterpolator`를 조합하지만 app에 mutable 내부 state를 노출하지 않는다. app이 알아야 하는 interface는 session 시작, destination 제출, scene polling과 종료뿐이다. predictor와 reassembler의 pure interface는 unit test에서도 그대로 사용한다.

## 고정 상수와 ID

| 계약 | 값 |
| --- | ---: |
| lobby 공개 TCP 기본 port | 7000 |
| worker control TCP 기본 port | 7001 |
| game TCP 기본 port | 7100 |
| game UDP 기본 port | 7101 |
| worker 동시 match capacity | 1 |
| reservation 응답 제한 | 2초 |
| game TCP 인증 제한 | 5초 |
| match ticket 수명 | 60초 |
| server simulation | 30Hz |
| snapshot 전송 | 15Hz |
| 한 번에 허용하는 catch-up tick | 5 |
| UDP datagram 전체 크기 | 최대 1,200바이트 |
| snapshot 최대 fragment 수 | 32 |
| client interpolation 지연 | 100ms, server tick 3개 |
| client input history | 최대 256개 |
| client snapshot buffer | 최대 32개 |

기존 `PlayerId`, `RoomId`, `MatchId`, `EntityId`에 다음 strong ID를 추가한다.

```cpp
struct WorkerId { std::uint32_t value; };
struct ReservationId { std::uint64_t value; };
```

`WorkerId`와 `ReservationId`의 0은 wire에서 거부한다. lobby가 만드는 `ReservationId`는 process lifetime 동안 증가하고 재사용하지 않는다. 소진 시 새 시작 요청을 `IdSpaceExhausted`로 거부한다.

기존 `LobbyError` 값 1부터 19는 바꾸지 않고 `MatchUnavailable = 20`을 추가한다. active worker 전체가 사라진 경우에만 사용하며 개별 game client disconnect에는 사용하지 않는다.

기존 lobby namespace의 `MatchTicketValue`는 `dxa::protocol`로 이동한다. UDP session token도 같은 크기의 별도 type으로 둬 ticket과 혼용하지 않는다.

두 type은 내부에 16바이트 배열을 소유하는 서로 다른 tagged value type이다. byte span, index와 비교 연산만 공개하며 서로 암시적으로 변환되지 않는다.

두 값은 production에서 Windows `BCryptGenRandom`, Linux `getrandom`으로 생성한다. 값 자체는 log, console, test failure message와 문서에 출력하지 않는다.

## worker 생명주기

```text
Disconnected -> Registering -> Idle -> Reserved -> Active -> Idle
                                   \-> Cancelling -> Idle
```

game server가 lobby의 worker control port로 persistent TCP 연결을 시작한다. 첫 frame은 반드시 `WorkerRegister`다.

등록 정보는 다음을 포함한다.

- nonzero `WorkerId`
- client에게 광고할 ASCII host, 최대 255바이트
- game TCP port와 UDP port
- capacity, 반드시 1

빈 host, port 0, capacity 1이 아닌 값, 이미 등록된 WorkerId와 중복 endpoint는 거부하고 연결을 닫는다. 성공하면 `WorkerRegistered`를 응답하고 worker를 Idle로 표시한다. Idle worker가 여러 개면 가장 작은 WorkerId를 선택한다.

v1은 별도 heartbeat를 두지 않는다. control TCP close가 liveness 신호다. Reserved worker가 끊기면 pending start를 실패시킨다. Active worker가 끊기면 해당 match를 중단하고 room을 정리한다. 이후 worker process가 다시 연결하면 새 등록으로 취급한다.

reservation timeout이 나면 lobby는 해당 control connection을 닫는다. timeout worker를 바로 Idle로 되돌리지 않으므로 늦은 ready와 다음 reservation이 같은 process에서 겹치지 않는다. game server는 control disconnect 시 아직 client에게 공개되지 않은 reservation을 폐기하고 다시 등록한다.

## 비동기 match 시작 transaction

8주차의 동기 `IGameWorkerAllocator::Allocate` 호출은 실제 worker ACK를 표현할 수 없다. runtime 경로에서 이를 제거하고 `LobbyService`가 action을 반환하는 형태로 바꾼다.

`StaticGameWorkerAllocator`와 lobby server의 `--worker-host`, `--worker-tcp-port`, `--worker-udp-port` option도 production 경로에서 제거한다. 8주차의 성공 fixture는 새 event와 action seam을 사용하는 deterministic service test로 교체한다. 이전 static endpoint가 실제 worker처럼 남아 두 시작 경로가 공존하지 않게 한다.

```cpp
struct MatchReservationAction
{
    ReservationId reservation;
    RoomId room;
    MatchId match;
    PlayerId requester;
    std::vector<ReservedParticipant> participants;
};

struct CancelReservationAction
{
    ReservationId reservation;
};

struct LobbyServiceResult
{
    std::vector<OutboundMessage> outbound;
    std::vector<LobbyAuditEvent> audit;
    std::vector<LobbyRuntimeAction> actions;
};
```

`LobbyService`는 socket을 알지 않는다. `LobbyTcpServer`와 `WorkerRegistry`를 조합하는 runtime coordinator가 action을 worker frame으로 바꾸고, 결과를 다시 service의 `CompleteReservation`에 전달한다. unit test는 socket 없이 action과 완료 결과를 주입한다.

시작 순서는 다음과 같다.

1. host의 `StartMatchRequest`를 기존 room 규칙으로 검증한다.
2. lobby가 Room을 Starting으로 바꾸고 참가자 전체에 Starting snapshot을 보낸다.
3. lobby가 MatchId, ReservationId와 참가자별 서로 다른 ticket을 만든다.
4. `WorkerRegistry`가 가장 작은 Idle worker를 Reserved로 바꾸고 `ReserveMatch`를 보낸다.
5. game server가 participant와 ticket 경계를 검증하고 `OfflineMatch`를 생성한 뒤 `Start`로 spawn을 검증한다.
6. game server가 ticket을 pending store에 넣은 뒤 `ReserveMatchReady`를 보낸다.
7. lobby가 같은 ReservationId의 ready를 받으면 room을 InMatch로 바꾸고 worker를 Active로 바꾼다.
8. lobby가 InMatch snapshot과 참가자별 `MatchTicket`을 보낸다.

ticket 수명은 lobby가 `ReserveMatch`를 보낸 시점과 game server가 이를 받은 시점의 각 process `steady_clock`에서 시작한다. lobby는 ready를 받을 때 실제 남은 초를 내림해 `MatchTicket.expiresInSeconds`에 기록한다. ready가 도착했을 때 남은 시간이 1초 미만이면 성공시키지 않고 rollback한다.

다음 조건은 모두 같은 실패 경로를 사용한다.

- Idle worker 없음
- ticket 생성 실패
- control write 실패
- `ReserveMatchRejected`
- 2초 timeout
- Reserved worker disconnect

실패 시 ticket을 revoke하고 room을 Waiting으로 되돌린다. 기존 ready 값은 유지한다. 참가자 전체에 Waiting snapshot을 먼저 보내고, 연결이 남아 있다면 시작을 요청한 host에게 `WorkerUnavailable`을 보낸다.

Starting 동안 ready, leave, join과 중복 start 같은 command는 `RoomNotJoinable`로 거부한다. TCP disconnect는 command와 다르게 lifecycle 정리로 처리한다. Starting 참가자가 끊기면 reservation을 취소하고 Waiting으로 rollback한 다음 해당 참가자를 room에서 제거한다. host였다면 남은 참가자 중 입장 순번이 가장 빠른 player로 권한을 넘긴다. 마지막 참가자였다면 room을 삭제한다.

취소된 reservation의 worker는 Cancelling 상태에서 `MatchReservationCancelled`를 기다린다. 2초 안에 응답하지 않으면 control connection을 닫는다. room rollback은 취소 ACK를 기다리지 않는다. 취소 ticket은 client에게 전달되지 않았으므로 stale worker가 game 인증에 성공시킬 경로가 없다.

같은 io_context thread에서 timeout과 ready를 직렬 처리한다. 먼저 처리된 event만 상태를 바꾸며, 이미 완료되거나 취소된 ReservationId의 늦은 응답은 상태를 바꾸지 않는다.

## worker control TCP protocol

기존 12바이트 TCP frame과 64KiB 제한을 그대로 사용한다. `MessageType`에 다음 값을 고정한다.

| 값 | 방향 | message |
| ---: | --- | --- |
| 13 | worker to lobby | `WorkerRegister` |
| 14 | lobby to worker | `WorkerRegistered` |
| 15 | lobby to worker | `ReserveMatch` |
| 16 | worker to lobby | `ReserveMatchReady` |
| 17 | worker to lobby | `ReserveMatchRejected` |
| 18 | lobby to worker | `CancelMatchReservation` |
| 19 | worker to lobby | `MatchReservationCancelled` |
| 20 | worker to lobby | `MatchFinished` |
| 21 | game client to worker | `GameClientHello` |
| 22 | worker to game client | `GameServerWelcome` |
| 23 | worker to game client | `GameServerError` |
| 24 | worker to game client | `GameMatchResult` |

worker control과 game TCP는 message variant와 codec을 분리한다. 잘못된 channel에서 유효한 다른 channel의 message type을 보내면 protocol violation으로 연결을 닫는다.

`ReserveMatch` payload는 ReservationId, MatchId, match seed, ticket lifetime milliseconds와 참가자 목록을 가진다. 참가자 목록은 PlayerId 오름차순이며 2명부터 24명까지 허용한다. 각 원소는 PlayerId와 16바이트 ticket이다. game endpoint는 이미 등록 정보에 있으므로 reservation마다 반복하지 않는다.

`ReserveMatchReady`와 `ReserveMatchRejected`는 ReservationId와 MatchId를 모두 포함한다. 두 값 중 하나라도 pending record와 다르면 control connection을 protocol violation으로 닫는다. 공개 reject frame은 제한된 enum만 보내고 내부 exception 문자열은 보내지 않는다.

`ReserveMatchRejected`의 공개 이유는 `Busy = 1`, `InvalidReservation = 2`, `SimulationInitializationFailed = 3`, `InternalError = 4`로 고정한다. 어떤 이유든 lobby participant에게는 기존 `WorkerUnavailable`만 전달한다.

`MatchFinished`는 MatchId, optional winner ActorId, 종료 이유와 finished tick을 보낸다. 종료 이유는 `LastSurvivor`, `TimeLimit`, `NoAuthenticatedPlayers`, `NoConnectedPlayers` 중 하나다. lobby는 Active worker를 Idle로 돌리고 InMatch room을 삭제하며 participant의 room index를 지운다. 연결이 남은 participant에게 최신 room list를 push한다.

Active worker가 끊긴 경우 lobby는 room을 삭제하고 연결이 남은 participant에게 새 `MatchUnavailable` error와 room list를 보낸다. game TCP 연결 종료에 따른 한 참가자의 탈락과 worker process 전체 disconnect를 구분한다.

## game TCP 인증

game TCP accept 후 5초 안에는 `GameClientHello`만 허용한다.

```cpp
struct GameClientHello
{
    MatchId match;
    PlayerId player;
    MatchTicketValue ticket;
};
```

game server ticket store는 reservation에서 받은 MatchId, PlayerId, ticket과 local expiry를 저장한다.

- 존재하지 않는 ticket은 실패
- MatchId나 PlayerId가 다르면 실패하며 ticket은 소비하지 않음
- 만료된 ticket은 제거하고 실패
- 정확히 일치한 ticket은 한 번만 소비
- 이미 game TCP session이 연결된 PlayerId는 실패
- 재사용 ticket은 실패

wire 응답은 위 실패 원인을 구분하지 않고 `GameServerError::AuthenticationFailed`만 보낸다. 내부 unit test와 metric counter는 not found, mismatch, expired, reused를 구분한다.

공개 `GameServerError` 값은 `AuthenticationFailed = 1`, `ServerNotReady = 2`, `ProtocolViolation = 3`, `InternalError = 4`로 고정한다. malformed TCP header처럼 frame 경계를 신뢰할 수 없는 경우에는 error frame 없이 연결을 닫는다.

성공하면 PlayerId 오름차순 participant index를 그대로 `ActorId` 0부터 N-1에 대응시키고 `GameServerWelcome`을 보낸다.

```cpp
struct GameServerWelcome
{
    MatchId match;
    PlayerId player;
    ActorId actor;
    std::uint16_t tickRate;
    std::uint16_t snapshotRate;
    std::uint32_t mapId;
    std::uint32_t navMeshCrc32;
    UdpSessionToken udpToken;
};
```

v1 mapId는 1이다. NavMesh CRC32는 vertex와 triangle index를 정해진 little-endian 순서로 직렬화한 canonical bytes에 계산한다. client의 bundled NavMesh와 값이 다르면 UDP bind 전에 session을 종료한다. CRC32는 asset 보안 검증이 아니라 client와 server prediction data의 불일치 탐지용이다.

UDP token은 game TCP 인증 성공 후 새로 생성한다. match ticket을 UDP token으로 재사용하지 않는다. game TCP는 경기 동안 유지한다. 인증된 game TCP가 끊기면 server는 해당 contender를 다음 simulation tick에 탈락 처리한다. UDP packet이 잠시 오지 않는 것만으로 탈락시키지 않는다.

경기가 끝나면 연결된 client에게 `GameMatchResult`를 보낸 뒤 session write queue를 비우고 닫는다. reconnect와 session resume은 v1에서 제공하지 않는다.

경기 시작 후 인증된 game TCP가 모두 끊기면 `OfflineMatch`에 마지막 contender의 disconnect command를 제출하지 않는다. worker는 optional winner 없이 `NoConnectedPlayers`로 match를 종료하고 lobby에 `MatchFinished`를 보낸다. 연결이 하나 이상 남아 있을 때만 끊긴 contender를 simulation에서 탈락시켜 가짜 생존자를 만들지 않는다.

### simulation 시작 gate

`ReserveMatchReady`는 `OfflineMatch::Start`가 spawn을 성공시키고 ticket store 준비까지 끝났다는 뜻이며, 30Hz 실행 시작 신호가 아니다. `Start` 직후에는 timer와 `Step`을 실행하지 않으므로 중립 AI와 zone도 진행되지 않는다. game server는 participant마다 `Pending`, `Authenticated`, `Unavailable` slot 상태를 유지한다.

- ticket 인증 성공 시 `Authenticated`
- ticket 만료 시 `Unavailable`
- 인증 후 game TCP가 시작 전에 끊기면 `Unavailable`
- 소비된 ticket은 다시 `Pending`으로 돌아가지 않음

모든 slot이 `Authenticated` 또는 `Unavailable`로 확정될 때까지 30Hz timer와 `OfflineMatch::Step`을 실행하지 않는다. 전원이 인증되면 즉시 실행을 시작한다. 일부 ticket이 만료되면 남은 slot이 확정되는 순간 실행을 시작하고, Unavailable contender의 lifecycle command를 첫 tick 전에 제출한다.

Authenticated slot이 하나 이상일 때만 simulation tick을 시작한다. 전원이 Unavailable이면 `Step`을 실행하지 않고 `NoAuthenticatedPlayers`로 `MatchFinished`를 lobby에 보낸다. 이 gate 때문에 ticket을 받지 못한 participant가 경기 시작 전에 중립 AI에게 공격받거나 match가 영구 대기하는 경로가 없다.

## simulation 연결

game server는 실제 runtime NavMesh를 한 번 load하고 다음 설정으로 match를 만든다.

```cpp
MatchConfig config = DefaultMatchConfig();
config.contenderCount = reservedParticipants.size();
config.enableInternalBots = false;
config.seed = reservation.seed;
```

중립 AI는 기본값대로 근접형 50개와 원거리형 50개를 유지한다. player participant만 외부 input으로 움직인다.

연결 종료를 deterministic simulation command로 처리하기 위해 다음 system command를 추가한다.

```cpp
enum class ContenderExitReason
{
    Disconnected
};

struct MatchLifecycleCommand
{
    ActorId actor;
    ContenderExitReason reason;
};
```

`OfflineMatch::Submit(MatchLifecycleCommand)`는 다음 `Step`에서 살아 있는 contender를 탈락시키고 `ActorDisconnected` event를 남긴다. 이미 죽었거나 존재하지 않는 actor에 대한 중복 command는 상태를 두 번 바꾸지 않는다. 마지막 생존자 판정은 기존 match resolution을 그대로 사용한다.

## 30Hz fixed tick runner

game server는 하나의 Boost.Asio `io_context` thread에서 control TCP, game TCP, UDP와 simulation timer를 처리한다. v1에서는 match state에 mutex를 추가하지 않는다.

tick period는 `1 / 30초`이며 다음 deadline은 실제 작업 종료 시각이 아니라 이전 목표 deadline에 period를 더해 계산한다. timer가 늦으면 한 callback에서 최대 5 tick만 따라잡는다. 그 뒤에도 뒤처졌다면 overrun counter와 지연 시간을 기록하고 다음 deadline을 현재 시각에서 한 period 뒤로 재설정한다. 지나간 tick을 무제한 실행해 event loop를 굶기지 않는다.

각 tick의 순서는 고정한다.

1. session별 도착 input 중 sequence가 증가한 최신 상태 반영
2. disconnect lifecycle command 반영
3. `OfflineMatch::Step`
4. match event drain과 reliable result 판정
5. 짝수 server tick이면 recipient별 snapshot 생성과 UDP 전송

`OfflineMatch`에는 항상 고정 delta가 적용된다. wall-clock frame delta를 simulation에 전달하지 않는다. runner는 clock과 timer scheduling을 주입할 수 있어 unit test가 sleep 없이 tick, catch-up과 overrun을 검증하게 한다.

## UDP datagram frame

UDP는 TCP frame을 재사용하지 않는다. 모든 datagram은 다음 10바이트 header로 시작한다.

| offset | 크기 | 필드 |
| ---: | ---: | --- |
| 0 | 4 | magic `DXU1`, wire bytes `44 58 55 31` |
| 4 | 2 | protocol version, little-endian |
| 6 | 1 | datagram type |
| 7 | 1 | reserved, 반드시 0 |
| 8 | 2 | payload byte count, little-endian |

수신 길이는 header의 payload byte count와 정확히 같아야 하며 전체가 1,200바이트를 넘으면 안 된다. 잘못된 magic, version, reserved byte, type, 길이와 payload는 상태를 바꾸지 않고 버린다. UDP 오류에는 응답하지 않아 amplification 경로를 만들지 않는다.

datagram type은 다음 값으로 고정한다.

| 값 | 방향 | datagram |
| ---: | --- | --- |
| 1 | client to server | `UdpBind` |
| 2 | server to client | `UdpBindAccepted` |
| 3 | client to server | `ClientInput` |
| 4 | server to client | `SnapshotFragment` |

모든 client to server payload는 MatchId, PlayerId와 16바이트 UDP token으로 시작한다. server는 configured game UDP endpoint에서 온 응답만 client가 받아들이게 한다.

### UDP bind

TCP 인증 후 client는 같은 MatchId, PlayerId와 UDP token을 담은 `UdpBind`를 보낸다. 첫 valid bind가 해당 session의 source IP와 port를 고정한다. 같은 endpoint의 duplicate bind에는 같은 `UdpBindAccepted`를 다시 보낼 수 있다. 다른 endpoint에서 온 valid token은 거부한다. v1은 endpoint rebind를 지원하지 않는다.

`UdpBindAccepted`는 MatchId, PlayerId와 현재 server tick을 담는다. bind 성공 전 input은 버리고 snapshot도 보내지 않는다.

### client input

`ClientInput`은 다음 값을 가진다.

- MatchId, PlayerId, UDP token
- `inputSequence`, 1부터 시작하는 `std::uint32_t`
- move destination 존재 bit와 존재할 때 float x, z
- attack target 존재 bit와 존재할 때 ActorId

move destination과 attack target은 button edge가 아니라 현재 원하는 지속 상태다. server는 한 simulation tick 동안 도착한 valid input 중 sequence가 가장 큰 상태를 한 번만 `MatchCommand`로 제출한다. attack cooldown과 거리 같은 실제 공격 가능 여부는 `OfflineMatch`가 판정한다.

float는 IEEE 754 binary32 bit를 little-endian `std::uint32_t`로 직렬화한다. NaN과 infinity는 decode 후 semantic validation에서 거부한다. input sequence는 match 동안 wrap하지 않는다. `UINT32_MAX` 다음 input이 필요해지면 client는 session을 protocol error로 종료한다.

server는 session별 `lastProcessedInputSequence`를 가진다.

- sequence가 이전 값 이하이면 duplicate 또는 오래된 input으로 버림
- 새 sequence는 semantic 성공 여부와 무관하게 last processed를 전진
- endpoint, token, MatchId와 PlayerId가 다르면 sequence도 전진시키지 않음
- move destination은 finite이고 NavMesh 안에 있어야 함
- path가 없는 destination은 거부
- attack target은 존재하고 현재 공격 가능한 actor ID 범위여야 함

semantic 거부도 sequence를 전진시키므로 같은 잘못된 input을 반복 재생할 수 없다. server는 유효한 부분만 `MatchCommand`로 제출하고 recipient snapshot에 last processed sequence를 ACK한다.

## snapshot과 fragment 재조립

server는 매 두 simulation tick마다 snapshot을 만든다. Week 9는 모든 actor, loot, zone, phase와 result를 포함하는 full-state baseline만 사용한다. float 양자화, 관심 영역과 delta는 적용하지 않는다.

recipient마다 ACK가 다르므로 `SnapshotFragment` header는 다음 값을 가진다.

- MatchId
- `snapshotId`, 1부터 증가하며 match 안에서 wrap하지 않음
- `serverTick`
- recipient의 `ackInputSequence`
- `fragmentIndex`
- `fragmentCount`
- `fullPayloadBytes`
- full snapshot payload의 CRC32
- fragment bytes

UDP 공통 header와 snapshot fragment metadata는 42바이트다. 한 fragment의 data는 최대 1,158바이트다. fragmentCount는 1부터 32, fullPayloadBytes는 최대 37,056바이트다. payload가 이 한계를 넘으면 일부만 보내지 않고 match server 오류로 기록한다.

CRC32는 직렬화된 full snapshot payload 전체에 계산하고 모든 fragment에 같은 값을 넣는다. client는 완전히 재조립한 뒤 길이와 CRC32가 모두 맞을 때만 decode한다. CRC32는 인증 수단이 아니며 전송 또는 조립 오류 탐지에만 사용한다.

client reassembler는 match마다 조립 중인 최신 snapshot 하나만 유지한다.

- 더 새로운 snapshotId를 받으면 불완전한 이전 snapshot 폐기
- 더 오래된 snapshotId는 폐기
- 같은 snapshot의 duplicate fragment는 한 번만 저장
- 같은 snapshotId인데 count, byte length, CRC와 server tick이 다르면 해당 snapshot 전체 폐기
- 모든 fragment가 모였을 때 index 순서로 결합
- 완성돼 전달한 snapshotId의 추가 fragment는 폐기

snapshot payload decode는 actor와 loot count를 먼저 검증하고 bounded allocation만 한다. vector count, enum, bool, finite float, duplicate ActorId와 LootId, trailing bytes가 잘못되면 world state를 바꾸지 않는다.

## client prediction과 reconciliation

`dxa_game_client`의 local predictor는 server와 같은 NavMesh, contender speed, stopping distance와 30Hz fixed delta를 사용한다. client가 right-click destination을 받으면 현재 destination으로 저장한다. 매 client tick마다 새 input sequence와 현재 destination을 전송하고 같은 input을 history에 보관한 뒤 local `NavAgent`를 한 tick 진행한다.

권위 snapshot을 받으면 다음 순서로 보정한다.

1. snapshot의 local ActorId 위치를 authoritative position으로 설정
2. history에서 sequence가 ACK 이하인 input 제거
3. 남은 input을 sequence 순서로 같은 fixed delta만큼 replay
4. replay 결과를 다음 render frame의 local 위치로 사용

ACK가 client가 발급한 최신 sequence보다 크거나 snapshot의 local actor가 없으면 protocol error로 session을 종료한다. history가 256개를 넘도록 ACK가 오지 않으면 오래된 input을 조용히 버리지 않고 synchronization failure를 알리고 연결을 종료한다.

이번 주차는 correction을 즉시 반영한다. 화면상 오차를 여러 frame에 나눠 숨기는 visual smoothing은 실제 correction 크기를 측정한 뒤 결정한다.

## remote interpolation

remote actor는 snapshot에 포함된 server tick을 기준으로 별도 buffer에 저장한다. 최대 32개 complete snapshot만 보관한다. render target tick은 최신 수신 tick에서 3 tick, 약 100ms를 뺀 값이다.

target 앞뒤의 두 snapshot이 있으면 position은 선형 보간한다. health, alive, weapon과 phase 같은 discrete 값은 더 새로운 snapshot 값을 사용한다. target보다 새 snapshot이 없으면 마지막 값을 유지하며 extrapolation하지 않는다. 늦게 도착한 이미 소비된 tick의 snapshot은 버린다.

local actor는 interpolation buffer에서 제외하고 prediction 결과를 사용한다. 중립 AI와 다른 contender는 같은 interpolation 경로를 사용한다.

## DX11 client 결합

엔진이 network type을 알지 않도록 app과 engine 사이에 작은 runtime scene 경계를 추가한다.

```cpp
struct RuntimeInputFrame
{
    std::optional<SceneVector3> moveDestination;
};

struct RuntimeSceneFrame
{
    SceneVector3 controlledPlayer;
    std::vector<SceneCharacterState> players;
    std::vector<SceneCharacterState> ai;
    float zoneRadius;
};

class IRuntimeSceneController
{
public:
    virtual ~IRuntimeSceneController() = default;
    virtual void FixedUpdate(const RuntimeInputFrame& input) = 0;
    [[nodiscard]] virtual RuntimeSceneFrame SampleScene() = 0;
};
```

`EngineApp`는 controller가 없으면 기존 benchmark와 render smoke 동작을 그대로 유지한다. controller가 있으면 render thread에서 30Hz accumulator를 실행하고 scene frame을 hybrid deferred renderer에 적용한다. 한 render frame의 fixed update도 최대 5회로 제한한다.

오른쪽 mouse press는 현재 camera의 screen ray와 y=0 ground plane 교점을 구한다. ray가 평면과 평행하거나 camera 뒤에서 만나거나 결과가 finite가 아니면 destination을 만들지 않는다. client NavMesh 검사와 무관하게 server가 같은 destination을 다시 검증한다.

network I/O는 별도 `io_context` thread에서 실행한다. render thread는 command를 io_context에 post하고, network thread는 complete snapshot을 capacity 64의 bounded queue로 전달한다. mutable simulation state를 두 thread가 함께 소유하지 않는다. queue가 가득 차면 가장 오래된 아직 소비되지 않은 snapshot을 버리고 최신 snapshot을 보존하며 drop count를 기록한다.

### 자동 방 생성 mode

DX11 client에 다음 선택적 option을 추가한다.

```text
--network-create
--expected-players <2..24>
--lobby-host <host>
--lobby-port <port>
```

`--network-create`는 benchmark mode와 함께 사용할 수 없다. client는 welcome 후 room을 만들고 한 번 ready를 켠다. 자신이 host이고 member 수가 expected players와 같으며 전원이 ready인 snapshot을 받으면 start를 한 번만 요청한다. ticket을 받으면 game TCP 인증과 UDP bind를 이어간다.

기존 `dxa_bot_client`는 lobby 전용 mode를 유지한다. `--play`를 지정한 한 개 bot은 ticket 수신 후 같은 `dxa_game_client`로 game TCP와 UDP에 연결하고 deterministic destination input을 보낸다. Week 9의 play mode는 `--count 1`만 허용한다. 23개 동시 game bot은 10주차에서 별도 부하 gate와 함께 연다.

실행 형태는 다음과 같다.

```powershell
dxa_lobby_server `
  --bind 127.0.0.1 `
  --port 7000 `
  --worker-bind 127.0.0.1 `
  --worker-port 7001

dxa_game_server `
  --lobby-control-host 127.0.0.1 `
  --lobby-control-port 7001 `
  --worker-id 1 `
  --advertise-host 127.0.0.1 `
  --game-tcp-port 7100 `
  --game-udp-port 7101

dxa_client `
  --render-path hybrid-deferred `
  --network-create `
  --expected-players 2 `
  --lobby-host 127.0.0.1 `
  --lobby-port 7000

dxa_bot_client `
  --host 127.0.0.1 `
  --port 7000 `
  --room 1 `
  --count 1 `
  --play
```

fresh lobby process에서 생성된 room이 1이 아닐 경우 마지막 명령의 가정을 사용하지 않고 실제 출력 ID를 넣는다. ticket과 UDP token은 어느 executable도 출력하지 않는다.

## lobby와 game disconnect 규칙

- Waiting에서 lobby TCP disconnect는 기존 leave와 같음
- Starting에서 lobby TCP disconnect는 reservation rollback 후 leave 적용
- InMatch에서 lobby TCP disconnect는 lobby routing만 제거하며 contender를 탈락시키지 않음
- game TCP disconnect는 다음 server tick에 해당 contender 탈락
- game UDP inactivity만으로는 탈락시키지 않음
- worker control disconnect는 match 전체 중단
- 재접속과 endpoint rebind는 지원하지 않음

두 client가 game 인증한 뒤 headless client의 game TCP를 정상 종료하는 integration scenario에서 server는 해당 actor를 탈락시키고 DX11 client를 최후 생존자로 판정해야 한다. client가 아직 game TCP에 인증하지 않은 채 ticket이 만료되면 해당 contender는 현재 v1에서 접속 실패 상태로 남는다. game server는 ticket expiry 시 그 contender를 탈락 처리해 match가 무기한 멈추지 않게 한다.

## 오류, 제한과 log

TCP malformed frame은 기존 64KiB 경계를 적용한다. frame 구조를 신뢰할 수 있으면 제한된 error enum을 보내고, header 자체가 잘못됐으면 연결을 닫는다. UDP malformed datagram은 응답 없이 버린다.

log에 허용하는 값은 WorkerId, ReservationId, MatchId, PlayerId, ActorId, endpoint, tick, snapshot byte 수, fragment 수, 공개 error code와 누적 counter다. 다음 값은 기록하지 않는다.

- match ticket bytes
- UDP session token
- packet 원문
- 내부 exception이 포함한 filesystem path
- 개인정보와 password

기본 bind는 모두 loopback이다. 외부 bind는 CLI에서 명시해야 한다. v1 TCP와 UDP는 암호화되지 않으므로 신뢰할 수 없는 network에 그대로 공개하지 않는다. 11주차 외부 접속 전에 보안 그룹, tunnel 또는 transport 보안 경계를 별도로 결정한다.

## 테스트 전략

### protocol unit test

- WorkerId와 ReservationId strong type 구분
- message type 13부터 24의 고정 값
- worker control과 game TCP 모든 message round trip
- channel에 맞지 않는 message 거부
- participant 2와 24 경계, 25 거부
- host 255바이트 경계와 초과 거부
- UDP exact bytes와 little-endian float bit
- 1,200바이트 경계와 초과 datagram 거부
- snapshot 1과 32 fragment 경계, 33 거부
- CRC 불일치, duplicate ID, non-finite float와 trailing bytes 거부

### lobby service와 worker registry unit test

- 가장 작은 Idle WorkerId 선택
- capacity가 1이 아닌 등록 거부
- Starting snapshot 뒤 ready에서만 InMatch 전환
- worker 없음, reject, timeout과 disconnect rollback
- rollback 후 ready 값 유지와 ticket revoke
- timeout과 늦은 ready 중 먼저 처리된 event만 유효
- Starting participant disconnect 시 cancel, leave와 host 승계
- cancel ACK 전 worker 재사용 금지
- MatchFinished 후 room과 player index 정리
- Active worker disconnect 시 MatchUnavailable 전달

### game server unit test

- participant 정렬과 ActorId 0부터 N-1 대응
- ticket match/player mismatch, expiry, consume와 reuse
- ticket과 UDP token 생성 실패의 안전한 종료
- 전원 인증 전 simulation 미시작, 일부 만료 후 첫 tick 탈락
- 전원 미인증 만료 시 NoAuthenticatedPlayers 종료
- 첫 endpoint bind와 다른 endpoint rebind 거부
- duplicate와 오래된 input 무시
- semantic invalid input도 ACK sequence 전진
- NaN, infinity, NavMesh 밖과 path 없는 destination 거부
- game TCP disconnect와 ticket expiry contender 탈락
- game TCP 전원 disconnect 시 NoConnectedPlayers 종료
- 30Hz tick, 15Hz snapshot과 최대 5 catch-up
- overrun counter와 timer rebase
- match result의 game TCP와 worker control 전달

### client unit test

- ACK 이하 input 제거와 unacked replay
- 의도적으로 다른 authoritative position에서 correction
- ACK가 발급 범위를 넘을 때 protocol error
- input history 256개 경계
- mapId 또는 NavMesh CRC 불일치 시 UDP bind 전 종료
- 새로운 snapshot이 불완전한 이전 조립 폐기
- duplicate fragment와 CRC 실패 무상태 처리
- server tick 3개 지연 보간
- newest 뒤 extrapolation 없이 hold
- bounded snapshot queue가 최신 상태 보존
- screen ray의 valid ground hit와 parallel, behind, non-finite 거부

### loopback integration test

- 실제 lobby public TCP와 worker control TCP 연결
- 실제 game TCP와 UDP ephemeral port 사용
- room create, join, ready, asynchronous start와 ticket 수신
- 두 client ticket 인증과 서로 다른 UDP token
- UDP bind 뒤 input ACK와 fragment snapshot 재조립
- 한 client game TCP close 뒤 탈락과 최후 생존 result
- invalid, expired, reused ticket이 동일 공개 인증 오류 반환
- incomplete older snapshot 뒤 newer complete snapshot 적용
- worker reservation timeout 뒤 room Waiting 복귀

socket integration은 Windows와 Ubuntu CI에서 headless client 두 개로 실행한다. Windows에는 실제 `dxa_client`를 WARP hidden mode로 띄워 network scene adapter와 game 인증을 통과하는 별도 smoke를 둔다. 실제 RTX 3050 Ti 화면 검수는 local 수동 gate로 남기고 CI 결과처럼 표현하지 않는다.

integration fixture는 production protocol과 runner를 그대로 쓰되 종료 시간을 줄이기 위한 `MatchConfig`를 생성자에서 주입한다. production executable 기본값인 중립 AI 100개와 8분에서 10분 match 설정을 바꾸지 않는다. 짧은 fixture 결과를 production 성능이나 완주 시간으로 기록하지 않는다.

## 완료 조건

- Windows MSVC client와 server build 성공
- Ubuntu GCC server와 headless client build 성공
- lobby에 실제 worker가 등록되고 ACK 전에는 ticket이 client에게 전달되지 않음
- 실제 DX11 client 1개와 headless client 1개가 같은 MatchId로 game 인증 성공
- 모든 participant slot이 인증 또는 만료로 확정되기 전에는 simulation이 시작되지 않음
- server가 30Hz fixed tick과 15Hz snapshot contract를 지킴
- right-click destination을 server가 NavMesh로 다시 검증
- local actor prediction, ACK reconciliation과 remote interpolation 검증
- invalid, expired, reused ticket과 잘못된 UDP token 거부
- duplicate 및 오래된 input과 incomplete snapshot 처리 검증
- game TCP disconnect가 탈락과 match result로 연결됨
- ticket과 UDP token이 log, console과 문서에 노출되지 않음
- README, ADR과 개발 기록이 실제 실행 명령과 검증 결과에 맞게 갱신됨

## 10주차로 남기는 범위

- 실제 DX11 client 1개와 game bot 23개의 세 경기 연속 완주
- 관심 영역 spatial grid
- position과 state 양자화
- 변경 bitset과 delta snapshot
- full-state baseline 대비 대역폭과 server 비용 비교
- 100ms RTT, 2퍼센트 손실, 10ms jitter 주입
- client 평균 수신량 64KiB/s 목표 검증
- 30분 soak, sanitizer, leak와 단조 메모리 증가 검사

## 남은 한계

- heartbeat가 없어 half-open control connection 감지는 OS TCP timeout에 의존함
- game TCP와 UDP가 암호화되지 않음
- reconnect, session resume와 UDP endpoint rebind 없음
- worker는 동시 match 하나만 처리함
- client correction visual smoothing 없음
- snapshot이 full state라 24명에서는 대역폭 목표를 넘을 수 있음
- worker process crash 시 active match 복구 없음
