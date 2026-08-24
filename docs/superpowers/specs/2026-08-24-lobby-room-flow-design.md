# 8주차 로비와 방 흐름 설계

## 목적

실제 TCP 연결을 사용하는 로비 수직 기능을 만든다. 한 명은 콘솔 로비 클라이언트로 방을 만들고, 봇 클라이언트는 같은 직렬화와 TCP 전송 계층으로 입장한다. 참가자는 준비 상태를 바꾸고 방장은 게임 시작을 요청한다. 성공하면 참가자별 일회용 match ticket을 받고, worker 배정이 실패하면 방은 대기 상태로 돌아간다.

이번 주차는 로비 경계를 검증한다. DX11 로비 화면, UDP, 권위형 게임 simulation server, client prediction, 재접속, 계정과 영구 전적은 구현하지 않는다.

## 선택한 접근

### 선택: 방 규칙, wire protocol, Asio adapter 분리

`dxa_protocol`은 ID, message와 little-endian codec만 소유한다. Boost.Asio를 include하지 않는다. 같은 디렉터리의 별도 `dxa_protocol_asio` target이 길이 제한이 있는 TCP frame 읽기와 순서 보장 write queue를 제공한다.

`dxa_lobby_core`는 방과 참가자 상태, 방장 승계, 시작 조건, worker 배정과 ticket 발급을 처리한다. socket이나 thread를 알지 못한다. `dxa_lobby_server`가 TCP session을 `LobbyService` command로 바꾸고 service가 만든 대상별 server message를 session에 전달한다.

`dxa_lobby_client`는 콘솔 클라이언트와 봇 클라이언트가 함께 쓰는 비동기 연결이다. 두 app이 자체 packet이나 socket 코드를 갖지 않는다.

이 구조는 9주차에 static worker allocator를 실제 worker registry로 교체하고 DX11 client가 `dxa_lobby_client`를 재사용할 수 있게 한다.

### 제외: 하나의 LobbyServer에서 모든 규칙 처리

구현 파일 수는 줄지만 socket 없이 방 규칙을 재현하기 어렵고 connection lifetime과 room lifetime이 결합된다. 24명 정원, 방장 승계, worker 실패 같은 규칙을 느린 network fixture로만 검증하게 되므로 제외한다.

### 제외: in-memory 방만 구현

9주차까지 TCP를 미루면 실제 client와 bot이 같은 codec과 transport를 쓴다는 8주차 완료 조건을 확인할 수 없다.

### 제외: 실제 game server 선행 구현

game server 등록, 30Hz simulation과 UDP snapshot은 9주차 범위다. 이번 주차는 allocator interface와 명시적으로 설정한 static endpoint만 만든다. static endpoint는 game server가 실행 중이라는 증거로 표현하지 않는다.

## 디렉터리와 target

```text
protocol/
  include/dxa/protocol/
  src/
  CMakeLists.txt

apps/
  lobby_server/
    include/dxa/lobby/
    src/
    CMakeLists.txt
  lobby_cli/
    src/
    CMakeLists.txt
  bot_client/
    src/
    CMakeLists.txt
```

- `dxa_protocol`: ID, enum, client 및 server message, codec, TCP frame header
- `dxa_protocol_asio`: async framed connection과 write queue
- `dxa_lobby_core`: Room, LobbyService, worker allocator, ticket registry
- `dxa_lobby_server_core`: acceptor와 session adapter
- `dxa_lobby_client`: CLI와 bot이 공유하는 async client
- `dxa_lobby_server`: Windows 및 Linux console executable
- `dxa_lobby_cli`: 사람이 명령을 입력하는 console executable
- `dxa_bot_client`: 한 process에서 1명부터 23명까지 연결 가능한 headless executable

기존 `dxa_client`, `dxa_engine`, `dxa_simulation`은 이 주차에서 protocol이나 lobby를 참조하지 않는다.

## 용어와 ID

ID는 서로 바꿔 쓸 수 없는 얇은 값 type으로 정의한다.

```cpp
struct PlayerId { std::uint32_t value; };
struct RoomId { std::uint32_t value; };
struct MatchId { std::uint64_t value; };
struct EntityId { std::uint32_t value; };
```

0은 invalid sentinel로 사용하지 않는다. 값의 부재는 `std::optional`로 표현한다. server가 발급하는 ID는 process lifetime 동안 증가하고 재사용하지 않는다. 다음 ID가 numeric limit을 넘으면 새 요청을 거부한다.

`ConnectionId`는 lobby server process 안에서만 사용하는 별도 `std::uint64_t` strong type이다. TCP accept 시 발급하고 protocol에는 직렬화하지 않는다. hello 전 session과 이미 PlayerId가 발급된 session을 같은 routing key로 다루기 위해 사용한다.

