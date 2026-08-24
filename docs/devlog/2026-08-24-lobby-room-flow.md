# 24명이 같은 TCP 로비에서 ticket까지 받게 만들었다

## 상황

7주차까지 한 경기는 process 안에서 끝났다. 참가자 24명과 중립 AI 100마리가 같은 `OfflineMatch`를 사용했지만, 방을 만들거나 다른 process에서 참가하는 경로는 없었다.

8주차 목표는 로비 화면을 그리는 것이 아니었다. 콘솔 client 1개와 bot 23개가 같은 protocol과 transport를 사용해 방 생성, 입장, 준비, 시작, ticket 수신까지 가는 수직 기능을 먼저 만들었다. 실제 game server, UDP와 network gameplay는 범위에서 뺐다.

## wire를 먼저 고정했다

첫 테스트는 protocol header가 없어서 compile되지 않는 RED였다. PlayerId, RoomId, MatchId와 EntityId를 다른 type으로 만들고 message type 12개와 public error 19개를 고정했다.

TCP frame은 `DXA1`, version, message type, payload length를 합친 12바이트 header다. 전체 frame은 65,536바이트, payload는 65,524바이트를 넘을 수 없다. payload는 little-endian writer와 bounded reader로 읽는다. trailing byte, 잘못된 bool과 enum, count 초과를 decode 단계에서 거부한다.

메모리 struct를 그대로 보내는 대안은 사용하지 않았다. compiler padding과 endian이 wire 계약이 되는 것을 피하려고 각 field를 순서대로 썼다.

## room 규칙을 socket 밖에 뒀다

`Room`은 Waiting, Starting, InMatch만 안다. 정원은 24명이고 host를 포함한 모든 참가자가 ready여야 한다. host가 Waiting에서 나가면 내부 입장 순번이 가장 작은 참가자가 host가 된다.

구현 검토 중 `BeginStarting()`을 직접 호출하면 ready 검증을 건너뛸 수 있는 경로를 찾았다. unready room에서 예외가 나는 테스트를 먼저 추가하고 `ValidateStart(host)`를 transition 안에서도 확인했다.

명시적 leave 응답에도 빈칸이 있었다. 참가자를 room에서 제거한 뒤에는 요청자에게 `RoomSnapshot`을 보낼 수 없다. 새 message를 추가하지 않고 요청자에게 최신 `RoomListResponse`를 보내고 남은 참가자에게 request ID 0 snapshot을 보내는 계약으로 정했다.

`LobbyService`는 connection, player, room index를 한 곳에서 관리한다. client request ID는 항상 증가해야 한다. `NotWelcomed`, `RoomNotFound` 같은 semantic error가 나도 그 ID를 다시 실행할 수 없도록 마지막 ID를 전진시킨다.

## 시작 실패를 되돌렸다

start는 host, 최소 2명, 전원 ready를 먼저 확인한다. 통과하면 MatchId를 예약하고 room을 Starting으로 바꾼다. 각 participant ticket을 임시 vector에 발급한 다음 worker allocator를 호출한다.

두 번째 ticket source가 실패하는 fixture에서 첫 ticket이 registry에 남을 수 있었다. 실패 경로에서 지금까지 발급한 값을 모두 revoke하고 Waiting으로 돌아가도록 했다. unavailable worker와 잘못된 static endpoint도 같은 rollback을 사용한다. ready 값은 지우지 않는다. MatchId는 재사용하지 않는다.

성공 시 room을 InMatch로 바꾸고 participant마다 다른 ticket을 보낸다. host의 ticket은 start request ID를, 나머지는 server push ID 0을 사용한다.

## async write queue에서 확인한 race

`AsioFramedConnection`은 header를 먼저 읽고 검증된 길이만큼만 payload를 할당한다. write는 deque 하나에서 순서를 지키며 pending frame 합이 256KiB를 넘으면 slow client로 보고 닫는다.

첫 pending 제한 테스트는 MSVC debug deque assertion으로 멈췄다. socket close 전에 async write completion이 이미 성공 상태로 queue에 들어온 경우가 있었다. close가 deque를 먼저 비운 뒤 completion handler가 `front()`를 읽었다.

진행 중인 buffer는 completion까지 deque에 남기고 handler가 closed 상태를 먼저 확인하게 바꿨다. close callback이 마지막 외부 `shared_ptr`을 해제하는 경우도 재현해 `Send`와 `Close` 범위에서 자신을 보존했다.

최종 검토에서는 close callback의 실행 시점도 다시 봤다. server가 outbound snapshot을 순서대로 보내는 중 slow client가 닫히면 callback이 같은 call stack에서 `Disconnect`를 실행할 수 있었다. 그러면 새 disconnect snapshot을 먼저 보낸 뒤 바깥 route가 이전 snapshot을 다시 보낼 수 있다. callback이 현재 call 안에서 실행되는 RED를 추가하고 socket close 알림을 같은 executor의 다음 turn으로 미뤘다. frame과 실제 TCP integration 13건으로 전송 순서를 다시 확인했다.

