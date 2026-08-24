# ADR 0006: 로비 domain과 TCP adapter를 분리함

- 상태: 채택
- 날짜: 2026-08-24

## 상황

8주차에는 방 생성, 입장, 준비, 시작과 participant ticket 발급을 실제 TCP로 연결해야 했다. 이 규칙을 socket callback 안에 직접 넣으면 room 전이와 network lifetime을 한 테스트에서만 확인하게 된다. Linux server와 Windows client가 같은 packet을 사용해야 하므로 C++ struct의 메모리를 그대로 보내는 방법도 사용할 수 없었다.

한 connection은 hello 전에도 routing 대상이어야 한다. PlayerId는 hello가 성공해야 생기므로 socket을 PlayerId만으로 관리하면 handshake 실패와 disconnect를 같은 방식으로 정리할 수 없다. slow client의 write queue, 잘린 frame, 잘못된 version처럼 room 규칙에 도달하기 전에 끝나는 경우도 있다.

## 결정

로비를 세 경계로 나눈다.

1. `dxa_protocol`은 strong ID, 12바이트 TCP header, little-endian payload codec과 `AsioFramedConnection`을 소유한다.
2. `dxa_lobby_core`는 `Room`, `LobbyService`, ticket registry와 worker allocator interface를 소유한다. socket, thread, spdlog를 참조하지 않는다.
3. `dxa_lobby_server_lib`는 acceptor와 session map을 소유하고 frame을 client message로 바꾼 뒤 `LobbyService` 결과를 connection별로 보낸다.

TCP accept 시 process-local `ConnectionId`를 먼저 발급한다. hello가 성공하면 PlayerId와 연결한다. protocol ID와 process ID를 같은 type으로 쓰지 않는다.

client request ID는 connection마다 1 이상에서 시작해 이전 값보다 커야 한다. semantic error도 마지막 request ID를 전진시킨다. TCP가 순서를 보장하므로 같은 request를 다시 실행하는 cache는 두지 않는다. server push는 request ID 0을 사용한다.

room mutation은 server의 단일 `io_context` thread에서 직렬 실행한다. v1의 `LobbyService`에는 mutex를 넣지 않는다. worker 시작은 조건 검증, MatchId 예약, Starting 전환, participant ticket 임시 발급, worker 배정, InMatch 전환 순서로 실행한다. ticket 또는 worker 배정이 실패하면 임시 ticket을 revoke하고 Waiting으로 돌아가며 ready 값은 유지한다.

match ticket은 Windows BCrypt 또는 Linux `getrandom`으로 만든 128비트 값이다. 60초 만료, participant와 MatchId 일치, 한 번의 consume을 registry에서 확인한다. audit event와 CLI formatter에는 ticket field를 두지 않는다.

`StaticGameWorkerAllocator`는 명시한 host와 TCP, UDP port를 돌려준다. 실제 worker health나 capacity를 확인하지 않는다. 이번 결정에서 static endpoint는 protocol 수직 기능용이며 game server 실행 증거가 아니다.

## 비교한 대안

session callback이 room map을 직접 바꾸는 방식은 파일 수가 적다. 대신 disconnect와 host 승계가 socket lifetime에 묶이고, room 전이 unit test를 만들기 어렵다. pure service가 대상별 outbound message를 반환하도록 했다.

PlayerId를 session key로 쓰는 방식은 hello 전 connection을 표현하지 못한다. 별도 ConnectionId를 사용해 open, welcome, close 순서를 분리했다.

protocol struct를 `reinterpret_cast`로 보내는 방식은 padding, endian, compiler ABI에 따라 wire가 달라진다. 모든 integer를 명시적 little-endian으로 쓰고 count와 enum, trailing byte를 decode 시점에 검사한다.

connection마다 thread와 mutex를 두는 방식도 제외했다. 24인 첫 버전에서는 하나의 `io_context`가 command 순서를 그대로 보장한다. 여러 worker thread로 확장할 때는 strand 또는 room ownership을 새 ADR로 정한다.

## 결과

실제 loopback test는 두 client가 create, join, ready, start를 거쳐 서로 다른 ticket을 받는 흐름과 worker 실패 rollback을 통과했다. 25개 TCP connection test에서는 24번째 입장이 성공하고 25번째가 `RoomFull`을 받았다. host socket을 닫으면 가장 먼저 입장한 다음 PlayerId로 host가 바뀌었다. 과대 header는 payload를 할당하거나 room을 만들기 전에 connection을 닫았다.

최종 로컬 실행은 새 server process에서 RoomId 1을 만들었다. CLI 1개와 bot 23개가 모두 ready가 된 뒤 InMatch로 전환했고 CLI ticket 알림 1건과 bot ticket 23건을 받았다. bot과 CLI exit code는 모두 0이었다. server listen 뒤 CLI process 시작부터 종료까지 로컬 wall time은 359ms였다. 이 값은 loopback 로비 흐름만 포함하며 game server 비용이나 internet latency가 아니다.

Windows Debug CTest 308건과 MSVC Release 빌드를 통과했다. Ubuntu 24.04 GCC CI에서도 build와 Linux CTest 262건이 통과했다. CTest에는 protocol, room, service와 실제 TCP integration이 포함되며 Windows에서는 WARP smoke도 실행한다.

## 결과에 따른 제약

InMatch 이후 disconnect와 탈락, game server의 live ticket consume은 아직 연결하지 않았다. ticket registry의 consume 규칙만 unit test로 검증했다.

static worker는 health check와 match capacity가 없다. 여러 match에 같은 endpoint를 반환할 수 있다.

room 종료 통지가 없어 InMatch room은 lobby process에 남는다. 9주차 worker lifecycle에서 정리해야 한다.

UDP 입력, 30Hz 권위 simulation, 15Hz snapshot, client prediction과 reconciliation은 이 ADR의 범위가 아니다.

Linux source는 Windows 전용 API와 분리했다. 로컬에는 Linux compiler가 없었지만 Docker Ubuntu AddressSanitizer로 Asio test 7건을 확인했고, 최종 Ubuntu CI에서 전체 Linux build와 test를 통과했다.

외부 bind를 명시하면 connection 총량을 제한하는 별도 gate는 없다. accept가 지속적으로 실패할 때 backoff하는 정책도 아직 없다. 기본 bind는 loopback이며 외부 배포 전 두 제한을 운영 설정과 함께 정해야 한다.