room participant는 다음 상태만 가진다.

- `PlayerId`
- 준비 여부
- 내부 입장 순번

사용자 이름, 비밀번호, 계정 ID와 개인정보는 저장하지 않는다. room은 이름 없이 `RoomId`로만 구분하며 CLI에는 `Room <id>`로 표시한다.

## TCP frame 계약

frame header는 12바이트다.

| offset | 크기 | 필드 |
| ---: | ---: | --- |
| 0 | 4 | magic `DXA1`, wire bytes `44 58 41 31` |
| 4 | 2 | protocol version |
| 6 | 2 | message type |
| 8 | 4 | payload byte count |

모든 정수는 명시적 little-endian으로 읽고 쓴다. C++ object memory와 padding을 송신하지 않는다.

- protocol version은 `1`
- header를 포함한 전체 frame 최대 크기는 65,536바이트
- payload 최대 크기는 65,524바이트
- payload 크기를 검증하기 전에는 payload buffer를 할당하지 않음
- unknown magic, version, message type과 과대 payload는 room 상태를 변경하지 않음
- 잘린 frame은 연결 종료로 처리

version 불일치와 semantic error는 frame 형식을 신뢰할 수 있을 때 `ErrorResponse`를 보낸다. header 자체가 잘못됐거나 크기가 과대하면 추가 할당 없이 연결을 닫는다.

## request 순서

client request는 `requestId`를 가진다. 0은 server push용으로 예약한다.

- 한 connection의 첫 request ID는 1 이상
- 이후 request ID는 이전 값보다 커야 함
- duplicate 또는 오래된 request ID는 `RequestOutOfOrder`로 거부
- 거부된 semantic request도 마지막으로 본 request ID를 전진시킴
- TCP가 신뢰성 전송을 제공하므로 v1에서는 같은 request ID 재전송 cache를 만들지 않음

direct response는 request ID를 그대로 돌려준다. 다른 참가자에게 보내는 room snapshot과 match ticket은 request ID 0을 사용한다. 방장의 성공 ticket만 `StartMatchRequest`의 request ID를 사용한다.

## message type

| 값 | 방향 | message |
| ---: | --- | --- |
| 1 | client to server | `ClientHello` |
| 2 | server to client | `ServerWelcome` |
| 3 | client to server | `ListRoomsRequest` |
| 4 | server to client | `RoomListResponse` |
| 5 | client to server | `CreateRoomRequest` |
| 6 | client to server | `JoinRoomRequest` |
| 7 | client to server | `LeaveRoomRequest` |
| 8 | client to server | `SetReadyRequest` |
| 9 | client to server | `StartMatchRequest` |
| 10 | server to client | `RoomSnapshot` |
| 11 | server to client | `MatchTicket` |
| 12 | server to client | `ErrorResponse` |

모든 message payload는 먼저 `requestId`를 쓴다.

### handshake

연결 직후에는 `ClientHello`만 허용한다. server는 새 `PlayerId`를 발급하고 `ServerWelcome`으로 응답한다. 같은 connection의 두 번째 hello는 `AlreadyWelcomed`, hello 전 다른 요청은 `NotWelcomed`다.

### room list

`RoomListResponse`는 Waiting room만 포함한다. 각 `RoomSummary`는 `RoomId`, 현재 인원, 정원 24를 가진다. RoomId 오름차순으로 직렬화한다.

### room snapshot

`RoomSnapshot`은 `RoomId`, `RoomState`, host PlayerId, 참가자 목록을 가진다. 참가자 view는 `PlayerId`와 준비 여부만 공개하고 PlayerId 오름차순으로 직렬화한다. 내부 입장 순번은 wire에 노출하지 않는다.

### match ticket

`MatchTicket`은 다음 값을 가진다.

- `MatchId`
- ticket 16바이트
- ASCII host, 최대 255바이트
- game TCP port
- game UDP port
- 만료까지 남은 초 60

port 0과 빈 host는 allocator 결과로 허용하지 않는다.

## 방 model

```text
Waiting -> Starting -> InMatch
             |
             +-> allocation 실패 시 Waiting
```

### 생성과 입장

- room 최대 인원은 24명
- process가 유지하는 room 최대 개수는 1,024개
- room 생성자는 첫 참가자이며 host
- host도 ready 조건에 포함
- Waiting room에만 입장 가능
- 이미 room에 있는 player는 다른 room을 만들거나 입장할 수 없음
- 24번째 입장은 성공하고 25번째 입장은 `RoomFull`
- room이 1,024개면 새 생성은 `RoomLimitReached`

### 퇴장과 연결 종료