Boost.Asio 1.92에서는 deprecated `timer.cancel(error_code)` overload도 사용할 수 없었다. 현재 `cancel()` API로 test deadline을 바꾸고 Windows target을 `_WIN32_WINNT=0x0A00`으로 명시했다.

## 실제 TCP에서 정원과 disconnect를 확인했다

domain test만 통과한 상태를 완료로 보지 않았다. ephemeral port server와 별도 client `io_context`를 둔 fixture에서 실제 loopback socket을 열었다.

25개 client를 welcome한 뒤 첫 client가 방을 만들었다. 24번째까지 입장했고 25번째는 `RoomFull`을 받았다. 이 테스트가 상수를 실제로 보는지 확인하려고 `RoomCapacity`를 잠시 23으로 바꿨다. 24번째를 기다리는 지점에서 5초 timeout RED가 났다. 상수를 즉시 24로 복구한 뒤 같은 test가 통과했고 임시 변경은 diff에 남기지 않았다.

host socket을 닫으면 PlayerId 2가 host인 snapshot이 남은 client에 도착했다. 65,524바이트보다 큰 payload 길이를 header에 넣은 raw connection은 room을 만들지 못하고 닫혔다.

## 콘솔 client와 23봇

`LobbyClient`는 resolver, framed connection과 request counter를 소유한다. CLI와 bot은 raw socket이나 protocol encoder를 사용하지 않고 `Hello`, `JoinRoom`, `SetReady`, `StartMatch` 같은 typed method만 호출한다.

bot은 welcome을 받으면 지정 room에 입장하고 자기 PlayerId가 snapshot에 나타나면 ready를 보낸다. ticket을 받은 bot 수가 요청 count와 같아야 exit code 0이다. option error는 2, protocol error는 3, 30초 timeout은 4다. count는 1부터 23까지만 받는다.

첫 수동 실행에서는 24명과 bot ready를 확인했지만 host가 start를 보내기 전에 30초가 지나 bot이 종료됐다. 기능 실패로 쓰지 않고 timeout 동작을 확인한 실행으로 남겼다.

다음 자동 실행은 CLI stdout이 redirected pipe에서 flush되지 않아 welcome을 5초 안에 관찰하지 못했다. 터미널에서는 줄이 보였지만 pipe에서는 buffer에 남았다. line writer의 `sync()` 호출을 검사하는 RED를 추가하고 모든 CLI 출력이 매 줄 flush되도록 고쳤다.

## 최종 실행 결과

새 server process를 띄우고 CLI가 만든 RoomId가 1인지 확인한 뒤 계속했다. host를 ready로 바꾸고 bot 23개가 모두 ready인 snapshot이 도착하자 start를 보냈다.

```text
room_id=1
participant_count=24
state=InMatch
cli_ticket_message_count=1
bot_ticket_count=23
bot_exit_code=0
cli_exit_code=0
elapsed_ms=359
ticket_bytes_recorded=false
```

359ms는 server가 이미 listen 중인 로컬 PC에서 CLI process 시작부터 종료까지 잰 wall time이다. internet RTT, game server 시작과 simulation 비용은 포함하지 않는다.

## 검증

다음 명령을 문서 작성 전 HEAD에서 실행했다.

```powershell
./scripts/build.ps1
./scripts/test.ps1
./scripts/build.ps1 -Preset windows-msvc-release
```

Windows Debug CTest 308건이 모두 통과했다. 여기에는 Client WARP, NavigationDemo WARP, OfflineMatchDemo WARP, protocol과 lobby unit test, 24인 TCP integration과 benchmark guard가 포함된다. MSVC Release build도 통과했다.

ticket formatter test는 16바이트를 `0xAB`로 채운 뒤 출력에 `AB`와 `ab`가 없는지 확인한다. server audit type에는 ticket field가 없다.

## 남은 한계

static worker endpoint는 실제 process와 health를 확인하지 않는다. 이번 ticket은 live game server에서 consume하지 않았고 registry unit test만 통과했다.

InMatch disconnect를 탈락으로 바꾸는 game server가 없다. reconnect도 없다. lobby process는 match 종료 통지를 받지 않아 InMatch room을 정리하지 못한다.

UDP 입력, 30Hz 권위 simulation, 15Hz snapshot, prediction과 reconciliation은 9주차에 연결한다.

Windows Debug와 Release는 로컬에서 검증했다. Linux compiler는 이번 로컬 환경에 없으므로 Linux build 완료로 표현하지 않는다. Ubuntu CI 결과를 별도로 확인한다.

기본 server는 loopback에 bind한다. 외부 bind를 명시한 경우에도 connection 총량 제한과 반복 accept failure backoff는 아직 없다. 외부 배포 전에 운영 제한을 추가해야 한다.