- Waiting room의 명시적 leave와 TCP disconnect는 같은 domain operation 사용
- host가 나가면 남은 참가자 중 내부 입장 순번이 가장 작은 player가 host
- 마지막 참가자가 나가면 room 삭제
- InMatch 이후 disconnect와 탈락 판정은 9주차 game server 책임

InMatch room은 새 room list에서 제외한다. v1 lobby process 안에서는 match 종료 통지가 없으므로 InMatch room 정리는 9주차 worker lifecycle과 함께 추가한다. Week 8 반복 test는 새로운 LobbyService fixture를 사용해 process 재시작 없이 영구 누적을 만들지 않는다.

### 준비와 시작

- Waiting 상태에서만 ready 변경 가능
- host만 start 요청 가능
- 최소 2명 필요
- host를 포함한 모든 참가자가 ready여야 함
- start 조건을 통과하면 먼저 Starting으로 전환
- worker allocator가 실패하면 Waiting으로 복귀하고 기존 ready 값 유지
- allocator가 성공하면 참가자별 ticket을 만들고 InMatch로 전환
- 같은 room에 있는 각 참가자는 서로 다른 ticket을 받음

start 처리 중에는 다른 command를 끼워 넣지 않는다. 모든 room mutation은 하나의 `io_context` thread에서 직렬 실행한다.

## LobbyService 출력

`LobbyService`는 protocol request를 받아 대상별 server message 목록을 반환한다.

```cpp
struct OutboundMessage
{
    ConnectionId recipient;
    ServerMessage message;
};
```

`LobbyService::OpenConnection()`은 새 ConnectionId를 만들고, hello 성공 시 ConnectionId와 PlayerId를 연결한다. `Disconnect(ConnectionId)`는 연결된 player가 있으면 room leave와 같은 operation을 실행한다.

성공한 room mutation은 현재 room 참가자의 connection 모두에게 최신 `RoomSnapshot`을 보낸다. 요청자는 원래 request ID를 받고 나머지는 0을 받는다. 실패는 요청 connection에만 `ErrorResponse`를 보내며 room snapshot을 바꾸지 않는다. worker 실패는 Waiting으로 복귀한 snapshot을 참가자 전체에 먼저 보내고, host connection에 `WorkerUnavailable` 오류를 보낸다.

## error code

- `MalformedPayload`
- `UnsupportedVersion`
- `UnknownMessageType`
- `FrameTooLarge`
- `RequestOutOfOrder`
- `NotWelcomed`
- `AlreadyWelcomed`
- `AlreadyInRoom`
- `NotInRoom`
- `RoomNotFound`
- `RoomNotJoinable`
- `RoomFull`
- `RoomLimitReached`
- `NotHost`
- `MinimumPlayersRequired`
- `NotAllReady`
- `WorkerUnavailable`
- `IdSpaceExhausted`
- `InternalError`

오류 응답은 공개 error code와 원래 request ID만 보낸다. 내부 exception 문자열, filesystem path와 ticket 값은 보내거나 log에 남기지 않는다.

## worker allocator

```cpp
class IGameWorkerAllocator
{
public:
    virtual WorkerAllocationResult Allocate(
        MatchId match,
        std::span<const PlayerId> players) = 0;
};
```

`StaticGameWorkerAllocator`는 server CLI에 host, TCP port, UDP port가 모두 주어진 경우에만 성공한다. 같은 endpoint를 여러 logical match에 반환할 수 있다. 이 결과는 transport contract 검증용이며 worker capacity 증거가 아니다.

endpoint가 설정되지 않은 기본 server는 start 요청에 `WorkerUnavailable`을 반환한다. 9주차에는 interface를 유지하고 실제 worker registry와 reservation으로 교체한다.

## ticket registry

ticket은 참가자별 128비트 값이다.

- Windows production source는 `BCryptGenRandom`
- Linux production source는 `getrandom`
- `std::random_device`는 보안 난수 근거로 사용하지 않음
- 발급 시 `MatchId`, `PlayerId`, 만료 시각을 registry에 저장
- 발급 후 60초 만료
- 한 번 consume한 ticket은 즉시 제거
- 잘못된 match 또는 player로 consume하면 실패하며 ticket은 소비하지 않음
- 만료 ticket은 consume할 수 없고 정리됨

unit test는 deterministic ticket source와 explicit `steady_clock::time_point`를 주입한다. production code는 ticket bytes를 console과 spdlog에 출력하지 않는다.

## Boost.Asio 경계

server는 v1에서 하나의 `boost::asio::io_context` thread만 사용한다. room과 player map에 mutex를 추가하지 않는다.

TCP session은 다음 순서로 동작한다.

1. 12바이트 header async read
2. magic, version, type, payload length 검증
3. 검증된 길이만큼 payload async read
4. message decode
5. LobbyService 호출
6. 대상 session의 write queue에 encoded frame 추가

한 session에서는 동시에 하나의 async write만 실행한다. pending encoded bytes가 256KiB를 넘으면 slow client로 보고 연결을 닫는다. server는 기본적으로 `127.0.0.1`에 bind한다. 외부 bind는 CLI에서 명시해야 한다.

`LobbyTcpClient`는 같은 frame reader와 write queue를 사용한다. message callback, disconnect callback과 `Send(ClientMessage)`만 공개한다. CLI 입력 thread와 bot orchestration은 command를 `io_context`에 post하며 socket을 직접 만지지 않는다.

## executable 동작

### lobby server

```powershell
dxa_lobby_server --bind 127.0.0.1 --port 7000

dxa_lobby_server `
  --bind 127.0.0.1 `
  --port 7000 `
  --worker-host 127.0.0.1 `
  --worker-tcp-port 7100 `
  --worker-udp-port 7101
```

port 0은 test fixture에서만 ephemeral port로 허용한다. user-facing executable은 1부터 65,535만 받는다.

### lobby CLI

```text
list
create
join <room-id>
leave
ready <on|off>
start
quit
```

CLI는 welcome, room list, room snapshot, error code와 ticket 도착 여부를 표시한다. ticket bytes는 표시하지 않는다.

### bot client

bot client는 1개에서 23개 connection을 한 process에서 만든다. 각 bot은 같은 `LobbyTcpClient`를 사용해 hello, join, ready를 실행하고 ticket 도착까지 기다린다. v1 bot은 방을 만들거나 start하지 않는다.

## log 경계

server log는 다음을 포함한다.

- connection 열림과 닫힘
- 발급된 PlayerId
- room 생성과 삭제
- host 승계
- start 성공 또는 공개 error code
- MatchId와 worker endpoint

ticket bytes, 사용자 입력 원문, 개인 식별 정보와 password는 log에 남기지 않는다.

## 테스트 전략

### protocol unit test

- ID와 enum의 little-endian byte 확인
- 모든 client 및 server message 왕복
- unknown type, version mismatch, 잘린 payload
- 65,536바이트 경계와 초과 frame
- count가 payload 남은 크기를 넘는 vector 거부
- host 255바이트 경계와 초과 거부
- decode 실패 시 입력 cursor가 임의 state를 만들지 않는지 확인

### room 및 service unit test

- 생성자가 host가 됨
- 24번째 입장 성공, 25번째 실패
- ready하지 않은 참가자가 있으면 시작 실패
- host가 아니면 시작 실패
- host 이탈 시 가장 이른 참가자 승계
- 마지막 참가자 이탈 시 room 삭제
- disconnect가 leave와 같은 결과 생성
- duplicate 및 오래된 request ID가 상태를 바꾸지 않음
- worker 실패 후 Waiting과 ready 값 유지
- 성공 시 참가자별 서로 다른 ticket과 InMatch 전환
- ticket 60초 만료, 잘못된 player, 재사용 거부
- ID와 room limit 경계

### loopback TCP integration test

- 실제 ephemeral-port server와 두 `LobbyTcpClient` 연결
- hello, create, join, ready, start, ticket 수신 완주
- 24개 client 입장 후 25번째 client의 `RoomFull` 확인
- host TCP close 뒤 room snapshot에서 host 승계 확인
- 과대 header는 payload allocation 없이 연결 종료
- worker 미설정 server가 Waiting snapshot과 `WorkerUnavailable` 반환
- 같은 test가 Windows와 Ubuntu CI에서 통과

### executable smoke

- server option parser 경계
- lobby CLI command parser 경계
- bot count 1과 23 경계
- server, CLI, bot이 모두 `dxa_protocol_asio`와 `dxa_lobby_client`에 link되는 CMake target 검사

## 완료 조건

- Windows MSVC와 Ubuntu GCC에서 protocol, lobby core, server, CLI, bot build 성공
- protocol 및 room unit test 통과
- 실제 loopback TCP로 생성부터 ticket까지 완주
- 24명 정원과 25번째 거부 검증
- 준비하지 않은 시작, 방장 이탈, worker 실패, 잘못된 ticket, 과대 frame 검증
- 실제 client와 bot이 별도 codec이나 socket 경로를 갖지 않음
- README에 로컬 실행 순서와 아직 game server가 없다는 한계 기록
- 개발 기록에 실제로 발생한 실패, 원인, 수정과 남은 한계 기록

## 남은 한계

- static worker allocator는 실제 worker health와 capacity를 확인하지 않음
- InMatch room 종료와 정리는 9주차 worker lifecycle 전까지 없음
- game server가 없으므로 ticket consume은 unit test에서만 검증
- reconnect와 session resume 없음
- TLS, account authentication과 password room 없음
- DX11 lobby UI 없음
- UDP input과 snapshot 없음
