# 9주차 권위형 게임 서버 구현 계획

> 구현 시 필수 skill: `superpowers:executing-plans`로 task를 순서대로 실행하고 각 checkpoint에서 검수한다. 각 step은 checkbox로 추적한다.

Goal: 실제 DX11 client 1개와 headless client 1개가 lobby room에서 시작해 별도 game server worker에 인증하고, 30Hz 권위 simulation과 15Hz UDP snapshot을 거쳐 한 match 결과까지 받는 수직 기능을 만든다.

Architecture: lobby는 room과 비동기 worker reservation을 소유하고, game server process 하나는 동시에 한 match만 실행한다. game TCP는 일회용 ticket 인증과 결과, game UDP는 endpoint bind, 지속 input과 fragment snapshot을 담당한다. client는 local actor를 예측하고 ACK 기준으로 재실행하며 remote actor는 server tick 3개 뒤에서 보간한다.

Tech Stack: C++20, CMake 3.25 이상, vcpkg manifest, Boost.Asio, GoogleTest, spdlog, Win32, DirectX 11

Spec: `docs/superpowers/specs/2026-08-24-authoritative-game-server-design.md`

## Global Constraints

- 현재 branch `feat/authoritative-game-server`에서 작업하고 별도 worktree를 만들지 않는다.
- 외부 agent, subagent와 교차 model 검수를 사용하지 않는다.
- protocol integer와 float bit는 명시적 little-endian으로 직렬화하고 C++ object memory를 송신하지 않는다.
- 기존 TCP frame은 header 포함 최대 65,536바이트다.
- UDP datagram은 header 포함 최대 1,200바이트다.
- worker reservation timeout은 production에서 2초, game TCP hello timeout은 5초다.
- match ticket과 UDP session token은 각각 128비트이며 production에서 `BCryptGenRandom` 또는 `getrandom`으로 생성한다.
- game server는 30Hz fixed tick, 15Hz full-state snapshot, 최대 5 catch-up tick으로 동작한다.
- room capacity는 24명, worker 동시 capacity는 1이다.
- 기본 listener는 loopback이며 lobby public TCP 7000, worker control TCP 7001, game TCP 7100, game UDP 7101을 사용한다.
- ticket bytes, UDP token, packet 원문, password와 개인정보를 log, console, assertion message와 문서에 남기지 않는다.
- reconnect, UDP endpoint rebind, heartbeat, 관심 영역, 양자화, delta snapshot과 network impairment는 구현하지 않는다.
- 기존 Windows WARP smoke와 benchmark 경로는 network mode를 사용하지 않을 때 동작이 바뀌지 않아야 한다.
- 새 dependency를 vcpkg에 추가하지 않는다.
- 한 commit에는 하나의 논리 변경만 담고 제목은 한국어 명사형 Conventional Commit을 사용한다.
- 모든 commit body에는 `이유`, `핵심 변경`, `검증`을 기록한다.
- 구현 완료, test 통과와 benchmark 수치를 실제 실행 전에는 문서에서 완료 사실로 표현하지 않는다.

## File Structure

### Protocol

- `protocol/include/dxa/protocol/Ids.hpp`: WorkerId와 ReservationId strong ID
- `protocol/include/dxa/protocol/GameTypes.hpp`: endpoint, token, network enum과 고정 상수
- `protocol/include/dxa/protocol/MessageCodec.hpp`: 모든 TCP codec이 공유하는 decode result
- `protocol/include/dxa/protocol/WorkerControlMessages.hpp`: lobby와 worker 사이 typed message
- `protocol/include/dxa/protocol/WorkerControlMessageCodec.hpp`: worker control TCP codec interface
- `protocol/src/WorkerControlMessageCodec.cpp`: worker control bounded codec
- `protocol/include/dxa/protocol/GameTcpMessages.hpp`: game authentication과 reliable result message
- `protocol/include/dxa/protocol/GameTcpMessageCodec.hpp`: game TCP codec interface
- `protocol/src/GameTcpMessageCodec.cpp`: game TCP bounded codec
- `protocol/include/dxa/protocol/Crc32.hpp`: application CRC32 interface
- `protocol/src/Crc32.cpp`: deterministic CRC32 implementation
- `protocol/include/dxa/protocol/GameUdpMessages.hpp`: UDP header, bind, input과 fragment 값 type
- `protocol/include/dxa/protocol/GameUdpCodec.hpp`: UDP encode, decode와 fragmentation interface
- `protocol/src/GameUdpCodec.cpp`: 1,200바이트 UDP codec
- `protocol/include/dxa/protocol/GameSnapshot.hpp`: simulation과 분리된 wire world state
- `protocol/include/dxa/protocol/GameSnapshotCodec.hpp`: full-state snapshot codec interface
- `protocol/src/GameSnapshotCodec.cpp`: bounded snapshot codec

### Lobby

- `apps/lobby_server/include/dxa/lobby/LobbyRuntimeTypes.hpp`: reservation action과 worker event
- `apps/lobby_server/include/dxa/lobby/LobbyService.hpp`: client 및 worker event를 받는 pure room module
- `apps/lobby_server/src/LobbyService.cpp`: Starting transaction, rollback과 match cleanup
- `apps/lobby_server/include/dxa/lobby/WorkerRegistry.hpp`: worker state machine interface
- `apps/lobby_server/src/WorkerRegistry.cpp`: lowest-id assignment과 timeout transition
- `apps/lobby_server/include/dxa/lobby/WorkerControlServer.hpp`: worker control Asio adapter
- `apps/lobby_server/src/WorkerControlServer.cpp`: register, frame routing과 timer
- `apps/lobby_server/include/dxa/lobby/LobbyTcpServer.hpp`: public lobby와 worker adapter 조합
- `apps/lobby_server/src/LobbyTcpServer.cpp`: service result의 public outbound와 runtime action 전달
- `apps/lobby_server/include/dxa/lobby/LobbyServerOptions.hpp`: public 및 worker listener option
- `apps/lobby_server/src/LobbyServerOptions.cpp`: CLI parser
- 삭제 `apps/lobby_server/include/dxa/lobby/GameWorkerAllocator.hpp`
- 삭제 `apps/lobby_server/src/GameWorkerAllocator.cpp`

### Shared arena and simulation

- `simulation/include/dxa/simulation/ArenaMap.hpp`: canonical map 1 source와 NavMesh factory
- `simulation/src/ArenaMap.cpp`: arena vertices, triangles와 grid size
- `simulation/include/dxa/simulation/MatchTypes.hpp`: disconnect lifecycle command와 event
- `simulation/include/dxa/simulation/OfflineMatch.hpp`: lifecycle Submit overload
- `simulation/src/OfflineMatchInternal.hpp`: queued lifecycle state
- `simulation/src/OfflineMatch.cpp`: lifecycle command admission
- `simulation/src/OfflineMatchStep.cpp`: disconnect elimination before player command
- `apps/game_common/include/dxa/game_common/ArenaFingerprint.hpp`: canonical map CRC adapter
- `apps/game_common/src/ArenaFingerprint.cpp`: protocol CRC32를 사용한 map fingerprint
- `apps/game_common/include/dxa/game_common/SnapshotAdapter.hpp`: simulation snapshot to wire snapshot
- `apps/game_common/src/SnapshotAdapter.cpp`: enum과 value 변환

### Game server

- `apps/game_server/include/dxa/game_server/GameServerOptions.hpp`: control, game listener와 worker identity option
- `apps/game_server/include/dxa/game_server/GameTicketStore.hpp`: pre-issued one-use ticket store
- `apps/game_server/include/dxa/game_server/ParticipantRoster.hpp`: PlayerId, ActorId와 slot 상태
- `apps/game_server/include/dxa/game_server/UdpTokenSource.hpp`: deterministic test source와 secure production source seam
- `apps/game_server/include/dxa/game_server/FixedTickScheduler.hpp`: deadline와 catch-up 계산
- `apps/game_server/include/dxa/game_server/AuthoritativeMatch.hpp`: 한 match의 deep event interface
- `apps/game_server/include/dxa/game_server/GameServer.hpp`: control TCP, game TCP와 UDP adapter
- `apps/game_server/src/GameServerOptions.cpp`
- `apps/game_server/src/GameTicketStore.cpp`
- `apps/game_server/src/ParticipantRoster.cpp`
- `apps/game_server/src/FixedTickScheduler.cpp`
- `apps/game_server/src/AuthoritativeMatch.cpp`
- `apps/game_server/src/SecureUdpTokenSource.cpp`
- `apps/game_server/src/GameServer.cpp`
- `apps/game_server/src/main.cpp`

### Game client and DX11 composition

- `apps/game_client/include/dxa/game_client/SnapshotReassembler.hpp`: 최신 snapshot 하나의 bounded 조립
- `apps/game_client/include/dxa/game_client/ClientPredictor.hpp`: local fixed-tick prediction과 replay
- `apps/game_client/include/dxa/game_client/RemoteInterpolator.hpp`: server tick 기반 remote interpolation
- `apps/game_client/include/dxa/game_client/GameSession.hpp`: app과 bot이 공유하는 deep session interface
- `apps/game_client/src/SnapshotReassembler.cpp`
- `apps/game_client/src/ClientPredictor.cpp`
- `apps/game_client/src/RemoteInterpolator.cpp`
- `apps/game_client/src/GameSession.cpp`
- `engine/include/dxa/engine/RuntimeScene.hpp`: engine이 소비하는 fixed-size scene interface
- `engine/include/dxa/engine/GroundPlanePicking.hpp`: screen ray to ground interface
- `engine/src/windows/GroundPlanePicking.cpp`: DirectXMath picking
- `apps/client/include/dxa/client/NetworkClientController.hpp`: lobby host flow와 GameSession 조합
- `apps/client/src/NetworkClientController.cpp`
- `apps/bot_client/include/dxa/bot_client/BotCoordinator.hpp`: lobby-only와 play mode state machine
- `apps/bot_client/src/BotCoordinator.cpp`

### Tests and records

- `tests/protocol_game_types_test.cpp`
- `tests/protocol_worker_control_codec_test.cpp`
- `tests/protocol_game_tcp_codec_test.cpp`
- `tests/protocol_game_udp_codec_test.cpp`
- `tests/protocol_game_snapshot_codec_test.cpp`
- `tests/lobby_worker_registry_test.cpp`
- `tests/lobby_worker_control_integration_test.cpp`
- `tests/simulation_arena_map_test.cpp`
- `tests/game_ticket_store_test.cpp`
- `tests/game_participant_roster_test.cpp`
- `tests/game_fixed_tick_scheduler_test.cpp`
- `tests/game_authoritative_match_test.cpp`
- `tests/game_server_options_test.cpp`
- `tests/game_server_adapter_test.cpp`
- `tests/game_server_integration_test.cpp`
- `tests/game_snapshot_reassembler_test.cpp`
- `tests/game_client_predictor_test.cpp`
- `tests/game_remote_interpolator_test.cpp`
- `tests/game_session_integration_test.cpp`
- `tests/network_vertical_slice_test.cpp`
- `docs/adr/0007-authoritative-game-session.md`
- `docs/devlog/2026-08-24-authoritative-game-server.md`
- `README.md`
- `docs/PROJECT_PLAN.md`

---

### Task 1: Game network ID, constant와 float byte contract

Files:

- Create: `protocol/include/dxa/protocol/GameTypes.hpp`
- Create: `protocol/include/dxa/protocol/MessageCodec.hpp`
- Create: `tests/protocol_game_types_test.cpp`
- Modify: `protocol/include/dxa/protocol/Ids.hpp`
- Modify: `protocol/include/dxa/protocol/LobbyTypes.hpp`
- Modify: `protocol/include/dxa/protocol/LobbyMessages.hpp`
- Modify: `protocol/include/dxa/protocol/ByteCodec.hpp`
- Modify: `protocol/src/ByteCodec.cpp`
- Modify: `protocol/include/dxa/protocol/LobbyMessageCodec.hpp`
- Modify: `protocol/src/LobbyMessageCodec.cpp`
- Modify: `protocol/src/TcpFrame.cpp`
- Modify: `apps/lobby_server/include/dxa/lobby/MatchTicketRegistry.hpp`
- Modify: `apps/lobby_server/src/MatchTicketRegistry.cpp`
- Modify: `apps/lobby_server/src/LobbyService.cpp`
- Modify: `apps/lobby_server/include/dxa/lobby/GameWorkerAllocator.hpp`
- Modify: `tests/protocol_ids_test.cpp`
- Modify: `tests/protocol_byte_codec_test.cpp`
- Modify: `tests/protocol_lobby_message_codec_test.cpp`
- Modify: `tests/protocol_tcp_frame_test.cpp`
- Modify: `tests/lobby_service_test.cpp`
- Modify: `tests/lobby_ticket_test.cpp`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: existing `PlayerId`, `RoomId`, `MatchId`, `EntityId`, `ByteWriter`, `ByteReader`.
- Produces: `WorkerId`, `ReservationId`, `MatchTicketValue`, `UdpSessionToken`, `GameEndpoint`, locked game enum, UDP 상수, generic `MessageDecodeResult`, `WriteF32`, `ReadF32`.

- [ ] Step 1: Write the failing type and byte tests

Add the test source to `dxa_tests` before adding the new header. Lock the values and exact float bytes.

```cpp
TEST(GameTypes, LocksWorkerIdsMessageValuesAndLimits)
{
    static_assert(!std::is_same_v<WorkerId, PlayerId>);
    static_assert(!std::is_same_v<ReservationId, MatchId>);
    EXPECT_EQ(13U, static_cast<unsigned>(MessageType::WorkerRegister));
    EXPECT_EQ(24U, static_cast<unsigned>(MessageType::GameMatchResult));
    EXPECT_EQ(20U, static_cast<unsigned>(LobbyError::MatchUnavailable));
    EXPECT_EQ(1200U, MaxUdpDatagramBytes);
    EXPECT_EQ(10U, UdpHeaderBytes);
    EXPECT_EQ(32U, MaxSnapshotFragments);
    EXPECT_EQ(1158U, MaxSnapshotFragmentPayloadBytes);
    EXPECT_EQ(37056U, MaxSnapshotPayloadBytes);
}

TEST(ByteCodec, WritesAndReadsLittleEndianFloatBits)
{
    ByteWriter writer;
    writer.WriteF32(1.0F);
    const auto bytes = std::move(writer).Finish();
    EXPECT_EQ(
        (std::vector<std::byte>{
            std::byte{0x00}, std::byte{0x00},
            std::byte{0x80}, std::byte{0x3F}}),
        bytes);
    ByteReader reader{bytes};
    const auto decoded = reader.ReadF32();
    ASSERT_TRUE(decoded.has_value());
    EXPECT_FLOAT_EQ(1.0F, *decoded);
    EXPECT_TRUE(reader.Empty());
}
```

Also assert that `WorkerId{0}`, `ReservationId{0}`, invalid enum values and non-finite float policy are rejected by the later message codecs rather than by the raw byte reader.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because `GameTypes.hpp`, WorkerId and float byte methods do not exist.

- [ ] Step 3: Add the exact protocol values

Define the new IDs with defaulted three-way comparison. Move `MatchTicketValue` out of lobby namespace and update existing call sites without changing ticket behavior.

```cpp
struct WorkerId
{
    std::uint32_t value = 0U;
    [[nodiscard]] auto operator<=>(const WorkerId&) const = default;
};

struct ReservationId
{
    std::uint64_t value = 0U;
    [[nodiscard]] auto operator<=>(const ReservationId&) const = default;
};

using MatchTicketValue = std::array<std::byte, MatchTicketBytes>;
using UdpSessionToken = std::array<std::byte, 16U>;

inline constexpr std::uint16_t GameTickRate = 30U;
inline constexpr std::uint16_t SnapshotRate = 15U;
inline constexpr std::size_t UdpHeaderBytes = 10U;
inline constexpr std::size_t MaxUdpDatagramBytes = 1200U;
inline constexpr std::size_t SnapshotFragmentMetadataBytes = 32U;
inline constexpr std::size_t MaxSnapshotFragmentPayloadBytes = 1158U;
inline constexpr std::size_t MaxSnapshotFragments = 32U;
inline constexpr std::size_t MaxSnapshotPayloadBytes = 37056U;
inline constexpr std::size_t MaxClientInputHistory = 256U;
inline constexpr std::size_t MaxClientSnapshotBuffer = 32U;

struct GameEndpoint
{
    std::string host;
    std::uint16_t tcpPort = 0U;
    std::uint16_t udpPort = 0U;
    [[nodiscard]] bool operator==(const GameEndpoint&) const = default;
};
```

Until Task 6 deletes the synchronous allocator, make its `dxa::lobby::GameEndpoint` name an alias of `dxa::protocol::GameEndpoint` so two endpoint structs cannot drift.

Add these locked enums to `GameTypes.hpp`.

```cpp
enum class WorkerReservationReject : std::uint8_t
{
    Busy = 1,
    InvalidReservation = 2,
    SimulationInitializationFailed = 3,
    InternalError = 4
};

enum class GameServerErrorCode : std::uint8_t
{
    AuthenticationFailed = 1,
    ServerNotReady = 2,
    ProtocolViolation = 3,
    InternalError = 4
};

enum class MatchCompletionReason : std::uint8_t
{
    LastSurvivor = 1,
    TimeLimit = 2,
    NoAuthenticatedPlayers = 3,
    NoConnectedPlayers = 4
};

enum class UdpDatagramType : std::uint8_t
{
    Bind = 1,
    BindAccepted = 2,
    ClientInput = 3,
    SnapshotFragment = 4
};
```

Append these exact values to the existing MessageType without changing values 1 through 12.

```cpp
WorkerRegister = 13,
WorkerRegistered = 14,
ReserveMatch = 15,
ReserveMatchReady = 16,
ReserveMatchRejected = 17,
CancelMatchReservation = 18,
MatchReservationCancelled = 19,
MatchFinished = 20,
GameClientHello = 21,
GameServerWelcome = 22,
GameServerError = 23,
GameMatchResult = 24
```

Implement `WriteF32` and `ReadF32` with `std::bit_cast<std::uint32_t>` and existing U32 little-endian methods. Do not reject NaN in `ByteReader`; semantic codecs own finite-value validation.

Move the generic decode result out of LobbyMessageCodec so worker and game codecs do not include lobby-only headers.

```cpp
template <typename MessageVariant>
struct MessageDecodeResult
{
    std::optional<MessageVariant> message;
    DecodeError error = DecodeError::None;
};
```

Extend TcpFrame known-type validation through message value 24 and make LobbyMessageCodec explicitly reject values 13 through 24 on its channel. Extend lobby error validation through MatchUnavailable. A MatchTicket endpoint is valid when `expiresInSeconds` is between 1 and 60 inclusive, because worker ready latency reduces the value sent to clients.

- [ ] Step 4: Run GREEN

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=GameTypes.*:ByteCodec.*:ProtocolIds.*:LobbyMessageCodec.*:TcpFrame.*
```

Expected: all selected tests pass and existing lobby ticket tests still compile against `dxa::protocol::MatchTicketValue`.

- [ ] Step 5: Commit

```powershell
git add -- protocol/include/dxa/protocol/GameTypes.hpp protocol/include/dxa/protocol/MessageCodec.hpp protocol/include/dxa/protocol/Ids.hpp protocol/include/dxa/protocol/LobbyTypes.hpp protocol/include/dxa/protocol/LobbyMessages.hpp protocol/include/dxa/protocol/ByteCodec.hpp protocol/src/ByteCodec.cpp protocol/include/dxa/protocol/LobbyMessageCodec.hpp protocol/src/LobbyMessageCodec.cpp protocol/src/TcpFrame.cpp apps/lobby_server/include/dxa/lobby/MatchTicketRegistry.hpp apps/lobby_server/src/MatchTicketRegistry.cpp apps/lobby_server/src/LobbyService.cpp apps/lobby_server/include/dxa/lobby/GameWorkerAllocator.hpp tests/protocol_game_types_test.cpp tests/protocol_ids_test.cpp tests/protocol_byte_codec_test.cpp tests/protocol_lobby_message_codec_test.cpp tests/protocol_tcp_frame_test.cpp tests/lobby_service_test.cpp tests/lobby_ticket_test.cpp tests/CMakeLists.txt
git commit -m "feat(protocol): game network 기본 계약 추가" -m "이유: worker reservation과 game TCP 및 UDP가 공유할 ID, token과 byte 표현을 먼저 고정해야 했다." -m "핵심 변경: WorkerId, ReservationId, endpoint, network enum, UDP 한계와 little-endian float codec을 추가했다." -m "검증: header 부재 RED 뒤 GameTypes, ByteCodec과 기존 ProtocolIds 테스트를 통과했다."
```

---

### Task 2: Worker control TCP message codec

Files:

- Create: `protocol/include/dxa/protocol/WorkerControlMessages.hpp`
- Create: `protocol/include/dxa/protocol/WorkerControlMessageCodec.hpp`
- Create: `protocol/src/WorkerControlMessageCodec.cpp`
- Create: `tests/protocol_worker_control_codec_test.cpp`
- Modify: `protocol/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Task 1 IDs, token, endpoint, `EncodedMessage` and byte codec.
- Produces: `WorkerToLobbyMessage`, `LobbyToWorkerMessage`, `EncodeWorkerToLobbyMessage`, `DecodeWorkerToLobbyMessage`, `EncodeLobbyToWorkerMessage`, `DecodeLobbyToWorkerMessage`.

- [ ] Step 1: Write failing round-trip, direction and boundary tests

Use deterministic ticket bytes but never stream them into an assertion message.

```cpp
TEST(WorkerControlCodec, RoundTripsRegistrationReservationAndCompletion)
{
    ExpectWorkerRoundTrip(WorkerRegister{
        WorkerId{7U}, "127.0.0.1", 7100U, 7101U, 1U});
    ExpectLobbyRoundTrip(ReserveMatch{
        ReservationId{9U},
        MatchId{11U},
        20260824U,
        60000U,
        {{PlayerId{2U}, Ticket(1U)}, {PlayerId{5U}, Ticket(2U)}}});
    ExpectWorkerRoundTrip(ReserveMatchReady{
        ReservationId{9U}, MatchId{11U}});
    ExpectWorkerRoundTrip(MatchFinished{
        MatchId{11U}, EntityId{0U}, true,
        MatchCompletionReason::LastSurvivor, 320U});
}

TEST(WorkerControlCodec, RejectsWrongDirectionAndParticipantBounds)
{
    const auto registration = EncodeWorkerToLobbyMessage(
        WorkerToLobbyMessage{WorkerRegister{
            WorkerId{1U}, "127.0.0.1", 7100U, 7101U, 1U}});
    EXPECT_EQ(
        DecodeError::InvalidValue,
        DecodeLobbyToWorkerMessage(
            registration.type, registration.payload).error);
    EXPECT_THROW(
        EncodeLobbyToWorkerMessage(LobbyToWorkerMessage{
            ReserveMatch{
                ReservationId{1U}, MatchId{1U}, 1U, 60000U, {}}}),
        std::invalid_argument);
}
```

Also cover 24 participants accepted, 25 rejected, duplicate PlayerId, unsorted input canonicalization, host length 255 accepted, 256 rejected, zero IDs, zero ports, capacity other than 1, invalid reject enum and trailing bytes.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because worker control message headers do not exist.

- [ ] Step 3: Implement the typed messages and bounded codec

Define these payloads exactly.

```cpp
struct ReservedParticipant
{
    PlayerId player;
    MatchTicketValue ticket;
    [[nodiscard]] bool operator==(const ReservedParticipant&) const = default;
};

struct WorkerRegister
{
    WorkerId worker;
    std::string advertisedHost;
    std::uint16_t gameTcpPort = 0U;
    std::uint16_t gameUdpPort = 0U;
    std::uint8_t capacity = 1U;
};

struct ReserveMatch
{
    ReservationId reservation;
    MatchId match;
    std::uint32_t seed = 0U;
    std::uint32_t ticketLifetimeMilliseconds = 60000U;
    std::vector<ReservedParticipant> participants;
};

struct MatchFinished
{
    MatchId match;
    EntityId winner;
    bool hasWinner = false;
    MatchCompletionReason reason = MatchCompletionReason::LastSurvivor;
    std::uint32_t finishedTick = 0U;
};

struct WorkerRegistered
{
    WorkerId worker;
};

struct ReserveMatchReady
{
    ReservationId reservation;
    MatchId match;
};

struct ReserveMatchRejected
{
    ReservationId reservation;
    MatchId match;
    WorkerReservationReject reason =
        WorkerReservationReject::InvalidReservation;
};

struct CancelMatchReservation
{
    ReservationId reservation;
    MatchId match;
};

struct MatchReservationCancelled
{
    ReservationId reservation;
    MatchId match;
};

using WorkerToLobbyMessage = std::variant<
    WorkerRegister,
    ReserveMatchReady,
    ReserveMatchRejected,
    MatchReservationCancelled,
    MatchFinished>;

using LobbyToWorkerMessage = std::variant<
    WorkerRegistered,
    ReserveMatch,
    CancelMatchReservation>;
```

Ready, reject and cancel messages carry both ReservationId and MatchId. Encoders sort participants by PlayerId and reject duplicates. Decoders validate all IDs, enum values, counts and trailing bytes before returning a message.

Encode fields in struct order. WorkerId and PlayerId are U32, ReservationId and MatchId are U64, host is String8, ports are U16, capacity and reject reason are U8, seed and lifetime are U32, participant count is U8 and each participant is PlayerId followed by 16 ticket bytes. MatchFinished writes MatchId, winner-present U8, optional EntityId U32, reason U8 and finished tick U32.

- [ ] Step 4: Run GREEN

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=WorkerControlCodec.*:TcpFrame.*
```

Expected: worker codec and existing 64KiB TCP frame tests pass.

- [ ] Step 5: Commit

```powershell
git add -- protocol/CMakeLists.txt protocol/include/dxa/protocol/WorkerControlMessages.hpp protocol/include/dxa/protocol/WorkerControlMessageCodec.hpp protocol/src/WorkerControlMessageCodec.cpp tests/CMakeLists.txt tests/protocol_worker_control_codec_test.cpp
git commit -m "feat(protocol): worker control message 추가" -m "이유: lobby와 game worker가 registration, reservation과 종료를 typed frame으로 교환해야 했다." -m "핵심 변경: message 13부터 20의 값 type, 방향별 variant와 bounded little-endian codec을 추가했다." -m "검증: header 부재 RED 뒤 round-trip, 방향 오류, 24명과 host 길이 경계 테스트를 통과했다."
```

---

### Task 3: Game TCP authentication and result codec

Files:

- Create: `protocol/include/dxa/protocol/GameTcpMessages.hpp`
- Create: `protocol/include/dxa/protocol/GameTcpMessageCodec.hpp`
- Create: `protocol/src/GameTcpMessageCodec.cpp`
- Create: `tests/protocol_game_tcp_codec_test.cpp`
- Modify: `protocol/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Task 1 IDs, tokens and Task 2 completion enum.
- Produces: `GameClientMessage`, `GameServerMessage`, direction-specific game TCP encode and decode functions.

- [ ] Step 1: Write failing exact-wire and validation tests

```cpp
TEST(GameTcpCodec, EncodesHelloInLockedOrder)
{
    const auto encoded = EncodeGameClientMessage(GameClientMessage{
        GameClientHello{MatchId{4U}, PlayerId{9U}, Ticket(0x20U)}});
    EXPECT_EQ(MessageType::GameClientHello, encoded.type);
    ASSERT_EQ(28U, encoded.payload.size());
    EXPECT_EQ(std::byte{0x04}, encoded.payload[0]);
    EXPECT_EQ(std::byte{0x09}, encoded.payload[8]);
}

TEST(GameTcpCodec, RoundTripsWelcomeErrorAndResult)
{
    ExpectServerRoundTrip(GameServerWelcome{
        MatchId{4U}, PlayerId{9U}, EntityId{0U},
        30U, 15U, 1U, 0x12345678U, Token(0x40U)});
    ExpectServerRoundTrip(GameServerErrorMessage{
        GameServerErrorCode::AuthenticationFailed});
    ExpectServerRoundTrip(GameMatchResult{
        MatchId{4U}, EntityId{0U}, true,
        MatchCompletionReason::LastSurvivor, 90U});
}
```

Name the message struct `GameServerErrorMessage` and its enum `GameServerErrorCode`. Test wrong-direction type, tick rate other than 30, snapshot rate other than 15, map ID zero, invalid enum, absent winner with LastSurvivor and trailing bytes. Preserve the existing rule that protocol PlayerId, MatchId and EntityId value 0 is representable; only WorkerId, ReservationId and input sequence reject zero.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because game TCP message headers do not exist.

- [ ] Step 3: Implement message 21 through 24

Use these interfaces.

```cpp
struct GameClientHello
{
    MatchId match;
    PlayerId player;
    MatchTicketValue ticket;
};

struct GameServerWelcome
{
    MatchId match;
    PlayerId player;
    EntityId actor;
    std::uint16_t tickRate = 30U;
    std::uint16_t snapshotRate = 15U;
    std::uint32_t mapId = 1U;
    std::uint32_t navMeshCrc32 = 0U;
    UdpSessionToken udpToken;
};

struct GameServerErrorMessage
{
    GameServerErrorCode error =
        GameServerErrorCode::AuthenticationFailed;
};

struct GameMatchResult
{
    MatchId match;
    EntityId winner;
    bool hasWinner = false;
    MatchCompletionReason reason = MatchCompletionReason::LastSurvivor;
    std::uint32_t finishedTick = 0U;
};

using GameClientMessage = std::variant<GameClientHello>;
using GameServerMessage = std::variant<
    GameServerWelcome,
    GameServerErrorMessage,
    GameMatchResult>;

[[nodiscard]] EncodedMessage EncodeGameClientMessage(
    const GameClientMessage& message);
[[nodiscard]] EncodedMessage EncodeGameServerMessage(
    const GameServerMessage& message);
[[nodiscard]] MessageDecodeResult<GameClientMessage> DecodeGameClientMessage(
    MessageType type,
    std::span<const std::byte> payload);
[[nodiscard]] MessageDecodeResult<GameServerMessage> DecodeGameServerMessage(
    MessageType type,
    std::span<const std::byte> payload);
```

Encode optional winner as one canonical bool byte followed by EntityId only when present. Public authentication failures contain no ticket lookup detail and no free-form text.

Encode GameClientHello as MatchId U64, PlayerId U32 and 16 ticket bytes. Encode welcome in struct order with Actor EntityId U32, both rates U16, map ID and CRC U32 and token 16 bytes. Error is one U8 enum. Match result is MatchId U64, winner-present U8, optional EntityId U32, reason U8 and finished tick U32.

- [ ] Step 4: Run GREEN

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=GameTcpCodec.*:WorkerControlCodec.*
```

Expected: all selected protocol tests pass.

- [ ] Step 5: Commit

```powershell
git add -- protocol/CMakeLists.txt protocol/include/dxa/protocol/GameTcpMessages.hpp protocol/include/dxa/protocol/GameTcpMessageCodec.hpp protocol/src/GameTcpMessageCodec.cpp tests/CMakeLists.txt tests/protocol_game_tcp_codec_test.cpp
git commit -m "feat(protocol): game TCP 인증 계약 추가" -m "이유: match ticket을 한 번 소비하고 UDP session token과 reliable result를 전달할 channel이 필요했다." -m "핵심 변경: GameClientHello, GameServerWelcome, 제한된 공개 오류와 match result codec을 추가했다." -m "검증: header 부재 RED 뒤 exact-wire, round-trip, 방향과 enum 경계 테스트를 통과했다."
```

---

### Task 4: Bounded UDP frame, input and snapshot fragmentation

Files:

- Create: `protocol/include/dxa/protocol/Crc32.hpp`
- Create: `protocol/src/Crc32.cpp`
- Create: `protocol/include/dxa/protocol/GameUdpMessages.hpp`
- Create: `protocol/include/dxa/protocol/GameUdpCodec.hpp`
- Create: `protocol/src/GameUdpCodec.cpp`
- Create: `tests/protocol_game_udp_codec_test.cpp`
- Modify: `protocol/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Task 1 byte codec, ID, token and UDP constants.
- Produces: `UdpBind`, `UdpBindAccepted`, `ClientInput`, `SnapshotFragment`, `EncodedDatagram`, CRC32 and snapshot fragmentation.

- [ ] Step 1: Write failing header, input, CRC and size tests

```cpp
TEST(GameUdpCodec, EncodesTenByteHeaderAndInput)
{
    const EncodedDatagram encoded = EncodeClientDatagram(ClientDatagram{
        ClientInput{
            MatchId{3U}, PlayerId{7U}, Token(0x10U), 9U,
            NetworkVec2{1.0F, -2.0F}, true,
            EntityId{4U}, true}});
    EXPECT_LE(encoded.bytes.size(), MaxUdpDatagramBytes);
    EXPECT_EQ(std::byte{0x44}, encoded.bytes[0]);
    EXPECT_EQ(std::byte{0x58}, encoded.bytes[1]);
    EXPECT_EQ(std::byte{0x55}, encoded.bytes[2]);
    EXPECT_EQ(std::byte{0x31}, encoded.bytes[3]);
}

TEST(GameUdpCodec, FragmentsAtExactlyElevenHundredFiftyEightBytes)
{
    const std::vector<std::byte> payload(MaxSnapshotFragmentPayloadBytes + 1U);
    const auto fragments = FragmentSnapshot(
        MatchId{1U}, 2U, 4U, 6U, payload);
    ASSERT_EQ(2U, fragments.size());
    EXPECT_EQ(MaxSnapshotFragmentPayloadBytes, fragments[0].bytes.size());
    EXPECT_EQ(1U, fragments[1].bytes.size());
    EXPECT_EQ(Crc32(payload), fragments[0].fullPayloadCrc32);
}
```

Also cover exact 1,200-byte datagram accepted, 1,201 rejected, payload length mismatch, bad magic, version, reserved byte and type, zero input sequence, invalid bool, NaN and infinity, fragment count 32 accepted, 33 rejected, index out of range and payload above 37,056 bytes.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because UDP and CRC headers do not exist.

- [ ] Step 3: Implement the exact datagram contract

Define a 10-byte common header and keep encoded bytes in one value.

```cpp
struct NetworkVec2
{
    float x = 0.0F;
    float z = 0.0F;
    [[nodiscard]] bool operator==(const NetworkVec2&) const = default;
};

struct ClientInput
{
    MatchId match;
    PlayerId player;
    UdpSessionToken token;
    std::uint32_t inputSequence = 0U;
    NetworkVec2 moveDestination;
    bool hasMoveDestination = false;
    EntityId attackTarget;
    bool hasAttackTarget = false;
};

struct UdpBind
{
    MatchId match;
    PlayerId player;
    UdpSessionToken token;
};

struct UdpBindAccepted
{
    MatchId match;
    PlayerId player;
    std::uint32_t serverTick = 0U;
};

struct SnapshotFragment
{
    MatchId match;
    std::uint32_t snapshotId = 0U;
    std::uint32_t serverTick = 0U;
    std::uint32_t ackInputSequence = 0U;
    std::uint16_t fragmentIndex = 0U;
    std::uint16_t fragmentCount = 0U;
    std::uint32_t fullPayloadBytes = 0U;
    std::uint32_t fullPayloadCrc32 = 0U;
    std::vector<std::byte> bytes;
};

using ClientDatagram = std::variant<UdpBind, ClientInput>;
using ServerDatagram = std::variant<UdpBindAccepted, SnapshotFragment>;

struct EncodedDatagram
{
    UdpDatagramType type = UdpDatagramType::Bind;
    std::vector<std::byte> bytes;
};

template <typename DatagramVariant>
struct DatagramDecodeResult
{
    std::optional<DatagramVariant> datagram;
    DecodeError error = DecodeError::None;
};

[[nodiscard]] EncodedDatagram EncodeClientDatagram(
    const ClientDatagram& datagram);
[[nodiscard]] EncodedDatagram EncodeServerDatagram(
    const ServerDatagram& datagram);
[[nodiscard]] DatagramDecodeResult<ClientDatagram> DecodeClientDatagram(
    std::span<const std::byte> bytes);
[[nodiscard]] DatagramDecodeResult<ServerDatagram> DecodeServerDatagram(
    std::span<const std::byte> bytes);
[[nodiscard]] std::vector<SnapshotFragment> FragmentSnapshot(
    MatchId match,
    std::uint32_t snapshotId,
    std::uint32_t serverTick,
    std::uint32_t ackInputSequence,
    std::span<const std::byte> fullPayload);
```

CRC32 uses polynomial `0xEDB88320`, initial value `0xFFFFFFFF` and final XOR `0xFFFFFFFF`. `FragmentSnapshot` rejects empty payload, assigns index 0 through count minus one and repeats the same length and CRC metadata. UDP decode validates frame structure first and semantic values second; any failure returns no partial datagram.

The common header is magic four bytes, protocol version U16, datagram type U8, reserved U8 zero and payload length U16. UdpBind payload is MatchId U64, PlayerId U32 and token 16 bytes. BindAccepted is MatchId U64, PlayerId U32 and server tick U32. ClientInput starts with the bind identity, then input sequence U32 and one flags U8 where bit 0 means move and bit 1 means attack; all higher bits are invalid. Present move writes F32 x and z, present attack writes EntityId U32. SnapshotFragment writes the metadata fields in the struct order before raw fragment bytes.

- [ ] Step 4: Run GREEN

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=GameUdpCodec.*:Crc32.*:ByteCodec.*
```

Expected: UDP size, exact byte, finite float, fragmentation and CRC tests pass.

- [ ] Step 5: Commit

```powershell
git add -- protocol/CMakeLists.txt protocol/include/dxa/protocol/Crc32.hpp protocol/src/Crc32.cpp protocol/include/dxa/protocol/GameUdpMessages.hpp protocol/include/dxa/protocol/GameUdpCodec.hpp protocol/src/GameUdpCodec.cpp tests/CMakeLists.txt tests/protocol_game_udp_codec_test.cpp
git commit -m "feat(protocol): 1200바이트 UDP frame 추가" -m "이유: input과 full-state snapshot fragment가 같은 bounded datagram 계약을 사용해야 했다." -m "핵심 변경: DXU1 header, bind와 input codec, CRC32, 1158바이트 fragment 분할과 32개 제한을 추가했다." -m "검증: header 부재 RED 뒤 exact size, malformed frame, finite float, CRC와 fragment 경계 테스트를 통과했다."
```

---

### Task 5: Full-state world snapshot codec

Files:

- Create: `protocol/include/dxa/protocol/GameSnapshot.hpp`
- Create: `protocol/include/dxa/protocol/GameSnapshotCodec.hpp`
- Create: `protocol/src/GameSnapshotCodec.cpp`
- Create: `tests/protocol_game_snapshot_codec_test.cpp`
- Modify: `protocol/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Task 4 `NetworkVec2`, EntityId and byte codec support.
- Produces: platform-neutral `GameSnapshot`, `EncodeGameSnapshot`, `DecodeGameSnapshot`.

- [ ] Step 1: Write failing round-trip and malformed-state tests

```cpp
TEST(GameSnapshotCodec, RoundTripsFullState)
{
    GameSnapshot source;
    source.phase = NetworkMatchPhase::Running;
    source.safeZoneStage = NetworkSafeZoneStage::Stage1;
    source.safeZoneCenter = {0.0F, 0.0F};
    source.safeZoneRadius = 128.0F;
    source.aliveContenders = 2U;
    source.actors = {
        NetworkActorSnapshot{
            EntityId{0U}, NetworkActorRole::Contender,
            NetworkNeutralArchetype::None, {1.0F, 2.0F},
            100, true, NetworkWeaponType::Blade, 0U, 0U}};
    source.loot = {
        NetworkLootSnapshot{
            1U, NetworkLootType::Rifle, {3.0F, 4.0F}, true}};
    const auto bytes = EncodeGameSnapshot(source);
    const auto decoded = DecodeGameSnapshot(bytes);
    ASSERT_TRUE(decoded.snapshot.has_value());
    EXPECT_EQ(source, *decoded.snapshot);
}

TEST(GameSnapshotCodec, RejectsDuplicateIdsAndNonFinitePositions)
{
    GameSnapshot invalid = MinimalSnapshot();
    invalid.actors.push_back(invalid.actors.front());
    EXPECT_THROW(EncodeGameSnapshot(invalid), std::invalid_argument);
    invalid = MinimalSnapshot();
    invalid.actors.front().position.x =
        std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(EncodeGameSnapshot(invalid), std::invalid_argument);
}
```

Also cover 124 actors accepted, 125 rejected, 60 loot accepted, 61 rejected, sorted canonical output, alive count mismatch, invalid enum, invalid bool, negative radius, result presence rules, truncated bytes and trailing bytes.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because snapshot wire types do not exist.

- [ ] Step 3: Implement wire-only world types and bounded allocation

Define locked enums mirroring simulation values without including simulation headers in protocol. Use explicit conversion later in `dxa_game_common`.

```cpp
inline constexpr std::size_t MaxSnapshotActors = 124U;
inline constexpr std::size_t MaxSnapshotLoot = 60U;

enum class NetworkActorRole : std::uint8_t
{
    Contender = 1,
    Neutral = 2
};

enum class NetworkNeutralArchetype : std::uint8_t
{
    None = 0,
    Melee = 1,
    Ranged = 2
};

enum class NetworkWeaponType : std::uint8_t
{
    Blade = 1,
    Rifle = 2,
    ArcPulse = 3
};

enum class NetworkLootType : std::uint8_t
{
    Rifle = 1,
    ArcPulse = 2,
    MedKit = 3
};

enum class NetworkMatchPhase : std::uint8_t
{
    Waiting = 1,
    Running = 2,
    SuddenDeath = 3,
    Finished = 4
};

enum class NetworkSafeZoneStage : std::uint8_t
{
    Stage1 = 1,
    Stage2 = 2,
    Stage3 = 3,
    Stage4 = 4,
    SuddenDeath = 5
};

enum class NetworkMatchEndReason : std::uint8_t
{
    LastSurvivor = 1,
    TimeLimit = 2
};

struct NetworkActorSnapshot
{
    EntityId id;
    NetworkActorRole role = NetworkActorRole::Contender;
    NetworkNeutralArchetype neutralArchetype =
        NetworkNeutralArchetype::None;
    NetworkVec2 position;
    std::int32_t health = 100;
    bool alive = true;
    NetworkWeaponType weapon = NetworkWeaponType::Blade;
    std::uint32_t cooldownTicksRemaining = 0U;
    std::uint32_t eliminations = 0U;
};

struct NetworkLootSnapshot
{
    std::uint32_t id = 0U;
    NetworkLootType type = NetworkLootType::Rifle;
    NetworkVec2 position;
    bool active = true;
};

struct NetworkMatchResult
{
    EntityId winner;
    NetworkMatchEndReason reason = NetworkMatchEndReason::LastSurvivor;
    std::uint32_t finishedTick = 0U;
};

struct GameSnapshot
{
    NetworkMatchPhase phase = NetworkMatchPhase::Waiting;
    NetworkSafeZoneStage safeZoneStage = NetworkSafeZoneStage::Stage1;
    NetworkVec2 safeZoneCenter;
    float safeZoneRadius = 128.0F;
    std::uint32_t aliveContenders = 0U;
    std::vector<NetworkActorSnapshot> actors;
    std::vector<NetworkLootSnapshot> loot;
    NetworkMatchResult result;
    bool hasResult = false;
    std::uint64_t eventChecksum = 0U;
};
```

Encode actor and loot in ascending ID order. Before allocating, decode counts and compare them with both fixed maxima and remaining bytes. Reject any duplicate ID, invalid enum or non-finite float before publishing a snapshot.
Give every wire value struct a defaulted equality operator so round-trip tests compare values rather than serialized buffers.

The full payload order is phase U8, safe-zone stage U8, center F32 x and z, radius F32, alive contender count U32, actor count U16, actor records, loot count U16, loot records, result-present U8, optional result, then event checksum U64. Actor records use EntityId U32, role U8, archetype U8, position F32 x and z, health signed bits in U32, alive U8, weapon U8, cooldown U32 and eliminations U32. Loot records use ID U32, type U8, position F32 x and z and active U8. A result uses winner EntityId U32, reason U8 and finished tick U32.

- [ ] Step 4: Run GREEN

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=GameSnapshotCodec.*:GameUdpCodec.*
```

Expected: snapshot round-trip, canonical ordering and malformed payload tests pass.

- [ ] Step 5: Commit

```powershell
git add -- protocol/CMakeLists.txt protocol/include/dxa/protocol/GameSnapshot.hpp protocol/include/dxa/protocol/GameSnapshotCodec.hpp protocol/src/GameSnapshotCodec.cpp tests/CMakeLists.txt tests/protocol_game_snapshot_codec_test.cpp
git commit -m "feat(protocol): full-state snapshot codec 추가" -m "이유: simulation object를 그대로 송신하지 않고 124 actor와 60 loot의 명시적 wire state가 필요했다." -m "핵심 변경: network enum과 world snapshot 값 type, canonical ordering, bounded encode와 decode를 추가했다." -m "검증: header 부재 RED 뒤 population 경계, duplicate ID, finite float와 trailing byte 테스트를 통과했다."
```

---

### Task 6: Lobby의 비동기 start transaction 전환

Files:

- Create: `apps/lobby_server/include/dxa/lobby/LobbyRuntimeTypes.hpp`
- Modify: `apps/lobby_server/include/dxa/lobby/LobbyService.hpp`
- Modify: `apps/lobby_server/src/LobbyService.cpp`
- Modify: `apps/lobby_server/include/dxa/lobby/MatchTicketRegistry.hpp`
- Modify: `apps/lobby_server/src/MatchTicketRegistry.cpp`
- Modify: `apps/lobby_server/include/dxa/lobby/LobbyTcpServer.hpp`
- Modify: `apps/lobby_server/src/LobbyTcpServer.cpp`
- Modify: `apps/lobby_server/include/dxa/lobby/LobbyServerOptions.hpp`
- Modify: `apps/lobby_server/src/LobbyServerOptions.cpp`
- Modify: `apps/lobby_server/src/main.cpp`
- Modify: `tests/lobby_service_test.cpp`
- Modify: `tests/lobby_ticket_test.cpp`
- Modify: `tests/support/lobby_network_fixture.hpp`
- Modify: `tests/lobby_server_options_test.cpp`
- Modify: `tests/lobby_tcp_integration_test.cpp`
- Modify: `apps/lobby_server/CMakeLists.txt`
- Delete: `apps/lobby_server/include/dxa/lobby/GameWorkerAllocator.hpp`
- Delete: `apps/lobby_server/src/GameWorkerAllocator.cpp`

Interfaces:

- Consumes: existing Room, ConnectionId, ticket registry and Task 2 reservation payload.
- Produces: `LobbyRuntimeAction`, `WorkerEvent`, `LobbyService::HandleWorkerEvent`, pending reservation state and asynchronous rollback.

- [ ] Step 1: Replace synchronous-start expectations with failing action tests

Delete tests that treat a static endpoint as a live worker. Keep all room validation tests. Add assertions at the service interface.

```cpp
TEST(LobbyService, StartsReservationWithoutPublishingTickets)
{
    ServiceFixture fixture;
    const ReadyRoom ready = ReadyTwoPlayerRoom(fixture);
    const LobbyServiceResult output = fixture.service.Handle(
        ready.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));

    EXPECT_EQ(RoomState::Starting, SnapshotFor(output, ready.host).state);
    EXPECT_FALSE(HasMessage<MatchTicket>(output));
    ASSERT_EQ(1U, output.actions.size());
    const auto& reserve = std::get<ReserveMatchAction>(output.actions.front());
    EXPECT_EQ(ReservationId{1U}, reserve.reservation);
    EXPECT_EQ(MatchId{1U}, reserve.match);
    ASSERT_EQ(2U, reserve.participants.size());
}

TEST(LobbyService, PublishesTicketsOnlyAfterMatchingReadyEvent)
{
    ServiceFixture fixture;
    const ReadyRoom ready = ReadyTwoPlayerRoom(fixture);
    const auto start = fixture.service.Handle(
        ready.host,
        ClientMessage{StartMatchRequest{5U}},
        Time(0));
    const auto reserve = std::get<ReserveMatchAction>(start.actions.front());

    const auto completed = fixture.service.HandleWorkerEvent(
        ReservationReadyEvent{
            reserve.reservation,
            reserve.match,
            WorkerId{3U},
            GameEndpoint{"127.0.0.1", 7100U, 7101U}},
        Time(1));

    EXPECT_EQ(RoomState::InMatch, SnapshotFor(completed, ready.host).state);
    EXPECT_EQ(2U, CountMessages<MatchTicket>(completed));
}
```

Add focused tests for worker unavailable, reject, timeout event, wrong ReservationId, ready after rollback, all ready flags preserved, ticket source partial failure, Starting command rejection, Starting host disconnect cancel and transfer, InMatch lobby disconnect retaining match membership, MatchFinished cleanup and active worker loss producing `MatchUnavailable`.

Adapt the existing public TCP fixture to capture a ReserveMatchAction and feed a ReservationReadyEvent through the same LobbyTcpServer seam that Task 8 will connect to the real worker adapter. Keep a no-handler test that rolls back as WorkerUnavailable rather than leaving a room in Starting forever.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because runtime actions and worker events do not exist.

- [ ] Step 3: Add event and action values

Define the service seam in one header.

```cpp
struct ReserveMatchAction
{
    ReservationId reservation;
    RoomId room;
    MatchId match;
    PlayerId requester;
    std::uint32_t requestId = 0U;
    std::uint32_t seed = 0U;
    std::chrono::steady_clock::time_point issuedAt;
    std::vector<ReservedParticipant> participants;
};

struct CancelReservationAction
{
    ReservationId reservation;
    MatchId match;
};

using LobbyRuntimeAction = std::variant<
    ReserveMatchAction,
    CancelReservationAction>;

struct ReservationReadyEvent
{
    ReservationId reservation;
    MatchId match;
    WorkerId worker;
    GameEndpoint endpoint;
};

struct ReservationFailedEvent
{
    ReservationId reservation;
    MatchId match;
};

struct MatchFinishedEvent
{
    WorkerId worker;
    MatchId match;
};

struct MatchUnavailableEvent
{
    WorkerId worker;
    MatchId match;
};

using WorkerEvent = std::variant<
    ReservationReadyEvent,
    ReservationFailedEvent,
    MatchFinishedEvent,
    MatchUnavailableEvent>;
```

Add `actions` to `LobbyServiceResult`, `nextReservation` to `LobbyServiceConfig`, and store pending reservation records by ReservationId and MatchId. Change the constructor to:

```cpp
explicit LobbyService(
    MatchTicketRegistry& tickets,
    LobbyServiceConfig config = {});

[[nodiscard]] LobbyServiceResult HandleWorkerEvent(
    const WorkerEvent& event,
    std::chrono::steady_clock::time_point now);
```

Add `matchSeedBase = 20260824U` to LobbyServiceConfig. Compute each reservation seed as `matchSeedBase ^ low32(MatchId) ^ high32(MatchId)` so tests reproduce a match while sequential matches do not all reuse the same spawn sequence.

Start changes Room to Starting, issues all tickets, stores the requester request ID and returns one ReserveMatchAction. Ready computes remaining lifetime from issuedAt; if less than one second remains it uses the same failure path. Failure revokes the batch, returns Room to Waiting, broadcasts request ID 0 snapshots and sends `WorkerUnavailable` only to a still-connected requester.

On ready, move the ticket batch from the pending record into an active-match record so the lobby uniqueness registry remains bounded. MatchFinished and MatchUnavailable revoke that active batch during room cleanup; the game worker remains the only runtime consumer.

Starting disconnect returns a CancelReservationAction before applying normal Waiting leave rules. InMatch lobby disconnect removes connection routing only. MatchFinished and MatchUnavailable erase the room and player-to-room indexes; only MatchUnavailable sends error code 20.

Add this stable typed seam to LobbyTcpServer so every intermediate commit builds and existing loopback coverage remains meaningful.

```cpp
using LobbyRuntimeActionHandler =
    std::function<void(const LobbyRuntimeAction&)>;

void SetRuntimeActionHandler(LobbyRuntimeActionHandler handler);
void ApplyWorkerEvent(
    const WorkerEvent& event,
    std::chrono::steady_clock::time_point now);
```

The server result pump sends public outbound first, then invokes the action handler. With no handler it immediately feeds ReservationFailedEvent back into LobbyService. Remove the static worker fields and flags from LobbyServerOptions, construct LobbyService with only MatchTicketRegistry in main, and reject old `--worker-host` options as unknown. Task 8 adds the separate control listener to this seam.

- [ ] Step 4: Run GREEN and existing lobby regression tests

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=LobbyService.*:LobbyRoom.*:LobbyTicket.*
```

Expected: asynchronous start and all prior room, ticket expiry and request-order tests pass.

- [ ] Step 5: Commit

```powershell
git add -- apps/lobby_server/CMakeLists.txt apps/lobby_server/include/dxa/lobby/LobbyRuntimeTypes.hpp apps/lobby_server/include/dxa/lobby/LobbyService.hpp apps/lobby_server/src/LobbyService.cpp apps/lobby_server/include/dxa/lobby/MatchTicketRegistry.hpp apps/lobby_server/src/MatchTicketRegistry.cpp apps/lobby_server/include/dxa/lobby/LobbyTcpServer.hpp apps/lobby_server/src/LobbyTcpServer.cpp apps/lobby_server/include/dxa/lobby/LobbyServerOptions.hpp apps/lobby_server/src/LobbyServerOptions.cpp apps/lobby_server/src/main.cpp tests/lobby_service_test.cpp tests/lobby_ticket_test.cpp tests/support/lobby_network_fixture.hpp tests/lobby_server_options_test.cpp tests/lobby_tcp_integration_test.cpp
git rm -- apps/lobby_server/include/dxa/lobby/GameWorkerAllocator.hpp apps/lobby_server/src/GameWorkerAllocator.cpp
git commit -m "refactor(lobby): 비동기 match reservation 전환" -m "이유: 실제 worker ready 전에는 room을 InMatch로 만들거나 ticket을 client에게 전달하면 안 됐다." -m "핵심 변경: 동기 static allocator를 제거하고 pending reservation action, worker event, rollback과 match cleanup을 추가했다." -m "검증: synchronous start 기대의 RED 뒤 ready 전 ticket 부재, 성공, 실패, disconnect와 cleanup 테스트를 통과했다."
```

---

### Task 7: Worker registry state machine

Files:

- Create: `apps/lobby_server/include/dxa/lobby/WorkerRegistry.hpp`
- Create: `apps/lobby_server/src/WorkerRegistry.cpp`
- Create: `tests/lobby_worker_registry_test.cpp`
- Modify: `apps/lobby_server/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Task 2 typed control messages and Task 6 runtime action and event.
- Produces: pure `WorkerRegistry` with register, execute, receive, disconnect and timeout transitions.

- [ ] Step 1: Write failing state-machine tests

```cpp
TEST(WorkerRegistry, AssignsLowestIdleWorkerAndWaitsForReady)
{
    WorkerRegistry registry;
    EXPECT_TRUE(registry.Register(
        WorkerConnectionId{20U},
        WorkerRegister{WorkerId{8U}, "worker8", 7200U, 7201U, 1U}).accepted);
    EXPECT_TRUE(registry.Register(
        WorkerConnectionId{10U},
        WorkerRegister{WorkerId{3U}, "worker3", 7100U, 7101U, 1U}).accepted);

    const auto assigned = registry.Execute(ReserveAction(), Time(0));
    ASSERT_EQ(1U, assigned.outbound.size());
    EXPECT_EQ(WorkerConnectionId{10U}, assigned.outbound.front().recipient);
    EXPECT_TRUE(assigned.events.empty());
}

TEST(WorkerRegistry, TimeoutFailsReservationAndClosesWorker)
{
    WorkerRegistry registry = RegistryWithOneIdleWorker();
    const auto assigned = registry.Execute(ReserveAction(), Time(0));
    ASSERT_EQ(1U, assigned.timers.size());
    const auto timeout = registry.Timeout(ReservationId{1U});
    EXPECT_TRUE(ContainsEvent<ReservationFailedEvent>(timeout));
    EXPECT_EQ(1U, timeout.closeConnections.size());
    EXPECT_EQ(0U, registry.IdleCount());
}
```

Cover duplicate WorkerId, duplicate endpoint, capacity other than 1, zero ID and ports, visible ASCII host boundary, no idle worker immediate failure, reject returning worker to Idle, ready transition to Active, cancel transition to Cancelling, cancel ACK returning Idle, cancel timeout close, mismatched ReservationId close, Reserved disconnect failure, Active disconnect MatchUnavailable, MatchFinished returning Idle and late frame no state change.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because `WorkerRegistry.hpp` does not exist.

- [ ] Step 3: Implement one pure state transition module

Use a local connection ID distinct from public lobby ConnectionId.

```cpp
struct WorkerConnectionId
{
    std::uint64_t value = 0U;
    [[nodiscard]] auto operator<=>(const WorkerConnectionId&) const = default;
};

enum class WorkerState
{
    Idle,
    Reserved,
    Cancelling,
    Active
};

struct WorkerControlOutbound
{
    WorkerConnectionId recipient;
    LobbyToWorkerMessage message;
};

enum class ReservationTimerKind
{
    Start,
    Cancel
};

struct ReservationTimerDirective
{
    ReservationTimerKind kind = ReservationTimerKind::Start;
    ReservationId reservation;
    std::chrono::milliseconds duration{2000};
};

struct WorkerRegistryResult
{
    bool accepted = false;
    std::vector<WorkerControlOutbound> outbound;
    std::vector<WorkerEvent> events;
    std::vector<ReservationTimerDirective> timers;
    std::vector<WorkerConnectionId> closeConnections;
};
```

Expose these operations and no map accessors other than counts used for diagnostics.

```cpp
[[nodiscard]] WorkerRegistryResult Register(
    WorkerConnectionId connection,
    const WorkerRegister& registration);
[[nodiscard]] WorkerRegistryResult Execute(
    const LobbyRuntimeAction& action,
    std::chrono::steady_clock::time_point now);
[[nodiscard]] WorkerRegistryResult Receive(
    WorkerConnectionId connection,
    const WorkerToLobbyMessage& message);
[[nodiscard]] WorkerRegistryResult Disconnect(
    WorkerConnectionId connection);
[[nodiscard]] WorkerRegistryResult Timeout(
    ReservationId reservation);
```

`ReservationTimerDirective` has Start or Cancel kind, ReservationId and a production duration of two seconds. Registry selection iterates WorkerId ordered storage and never returns Cancelling, Reserved or Active workers.

For ReserveMatchAction, compute the protocol ticket lifetime as `issuedAt + 60 seconds - now`, clamp it to whole milliseconds and fail immediately if it is not positive. This keeps lobby and worker expiry windows tied to the same issuance event without transferring a steady-clock epoch across processes.

- [ ] Step 4: Run GREEN

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=WorkerRegistry.*:LobbyService.*
```

Expected: registry transition and service action tests pass without sockets or sleep.

- [ ] Step 5: Commit

```powershell
git add -- apps/lobby_server/CMakeLists.txt apps/lobby_server/include/dxa/lobby/WorkerRegistry.hpp apps/lobby_server/src/WorkerRegistry.cpp tests/CMakeLists.txt tests/lobby_worker_registry_test.cpp
git commit -m "feat(lobby): worker registry state machine 추가" -m "이유: worker 선택, reservation timeout과 disconnect를 socket callback에서 직접 관리하지 않아야 했다." -m "핵심 변경: Idle, Reserved, Cancelling, Active 전이와 typed outbound, timer, service event 결과를 추가했다." -m "검증: header 부재 RED 뒤 lowest-id 선택, reject, ready, cancel, timeout과 disconnect 전이 테스트를 통과했다."
```

---

### Task 8: Worker control TCP와 lobby runtime 연결

Files:

- Create: `apps/lobby_server/include/dxa/lobby/WorkerControlServer.hpp`
- Create: `apps/lobby_server/src/WorkerControlServer.cpp`
- Create: `tests/lobby_worker_control_integration_test.cpp`
- Modify: `apps/lobby_server/include/dxa/lobby/LobbyTcpServer.hpp`
- Modify: `apps/lobby_server/src/LobbyTcpServer.cpp`
- Modify: `apps/lobby_server/include/dxa/lobby/LobbyServerOptions.hpp`
- Modify: `apps/lobby_server/src/LobbyServerOptions.cpp`
- Modify: `apps/lobby_server/src/main.cpp`
- Modify: `apps/lobby_server/CMakeLists.txt`
- Modify: `tests/support/lobby_network_fixture.hpp`
- Modify: `tests/lobby_server_options_test.cpp`
- Modify: `tests/lobby_tcp_integration_test.cpp`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Task 6 LobbyService result and Task 7 WorkerRegistry.
- Produces: actual control acceptor, two-second timers and unified lobby runtime with public and control ports.

- [ ] Step 1: Write failing parser and loopback tests

```cpp
TEST(LobbyServerOptions, ParsesSeparateWorkerControlListener)
{
    const auto parsed = Parse({
        "--bind", "127.0.0.1",
        "--port", "7000",
        "--worker-bind", "127.0.0.1",
        "--worker-port", "7001"});
    ASSERT_TRUE(parsed.options.has_value());
    EXPECT_EQ(7001U, parsed.options->workerPort);
    EXPECT_FALSE(Parse({"--worker-host", "127.0.0.1"}).options.has_value());
}

TEST(LobbyWorkerControl, DoesNotPublishTicketBeforeRealWorkerReady)
{
    LobbyNetworkFixture fixture;
    WorkerProbe worker{fixture.Io(), fixture.WorkerPort()};
    worker.ConnectAndRegister(WorkerId{1U}, 7100U, 7101U);
    const ReadyNetworkRoom room = fixture.CreateReadyTwoPlayerRoom();
    room.host->client->StartMatch();
    fixture.RunUntil([&] { return worker.Reservations().size() == 1U; });
    EXPECT_FALSE(room.host->HasTicket());
    worker.SendReady(worker.Reservations().front());
    fixture.RunUntil([&] {
        return room.host->HasTicket() && room.guest->HasTicket();
    });
}
```

Add a fixture-configured 20ms timeout test that proves production default remains two seconds, no-worker immediate rollback, reject rollback, cancel on Starting disconnect, control close while Reserved, Active worker close MatchUnavailable and match-finished room cleanup.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: parser and fixture fail because worker control listener and port do not exist.

- [ ] Step 3: Implement the Asio adapter and result pump

`WorkerControlServer` accepts framed connections, requires WorkerRegister as the first frame, assigns monotonically increasing WorkerConnectionId and delegates every transition to WorkerRegistry. It owns one `steady_timer` per active ReservationId and converts timer callbacks into `registry.Timeout`.

```cpp
struct LobbyServerOptions
{
    std::string bindAddress = "127.0.0.1";
    std::uint16_t port = 7000U;
    std::string workerBindAddress = "127.0.0.1";
    std::uint16_t workerPort = 7001U;
};

struct WorkerControlServerConfig
{
    std::chrono::milliseconds reservationTimeout{2000};
};

using WorkerEventHandler = std::function<void(WorkerEvent)>;

class WorkerControlServer
{
public:
    void Start();
    void Stop();
    void Execute(
        const LobbyRuntimeAction& action,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::uint16_t LocalPort() const;
};
```

Extend `LobbyTcpServer` to own the control adapter and add `WorkerControlPort()`. Its result pump follows one ordering: queue public outbound, write audit, pass runtime actions to WorkerControlServer, then feed resulting worker events back to `LobbyService::HandleWorkerEvent`. All callbacks remain on one io_context thread.

Replace static endpoint options with `workerBindAddress` and `workerPort`. User-facing parsers accept only ports 1 through 65,535; test constructors pass endpoint port 0 directly.

- [ ] Step 4: Run GREEN and the full lobby network subset

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=LobbyServerOptions.*:LobbyWorkerControl.*:LobbyTcpIntegration.*:AsioFramedConnection.*
```

Expected: real control and public TCP tests pass, including ready gating and timeout rollback.

- [ ] Step 5: Commit

```powershell
git add -- apps/lobby_server/CMakeLists.txt apps/lobby_server/include/dxa/lobby/WorkerControlServer.hpp apps/lobby_server/src/WorkerControlServer.cpp apps/lobby_server/include/dxa/lobby/LobbyTcpServer.hpp apps/lobby_server/src/LobbyTcpServer.cpp apps/lobby_server/include/dxa/lobby/LobbyServerOptions.hpp apps/lobby_server/src/LobbyServerOptions.cpp apps/lobby_server/src/main.cpp tests/support/lobby_network_fixture.hpp tests/lobby_server_options_test.cpp tests/lobby_tcp_integration_test.cpp tests/lobby_worker_control_integration_test.cpp tests/CMakeLists.txt
git commit -m "feat(lobby): worker control TCP 연결" -m "이유: room start가 설정된 endpoint가 아니라 실제 등록 worker의 ready 응답을 기다려야 했다." -m "핵심 변경: control acceptor, registration frame, reservation timer와 public lobby result pump를 연결했다." -m "검증: listener 부재 RED 뒤 실제 TCP ready gate, reject, timeout, cancel과 worker disconnect 테스트를 통과했다."
```

---

### Task 9: Canonical arena map과 fingerprint 공유

Files:

- Create: `simulation/include/dxa/simulation/ArenaMap.hpp`
- Create: `simulation/src/ArenaMap.cpp`
- Create: `apps/game_common/CMakeLists.txt`
- Create: `apps/game_common/include/dxa/game_common/ArenaFingerprint.hpp`
- Create: `apps/game_common/src/ArenaFingerprint.cpp`
- Create: `tests/simulation_arena_map_test.cpp`
- Modify: `simulation/CMakeLists.txt`
- Modify: `apps/offline_match_demo/src/main.cpp`
- Modify: `apps/offline_match_benchmark/src/main.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: NavMesh, Task 1 float codec and Task 4 CRC32.
- Produces: one map 1 definition, `BuildSurvivalArenaNavMesh`, `SurvivalArenaFingerprint` for server and client.

- [ ] Step 1: Write failing shared-map tests

```cpp
TEST(ArenaMap, BuildsMapOneWithStableSourceAndCoverage)
{
    const ArenaMapDefinition& definition = SurvivalArenaMapDefinition();
    EXPECT_EQ(1U, definition.mapId);
    EXPECT_EQ(4U, definition.vertices.size());
    EXPECT_EQ(2U, definition.triangles.size());
    EXPECT_FLOAT_EQ(4.0F, definition.gridCellSize);
    const NavMesh mesh = BuildSurvivalArenaNavMesh();
    EXPECT_TRUE(mesh.FindContainingTriangleGrid({0.0F, 0.0F}).triangle.has_value());
    EXPECT_FALSE(mesh.FindContainingTriangleGrid({129.0F, 0.0F}).triangle.has_value());
}

TEST(ArenaFingerprint, ChangesWhenCanonicalSourceChanges)
{
    const auto source = SurvivalArenaMapDefinition();
    const std::uint32_t canonical = SurvivalArenaFingerprint(source);
    auto changed = source;
    changed.vertices[0].x += 1.0F;
    EXPECT_NE(canonical, SurvivalArenaFingerprint(changed));
}
```

Also assert that two builds return the same fingerprint and NavMesh query results. Remove local `MakeArenaNavMesh` helpers from the demo and benchmark and verify their existing tests still pass.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because `ArenaMap.hpp` and `dxa_game_common` do not exist.

- [ ] Step 3: Implement one canonical source

```cpp
struct ArenaMapDefinition
{
    std::uint32_t mapId = 1U;
    std::vector<Vec2> vertices;
    std::vector<NavTriangleIndices> triangles;
    float gridCellSize = 4.0F;
};

[[nodiscard]] ArenaMapDefinition SurvivalArenaMapDefinition();
[[nodiscard]] NavMesh BuildSurvivalArenaNavMesh();
```

The source uses vertices `(-128,-128)`, `(128,-128)`, `(-128,128)`, `(128,128)` and triangles `(0,1,2)`, `(1,3,2)`. Fingerprint writes map ID, vertex count, each float bit, triangle count, every index and grid size with ByteWriter, then calls Task 4 CRC32.

- [ ] Step 4: Run GREEN and offline regressions

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=ArenaMap.*:ArenaFingerprint.*:OfflineMatch.*
```

Expected: shared map, fingerprint and existing offline match tests pass.

- [ ] Step 5: Commit

```powershell
git add -- CMakeLists.txt simulation/CMakeLists.txt simulation/include/dxa/simulation/ArenaMap.hpp simulation/src/ArenaMap.cpp apps/game_common/CMakeLists.txt apps/game_common/include/dxa/game_common/ArenaFingerprint.hpp apps/game_common/src/ArenaFingerprint.cpp apps/offline_match_demo/src/main.cpp apps/offline_match_benchmark/src/main.cpp tests/simulation_arena_map_test.cpp tests/CMakeLists.txt
git commit -m "refactor(simulation): canonical arena map 공유" -m "이유: server와 client가 복제된 NavMesh를 사용하면 prediction mismatch를 CRC로 신뢰성 있게 탐지할 수 없었다." -m "핵심 변경: map 1 source, NavMesh factory와 canonical CRC adapter를 추가하고 offline 실행 경로의 복제 생성을 제거했다." -m "검증: shared header 부재 RED 뒤 map coverage, fingerprint 변화와 기존 OfflineMatch 테스트를 통과했다."
```

---

### Task 10: Disconnect lifecycle simulation command

Files:

- Modify: `simulation/include/dxa/simulation/MatchTypes.hpp`
- Modify: `simulation/include/dxa/simulation/OfflineMatch.hpp`
- Modify: `simulation/src/OfflineMatchInternal.hpp`
- Modify: `simulation/src/OfflineMatch.cpp`
- Modify: `simulation/src/OfflineMatchStep.cpp`
- Modify: `tests/simulation_offline_match_test.cpp`

Interfaces:

- Consumes: existing OfflineMatch fixed tick and actor ordering.
- Produces: `MatchLifecycleCommand`, `ContenderExitReason::Disconnected`, `MatchEventType::ActorDisconnected` and Submit overload.

- [ ] Step 1: Write failing lifecycle tests

```cpp
TEST(OfflineMatch, DisconnectEliminatesContenderOnNextStep)
{
    OfflineMatch match = StartedCloseCombatMatch(3U);
    match.Submit(MatchLifecycleCommand{
        2U, ContenderExitReason::Disconnected});
    EXPECT_TRUE(ActorById(match.Snapshot(), 2U).alive);

    match.Step();

    EXPECT_FALSE(ActorById(match.Snapshot(), 2U).alive);
    EXPECT_EQ(0, ActorById(match.Snapshot(), 2U).health);
    EXPECT_EQ(2U, match.Snapshot().aliveContenders);
    EXPECT_TRUE(ContainsEvent(
        match.DrainEvents(), MatchEventType::ActorDisconnected, 2U));
}

TEST(OfflineMatch, DuplicateDisconnectChangesStateOnce)
{
    OfflineMatch match = StartedCloseCombatMatch(3U);
    match.Submit(MatchLifecycleCommand{2U, ContenderExitReason::Disconnected});
    match.Submit(MatchLifecycleCommand{2U, ContenderExitReason::Disconnected});
    match.Step();
    EXPECT_EQ(1U, CountEvents(
        match.DrainEvents(), MatchEventType::ActorDisconnected, 2U));
}
```

Also test command before Start and after Finished, missing actor, neutral actor, already-dead contender, disconnect plus normal command in one tick and two disconnects leaving one survivor and finishing the match.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because lifecycle command and Submit overload do not exist.

- [ ] Step 3: Apply lifecycle commands before player commands

```cpp
enum class ContenderExitReason
{
    Disconnected
};

struct MatchLifecycleCommand
{
    ActorId actor = 0U;
    ContenderExitReason reason = ContenderExitReason::Disconnected;
    [[nodiscard]] bool operator==(const MatchLifecycleCommand&) const = default;
};
```

Store lifecycle commands separately. At the beginning of `Impl::Step`, sort by ActorId, deduplicate, validate that each target is a live contender, set health to zero and alive to false and append one ActorDisconnected event. Remove normal queued commands from disconnected actors before existing command selection. Existing last-survivor resolution runs in the same tick.

The game server must abort with `NoConnectedPlayers` before submitting a lifecycle command for the final connected contender. OfflineMatch therefore keeps its invariant that a running match never resolves with zero contenders.

- [ ] Step 4: Run GREEN and all simulation tests

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=OfflineMatch.*:MatchResolution.*:Combat.*
```

Expected: disconnect ordering, idempotence, winner and all existing combat tests pass.

- [ ] Step 5: Commit

```powershell
git add -- simulation/include/dxa/simulation/MatchTypes.hpp simulation/include/dxa/simulation/OfflineMatch.hpp simulation/src/OfflineMatchInternal.hpp simulation/src/OfflineMatch.cpp simulation/src/OfflineMatchStep.cpp tests/simulation_offline_match_test.cpp
git commit -m "feat(simulation): contender disconnect 탈락 추가" -m "이유: game TCP 종료를 다음 권위 tick에서 deterministic 탈락으로 반영할 simulation command가 필요했다." -m "핵심 변경: lifecycle command, ActorDisconnected event와 player command 전 disconnect 처리 순서를 추가했다." -m "검증: type 부재 RED 뒤 next-tick 탈락, duplicate, invalid actor와 last-survivor 테스트를 통과했다."
```

---

### Task 11: Game ticket store와 participant roster

Files:

- Create: `apps/game_server/CMakeLists.txt`
- Create: `apps/game_server/include/dxa/game_server/GameTicketStore.hpp`
- Create: `apps/game_server/src/GameTicketStore.cpp`
- Create: `apps/game_server/include/dxa/game_server/ParticipantRoster.hpp`
- Create: `apps/game_server/src/ParticipantRoster.cpp`
- Create: `tests/game_ticket_store_test.cpp`
- Create: `tests/game_participant_roster_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Task 2 `ReserveMatch`, Task 3 `GameClientHello`, Task 4 UDP token.
- Produces: one-use game-side ticket validation, deterministic PlayerId to EntityId mapping and participant slot gate.

- [ ] Step 1: Write failing ticket and roster tests

```cpp
TEST(GameTicketStore, AcceptsExactTicketOnceAndKeepsMismatchUnconsumed)
{
    GameTicketStore store;
    store.Load(
        MatchId{7U},
        {{PlayerId{2U}, Ticket(1U)}, {PlayerId{9U}, Ticket(2U)}},
        Time(0),
        std::chrono::seconds{60});

    EXPECT_EQ(
        GameTicketConsumeResult::Mismatch,
        store.Consume(Ticket(1U), MatchId{7U}, PlayerId{9U}, Time(1)));
    EXPECT_EQ(
        GameTicketConsumeResult::Accepted,
        store.Consume(Ticket(1U), MatchId{7U}, PlayerId{2U}, Time(1)));
    EXPECT_EQ(
        GameTicketConsumeResult::Used,
        store.Consume(Ticket(1U), MatchId{7U}, PlayerId{2U}, Time(1)));
}

TEST(ParticipantRoster, MapsSortedPlayersToZeroBasedActors)
{
    ParticipantRoster roster{{PlayerId{9U}, PlayerId{2U}, PlayerId{5U}}};
    EXPECT_EQ(EntityId{0U}, roster.ActorFor(PlayerId{2U}));
    EXPECT_EQ(EntityId{1U}, roster.ActorFor(PlayerId{5U}));
    EXPECT_EQ(EntityId{2U}, roster.ActorFor(PlayerId{9U}));
    EXPECT_FALSE(roster.ReadyToStart());
}
```

Cover 60-second exact expiry boundary, purge returning expired PlayerIds, ticket load duplicate rejection, 2 and 24 participant bounds, duplicate PlayerId, authentication transition, duplicate connection, unavailable transition, all slots resolved, authenticated count and optional connection lookup.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: CMake or compilation fails because the game server target and headers do not exist.

- [ ] Step 3: Implement the bounded stores

```cpp
enum class GameTicketConsumeResult
{
    Accepted,
    NotFound,
    Expired,
    Mismatch,
    Used
};

class GameTicketStore
{
public:
    void Load(
        MatchId match,
        std::span<const ReservedParticipant> participants,
        std::chrono::steady_clock::time_point now,
        std::chrono::milliseconds lifetime);
    [[nodiscard]] GameTicketConsumeResult Consume(
        const MatchTicketValue& ticket,
        MatchId match,
        PlayerId player,
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::vector<PlayerId> PurgeExpired(
        std::chrono::steady_clock::time_point now);
};

enum class ParticipantSlotState
{
    Pending,
    Authenticated,
    Unavailable
};

struct GameConnectionId
{
    std::uint64_t value = 0U;
    [[nodiscard]] auto operator<=>(const GameConnectionId&) const = default;
};

struct ParticipantSlot
{
    PlayerId player;
    EntityId actor;
    ParticipantSlotState state = ParticipantSlotState::Pending;
    std::optional<GameConnectionId> connection;
    std::optional<UdpSessionToken> udpToken;
};
```

`GameTicketStore` retains consumed values in a used set until match teardown so tests and metrics distinguish reuse from not found. `PurgeExpired(now)` removes expired records and returns PlayerIds in ascending order. `ParticipantRoster` owns sorted slots and exposes state-changing methods, not its map.
ActorFor and PlayerFor throw `std::out_of_range` when the requested participant is absent; callers validate external IDs before using either lookup.

```cpp
[[nodiscard]] bool Authenticate(
    PlayerId player,
    GameConnectionId connection,
    UdpSessionToken token);
[[nodiscard]] bool MarkUnavailable(PlayerId player);
[[nodiscard]] EntityId ActorFor(PlayerId player) const;
[[nodiscard]] PlayerId PlayerFor(EntityId actor) const;
[[nodiscard]] bool ReadyToStart() const noexcept;
[[nodiscard]] std::size_t AuthenticatedCount() const noexcept;
[[nodiscard]] std::vector<PlayerId> UnavailablePlayers() const;
```

- [ ] Step 4: Run GREEN

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=GameTicketStore.*:ParticipantRoster.*
```

Expected: one-use, expiry, deterministic actor mapping and slot transition tests pass.

- [ ] Step 5: Commit

```powershell
git add -- CMakeLists.txt apps/game_server/CMakeLists.txt apps/game_server/include/dxa/game_server/GameTicketStore.hpp apps/game_server/src/GameTicketStore.cpp apps/game_server/include/dxa/game_server/ParticipantRoster.hpp apps/game_server/src/ParticipantRoster.cpp tests/game_ticket_store_test.cpp tests/game_participant_roster_test.cpp tests/CMakeLists.txt
git commit -m "feat(server): game ticket과 participant roster 추가" -m "이유: worker가 lobby ticket을 실제로 한 번 소비하고 PlayerId를 deterministic ActorId에 연결해야 했다." -m "핵심 변경: mismatch 보존, expiry와 reuse를 구분하는 ticket store와 participant slot start gate를 추가했다." -m "검증: target 부재 RED 뒤 60초 경계, one-use, 2명과 24명, 정렬 actor mapping과 slot 전이 테스트를 통과했다."
```

---

### Task 12: Drift-free fixed tick scheduler

Files:

- Create: `apps/game_server/include/dxa/game_server/FixedTickScheduler.hpp`
- Create: `apps/game_server/src/FixedTickScheduler.cpp`
- Create: `tests/game_fixed_tick_scheduler_test.cpp`
- Modify: `apps/game_server/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: `std::chrono::steady_clock` only.
- Produces: absolute 30Hz deadline, maximum 5 catch-up ticks and explicit overrun result.

- [ ] Step 1: Write failing schedule tests with a manual clock value

```cpp
TEST(FixedTickScheduler, ProducesThirtyTicksAtOneSecondWithoutAccumulatedDrift)
{
    FixedTickScheduler scheduler{30U, 5U};
    scheduler.Start(TimeNs(0));
    std::uint32_t total = 0U;
    for (std::uint32_t frame = 1U; frame <= 30U; ++frame)
    {
        const auto result = scheduler.Advance(
            TimeNs((1000000000ULL * frame) / 30ULL));
        total += result.ticksDue;
    }
    EXPECT_EQ(30U, total);
}

TEST(FixedTickScheduler, CapsCatchUpAndRebasesAfterOverrun)
{
    FixedTickScheduler scheduler{30U, 5U};
    scheduler.Start(TimeNs(0));
    const auto result = scheduler.Advance(TimeNs(1000000000ULL));
    EXPECT_EQ(5U, result.ticksDue);
    EXPECT_TRUE(result.rebased);
    EXPECT_GT(result.lateness, std::chrono::nanoseconds::zero());
    EXPECT_GT(scheduler.NextDeadline(), TimeNs(1000000000ULL));
}
```

Also cover start once, advance before start, early timer zero ticks, exact first deadline, no rebase for two late ticks and invalid zero rate or catch-up limit.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because `FixedTickScheduler.hpp` does not exist.

- [ ] Step 3: Implement deadlines from an epoch and ordinal

```cpp
struct TickAdvanceResult
{
    std::uint32_t ticksDue = 0U;
    bool rebased = false;
    std::chrono::steady_clock::duration lateness{};
};

class FixedTickScheduler
{
public:
    FixedTickScheduler(std::uint32_t tickRate, std::uint32_t maximumCatchUpTicks);
    void Start(std::chrono::steady_clock::time_point now);
    [[nodiscard]] TickAdvanceResult Advance(
        std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::chrono::steady_clock::time_point NextDeadline() const;
};
```

Calculate each deadline as `epoch + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<std::uint64_t, std::ratio<1, 30>>{ordinal})`. Do not add a truncated 33,333,333ns period repeatedly. If more than five deadlines are due, return five ticks, record lateness, set epoch to now and make ordinal 1 the next deadline.

- [ ] Step 4: Run GREEN

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=FixedTickScheduler.*
```

Expected: exact-second count, catch-up and rebase tests pass without sleep.

- [ ] Step 5: Commit

```powershell
git add -- apps/game_server/CMakeLists.txt apps/game_server/include/dxa/game_server/FixedTickScheduler.hpp apps/game_server/src/FixedTickScheduler.cpp tests/game_fixed_tick_scheduler_test.cpp tests/CMakeLists.txt
git commit -m "feat(server): 30Hz fixed tick scheduler 추가" -m "이유: 작업 종료 뒤 sleep하는 방식의 drift와 무제한 catch-up을 피해야 했다." -m "핵심 변경: epoch 기반 deadline, 최대 5 tick catch-up, lateness와 rebase 결과를 추가했다." -m "검증: header 부재 RED 뒤 1초 30 tick, early timer, bounded catch-up과 overrun 테스트를 통과했다."
```

---

### Task 13: Authoritative match core

Files:

- Create: `apps/game_common/include/dxa/game_common/SnapshotAdapter.hpp`
- Create: `apps/game_common/src/SnapshotAdapter.cpp`
- Create: `apps/game_server/include/dxa/game_server/AuthoritativeMatch.hpp`
- Create: `apps/game_server/include/dxa/game_server/UdpTokenSource.hpp`
- Create: `apps/game_server/src/AuthoritativeMatch.cpp`
- Create: `tests/game_authoritative_match_test.cpp`
- Modify: `apps/game_common/CMakeLists.txt`
- Modify: `apps/game_server/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Tasks 2 through 5 protocol, Task 9 canonical arena, Task 10 lifecycle, Task 11 roster and Task 12 scheduler.
- Produces: one match deep module that returns typed TCP, UDP and worker-control actions without owning sockets.

- [ ] Step 1: Write failing reservation, authentication, input and snapshot tests

```cpp
TEST(AuthoritativeMatch, WaitsForEverySlotBeforeStarting)
{
    DeterministicUdpTokenSource tokens;
    AuthoritativeMatch match = CreateMatch(tokens, Time(0));
    const auto first = match.Authenticate(
        GameConnectionId{1U}, HelloFor(PlayerId{1U}), Time(1));
    EXPECT_TRUE(ContainsWelcome(first));
    EXPECT_FALSE(match.Started());

    const auto second = match.Authenticate(
        GameConnectionId{2U}, HelloFor(PlayerId{2U}), Time(1));
    EXPECT_TRUE(ContainsWelcome(second));
    EXPECT_TRUE(match.Started());
    EXPECT_EQ(NetworkMatchPhase::Running, match.Snapshot().phase);
}

TEST(AuthoritativeMatch, AcksInvalidNewInputButKeepsPreviousValidState)
{
    AuthoritativeMatch match = StartedBoundMatch();
    match.ReceiveClientDatagram(PeerA(), ClientInputFor(1U, {0.0F, 0.0F}));
    match.ReceiveClientDatagram(PeerA(), ClientInputFor(2U, {999.0F, 999.0F}));
    const auto output = match.Advance(SecondSnapshotDeadline());
    EXPECT_EQ(2U, AckFor(output, PlayerId{1U}));
    EXPECT_TRUE(MovedTowardOrigin(match.Snapshot(), EntityId{0U}));
}
```

Add tests for reservation participant count and internal bots disabled, sorted actor mapping, exact public authentication error for not found, mismatch, expired and reused ticket, token generation failure, map ID and CRC in welcome, UDP bind idempotence, different-peer rebind, wrong token, old and duplicate input, input sequence max, NaN, off-mesh and no-path destination, missing and self attack target, snapshot every second tick, 1,200-byte fragments, recipient-specific ACK, incomplete recipient skipped before bind, ticket expiry slot start, all expired NoAuthenticatedPlayers, one TCP disconnect lifecycle, all TCP sessions disconnected NoConnectedPlayers and reliable plus control match result.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because AuthoritativeMatch and SnapshotAdapter do not exist.

- [ ] Step 3: Define the event result interface

```cpp
struct UdpPeer
{
    std::array<std::byte, 16U> address{};
    std::uint16_t port = 0U;
    bool ipv6 = false;
    [[nodiscard]] auto operator<=>(const UdpPeer&) const = default;
};

struct GameTcpOutbound
{
    GameConnectionId recipient;
    GameServerMessage message;
    bool closeAfterWrite = false;
};

struct GameUdpOutbound
{
    UdpPeer recipient;
    ServerDatagram datagram;
};

struct AuthoritativeMatchResult
{
    std::vector<GameTcpOutbound> tcp;
    std::vector<GameUdpOutbound> udp;
    std::vector<WorkerToLobbyMessage> control;
    std::vector<GameConnectionId> closeTcp;
    std::uint32_t ticksExecuted = 0U;
    bool overrun = false;
    std::chrono::steady_clock::duration overrunLateness{};
    std::uint64_t totalOverruns = 0U;
};

class IUdpTokenSource
{
public:
    virtual ~IUdpTokenSource() = default;
    [[nodiscard]] virtual bool Fill(
        std::span<std::byte, 16U> output) noexcept = 0;
};

class SecureUdpTokenSource final : public IUdpTokenSource
{
public:
    [[nodiscard]] bool Fill(
        std::span<std::byte, 16U> output) noexcept override;
};
```

Expose only these operations.

```cpp
[[nodiscard]] static AuthoritativeMatch Create(
    const ReserveMatch& reservation,
    const dxa::simulation::ArenaMapDefinition& arena,
    dxa::simulation::MatchConfig config,
    IUdpTokenSource& tokenSource,
    std::chrono::steady_clock::time_point now);

[[nodiscard]] AuthoritativeMatchResult Authenticate(
    GameConnectionId connection,
    const GameClientHello& hello,
    std::chrono::steady_clock::time_point now);
[[nodiscard]] AuthoritativeMatchResult ReceiveClientDatagram(
    UdpPeer peer,
    const ClientDatagram& datagram);
[[nodiscard]] AuthoritativeMatchResult Disconnect(GameConnectionId connection);
[[nodiscard]] AuthoritativeMatchResult Advance(
    std::chrono::steady_clock::time_point now);
[[nodiscard]] std::optional<std::chrono::steady_clock::time_point>
NextDeadline() const;
[[nodiscard]] bool Started() const noexcept;
[[nodiscard]] dxa::protocol::GameSnapshot Snapshot() const;
```

Create OfflineMatch with reserved participant count and internal bots false and call Start during reservation so spawn failure becomes `SimulationInitializationFailed` before worker ready. Do not start FixedTickScheduler or call Step until every slot resolves. Before the first Step, submit Unavailable lifecycle commands. If no participant authenticated, emit `NoAuthenticatedPlayers` without stepping. If all currently authenticated TCP sessions disappear after tick execution starts, emit `NoConnectedPlayers` without asking OfflineMatch to produce a zero-survivor state.

On each server tick, submit only the highest-sequence valid persistent command per actor, Step once and encode a snapshot on even ticks. Input semantic failure advances that session ACK but does not overwrite the last valid persistent command. Build full payload once, then call `FragmentSnapshot` per bound recipient with its ACK.

Before simulation starts, `NextDeadline` returns the earliest pending ticket expiry. After start it returns FixedTickScheduler.NextDeadline. `Advance` purges expired slots before calculating due ticks, so a missing client cannot leave the worker waiting forever.
`Started()` means fixed-tick execution is active, not merely that OfflineMatch has spawned its actors.

`SnapshotAdapter` exhaustively switches every simulation enum to its protocol counterpart and throws on an unknown value. It fills inactive player renderer slots later in the client, not in the wire snapshot.

- [ ] Step 4: Run GREEN and simulation regressions

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=AuthoritativeMatch.*:GameTicketStore.*:ParticipantRoster.*:FixedTickScheduler.*:OfflineMatch.*
```

Expected: start gate, authentication, UDP validation, 30Hz tick, 15Hz fragmentation and completion tests pass.

- [ ] Step 5: Commit

```powershell
git add -- apps/game_common/CMakeLists.txt apps/game_common/include/dxa/game_common/SnapshotAdapter.hpp apps/game_common/src/SnapshotAdapter.cpp apps/game_server/CMakeLists.txt apps/game_server/include/dxa/game_server/UdpTokenSource.hpp apps/game_server/include/dxa/game_server/AuthoritativeMatch.hpp apps/game_server/src/AuthoritativeMatch.cpp tests/game_authoritative_match_test.cpp tests/CMakeLists.txt
git commit -m "feat(server): 권위형 match core 추가" -m "이유: socket과 분리된 한 module에서 ticket, slot gate, input ACK, fixed tick과 snapshot 순서를 검증해야 했다." -m "핵심 변경: typed event result interface, 30Hz OfflineMatch 실행, 15Hz recipient snapshot과 종료 action을 추가했다." -m "검증: module 부재 RED 뒤 인증, bind, invalid input ACK, fragment, disconnect와 completion 테스트를 통과했다."
```

---

### Task 14: Game server Asio adapter와 executable

Files:

- Create: `apps/game_server/include/dxa/game_server/GameServerOptions.hpp`
- Create: `apps/game_server/include/dxa/game_server/GameServer.hpp`
- Create: `apps/game_server/src/GameServerOptions.cpp`
- Create: `apps/game_server/src/SecureUdpTokenSource.cpp`
- Create: `apps/game_server/src/GameServer.cpp`
- Create: `apps/game_server/src/main.cpp`
- Create: `tests/game_server_options_test.cpp`
- Create: `tests/game_server_adapter_test.cpp`
- Modify: `protocol/include/dxa/protocol/AsioFramedConnection.hpp`
- Modify: `protocol/src/AsioFramedConnection.cpp`
- Modify: `tests/protocol_asio_framed_connection_test.cpp`
- Modify: `apps/game_server/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Task 13 AuthoritativeMatch outputs and existing framed TCP transport.
- Produces: worker control client, game TCP and UDP listener, secure UDP token source and `dxa_game_server`.

- [ ] Step 1: Write failing option, flush and adapter smoke tests

```cpp
TEST(GameServerOptions, ParsesLoopbackWorkerAndGamePorts)
{
    const auto parsed = ParseGameServerOptions({
        "--lobby-control-host", "127.0.0.1",
        "--lobby-control-port", "7001",
        "--worker-id", "1",
        "--advertise-host", "127.0.0.1",
        "--game-bind", "127.0.0.1",
        "--game-tcp-port", "7100",
        "--game-udp-port", "7101"});
    ASSERT_TRUE(parsed.options.has_value());
    EXPECT_EQ(WorkerId{1U}, parsed.options->worker);
    EXPECT_EQ(7101U, parsed.options->gameUdpPort);
}

TEST(AsioFramedConnection, CloseAfterFlushDeliversFinalFrameThenCloses)
{
    FramedConnectionPair pair;
    EXPECT_TRUE(pair.server->Send(EncodedMessage{
        MessageType::GameMatchResult, {std::byte{0x2A}}}));
    pair.server->CloseAfterFlush();
    pair.RunUntilClosed();
    ASSERT_EQ(1U, pair.clientFrames.size());
    EXPECT_EQ(std::byte{0x2A}, pair.clientFrames.front().payload.front());
}
```

Add option tests for defaults, zero and 65,536 ports, duplicate option, zero WorkerId, nonnumeric address, invisible advertised host and unknown option. Add an adapter smoke with ephemeral game ports and a fake control listener that receives WorkerRegister and returns WorkerRegistered. Cover game TCP close after five seconds without hello, second hello rejection and a lobby-channel message on the game channel.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because game server option and adapter headers and CloseAfterFlush do not exist.

- [ ] Step 3: Implement the network adapter

```cpp
struct GameServerOptions
{
    std::string lobbyControlHost = "127.0.0.1";
    std::uint16_t lobbyControlPort = 7001U;
    WorkerId worker{1U};
    std::string advertisedHost = "127.0.0.1";
    std::string gameBindAddress = "127.0.0.1";
    std::uint16_t gameTcpPort = 7100U;
    std::uint16_t gameUdpPort = 7101U;
};

struct GameServerConfig
{
    GameServerOptions options;
    dxa::simulation::MatchConfig matchConfig =
        dxa::simulation::DefaultMatchConfig();
    std::chrono::milliseconds authenticationTimeout{5000};
    std::chrono::milliseconds controlReconnectDelay{1000};
};

class GameServer
{
public:
    GameServer(boost::asio::io_context& io, GameServerConfig config);
    void Start();
    void Stop();
    [[nodiscard]] std::uint16_t GameTcpPort() const;
    [[nodiscard]] std::uint16_t GameUdpPort() const;
};
```

GameServer binds game TCP and UDP before opening worker control so registration always advertises ports that are already listening. Production option parsing requires nonzero ports; test configuration may pass port 0 and registration then uses the actual ephemeral local ports. The control client reconnects every one second while Idle, sends WorkerRegister after connect and accepts only lobby-to-worker message types. On control disconnect it closes all game sessions, discards the match and schedules re-registration. Reserve creates AuthoritativeMatch and sends ready or a bounded reject. Cancel destroys only the matching reservation and sends cancel ACK.

When AuthoritativeMatch emits MatchFinished, detach the finished match before writing the control frame and keep its game TCP sessions only in a closing collection until CloseAfterFlush callbacks arrive. This lets the worker accept the next sequential reservation without hosting two active simulations.

Game TCP assigns increasing GameConnectionId and closes an unauthenticated socket after five seconds. Receive UDP into a 1,201-byte buffer so an oversized datagram is observed and rejected rather than accepted after silent truncation. Decode valid-size datagrams before converting the source endpoint to UdpPeer. Every AuthoritativeMatch result is drained in order: control messages, TCP sends, UDP sends, CloseAfterFlush and timer reschedule. One match `steady_timer` always expires at `AuthoritativeMatch::NextDeadline`, which is ticket expiry before start and fixed tick deadline after start.

Log overrun count and lateness from the typed result with MatchId and server tick. Do not silently discard this signal when the scheduler rebases.

`SecureUdpTokenSource` fills exactly 16 bytes using BCryptGenRandom on Windows and getrandom on Linux. It never falls back to `std::random_device`.

- [ ] Step 4: Run GREEN and build both server configurations

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=GameServerOptions.*:GameServerAdapter.*:AsioFramedConnection.*
./scripts/build.ps1 -Preset windows-msvc-release
```

Expected: option, registration, final-frame flush and Debug plus Release builds pass.

- [ ] Step 5: Commit

```powershell
git add -- apps/game_server/CMakeLists.txt apps/game_server/include/dxa/game_server/GameServerOptions.hpp apps/game_server/include/dxa/game_server/GameServer.hpp apps/game_server/src/GameServerOptions.cpp apps/game_server/src/SecureUdpTokenSource.cpp apps/game_server/src/GameServer.cpp apps/game_server/src/main.cpp protocol/include/dxa/protocol/AsioFramedConnection.hpp protocol/src/AsioFramedConnection.cpp tests/game_server_options_test.cpp tests/game_server_adapter_test.cpp tests/protocol_asio_framed_connection_test.cpp tests/CMakeLists.txt
git commit -m "feat(server): game worker network adapter 추가" -m "이유: 권위형 match core를 실제 worker control, game TCP와 UDP socket에 연결해야 했다." -m "핵심 변경: worker registration과 reconnect, authentication timeout, UDP routing, secure token과 final-frame flush를 추가했다." -m "검증: adapter 부재 RED 뒤 option, control registration, CloseAfterFlush, ephemeral listener와 MSVC Release build를 통과했다."
```

---

### Task 15: Lobby부터 game result까지 server loopback integration

Files:

- Create: `tests/support/game_network_fixture.hpp`
- Create: `tests/game_server_integration_test.cpp`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: real LobbyTcpServer, GameServer, LobbyClient, game TCP codec and UDP codec.
- Produces: cross-platform socket evidence for the entire server-side vertical path.

- [ ] Step 1: Write the full failing integration scenario

```cpp
TEST(GameServerIntegration, CompletesLobbyReservationAuthenticationAndDisconnectResult)
{
    GameNetworkFixture fixture{ShortMatchConfig()};
    fixture.StartLobbyAndWorker();
    const ReadyNetworkRoom room = fixture.CreateReadyTwoPlayerRoom();
    fixture.StartMatch(room.host);
    const auto tickets = fixture.WaitForTwoTickets();

    GameClientProbe host = fixture.Authenticate(tickets.host);
    GameClientProbe guest = fixture.Authenticate(tickets.guest);
    host.BindUdp();
    guest.BindUdp();
    host.SendDestination({0.0F, 0.0F}, 1U);
    fixture.WaitForSnapshotAck(host, 1U);
    guest.CloseTcp();

    const GameMatchResult result = fixture.WaitForResult(host);
    EXPECT_TRUE(result.hasWinner);
    EXPECT_EQ(host.actor, result.winner);
    fixture.WaitForRoomCleanup();
}
```

Add separate real-socket tests for invalid ticket, wrong player, reused ticket, ticket expiration with injected short lifetime, bad UDP token, other endpoint rebind, duplicate and old input, worker control close while Reserved and Active, and three sequential reservations proving capacity one without simultaneous matches.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: integration tests fail at the first missing or incorrect live route even though unit tests pass.

- [ ] Step 3: Make only reproduced integration fixes

Implement the fixture as an owner of real server objects and probe clients, not a second implementation of their state machines.

```cpp
class GameNetworkFixture
{
public:
    explicit GameNetworkFixture(dxa::simulation::MatchConfig config);
    void StartLobbyAndWorker();
    [[nodiscard]] ReadyNetworkRoom CreateReadyTwoPlayerRoom();
    void StartMatch(const std::shared_ptr<LobbyClientProbe>& host);
    [[nodiscard]] TicketPair WaitForTwoTickets();
    [[nodiscard]] GameClientProbe Authenticate(const MatchTicket& ticket);
    void WaitForSnapshotAck(GameClientProbe& client, std::uint32_t sequence);
    [[nodiscard]] GameMatchResult WaitForResult(GameClientProbe& client);
    void WaitForRoomCleanup();
};
```

Keep fixture timeouts at five seconds and use ephemeral ports for every listener. `ShortMatchConfig` keeps the production protocol and runner but sets sudden death to tick 30 and hard timeout to tick 60. It does not change production executable defaults.

If a failure requires production changes, first keep the failing focused test, modify only the responsible file, and include those exact files in this commit. Do not weaken assertions or add sleeps; drive io_context until an observable condition or timeout.

- [ ] Step 4: Run GREEN and the complete network subset

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=GameServerIntegration.*:LobbyWorkerControl.*:LobbyTcpIntegration.*:GameServerAdapter.*
```

Expected: live public TCP, control TCP, game TCP and UDP scenarios all pass.

- [ ] Step 5: Commit

```powershell
git add -- tests/support/game_network_fixture.hpp tests/game_server_integration_test.cpp tests/CMakeLists.txt
git commit -m "test(server): lobby부터 game result 경로 고정" -m "이유: 각 network module의 unit test만으로는 실제 socket ordering과 reservation lifecycle을 증명할 수 없었다." -m "핵심 변경: ephemeral public, control, game TCP와 UDP를 잇는 two-client integration fixture와 실패 경계를 추가했다." -m "검증: 첫 live-route RED 뒤 인증, input ACK, disconnect result, worker loss와 순차 reservation 시나리오를 통과했다."
```

If production files were required for a reproduced defect, add only those named files to the commit command and change the subject to `fix(server): <재현된 결과>`.

---

### Task 16: Snapshot reassembly, local prediction and remote interpolation

Files:

- Create: `apps/game_client/CMakeLists.txt`
- Create: `apps/game_client/include/dxa/game_client/SnapshotReassembler.hpp`
- Create: `apps/game_client/src/SnapshotReassembler.cpp`
- Create: `apps/game_client/include/dxa/game_client/ClientPredictor.hpp`
- Create: `apps/game_client/src/ClientPredictor.cpp`
- Create: `apps/game_client/include/dxa/game_client/RemoteInterpolator.hpp`
- Create: `apps/game_client/src/RemoteInterpolator.cpp`
- Create: `tests/game_snapshot_reassembler_test.cpp`
- Create: `tests/game_client_predictor_test.cpp`
- Create: `tests/game_remote_interpolator_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Task 4 fragment, Task 5 GameSnapshot, Task 9 arena NavMesh and NavAgent.
- Produces: complete snapshot envelope, 256-input predictor and 3-tick interpolation buffer.

- [ ] Step 1: Write failing pure client-state tests

```cpp
TEST(SnapshotReassembler, NewerSnapshotDiscardsIncompleteOlderSnapshot)
{
    SnapshotReassembler reassembler;
    const auto older = MakeFragments(10U, PayloadForSnapshot(10U));
    const auto newer = MakeFragments(11U, PayloadForSnapshot(11U));
    EXPECT_FALSE(reassembler.Push(older.front()).has_value());
    std::optional<ReassembledSnapshot> completed;
    for (const auto& fragment : newer)
    {
        completed = reassembler.Push(fragment);
    }
    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(11U, completed->snapshotId);
}

TEST(ClientPredictor, ReconcilesAndReplaysOnlyUnacknowledgedInputs)
{
    ClientPredictor predictor{BuildSurvivalArenaNavMesh(), {10.0F, 0.0F}, 6.0F, 0.1F};
    ASSERT_TRUE(predictor.SetDestination({0.0F, 0.0F}));
    const PredictedInput first = predictor.AdvanceTick();
    const PredictedInput second = predictor.AdvanceTick();
    const Vec2 before = predictor.Position();
    predictor.Reconcile({9.7F, 0.0F}, first.sequence);
    EXPECT_NE(before, predictor.Position());
    EXPECT_EQ(1U, predictor.HistorySize());
    EXPECT_EQ(second.sequence, predictor.LastIssuedSequence());
}

TEST(RemoteInterpolator, SamplesThreeTicksBehindAndNeverExtrapolates)
{
    RemoteInterpolator interpolation{3U, 32U};
    interpolation.Push(SnapshotAt(10U, EntityId{4U}, {0.0F, 0.0F}));
    interpolation.Push(SnapshotAt(14U, EntityId{4U}, {4.0F, 0.0F}));
    EXPECT_EQ((NetworkVec2{1.0F, 0.0F}),
              FindActor(interpolation.Sample(), EntityId{4U}).position);
    interpolation.Push(SnapshotAt(18U, EntityId{4U}, {8.0F, 0.0F}));
    EXPECT_LE(FindActor(interpolation.Sample(), EntityId{4U}).position.x, 8.0F);
}
```

Cover out-of-order and duplicate fragment, metadata mismatch, CRC mismatch, 32-fragment boundary, delivered snapshot replay, ACK equal and below, ACK above issued sequence, history 256 accepted and 257 synchronization failure, non-finite and off-mesh local destination rejection, non-finite authoritative position, late complete snapshot drop, discrete state from newer sample, local actor exclusion, buffer 32 and hold before or beyond available brackets.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: CMake or compilation fails because game client pure modules do not exist.

- [ ] Step 3: Implement the three small interfaces

```cpp
struct ReassembledSnapshot
{
    std::uint32_t snapshotId = 0U;
    std::uint32_t serverTick = 0U;
    std::uint32_t ackInputSequence = 0U;
    dxa::protocol::GameSnapshot snapshot;
};

class SnapshotReassembler
{
public:
    [[nodiscard]] std::optional<ReassembledSnapshot> Push(
        const dxa::protocol::SnapshotFragment& fragment);
    void Reset() noexcept;
};

struct PredictedInput
{
    std::uint32_t sequence = 0U;
    std::optional<dxa::simulation::Vec2> moveDestination;
    std::optional<dxa::simulation::ActorId> attackTarget;
};

class ClientPredictor
{
public:
    ClientPredictor(
        dxa::simulation::NavMesh navMesh,
        dxa::simulation::Vec2 position,
        float speed,
        float stoppingDistance);
    [[nodiscard]] bool SetDestination(dxa::simulation::Vec2 destination);
    [[nodiscard]] PredictedInput AdvanceTick();
    void Reconcile(
        dxa::simulation::Vec2 authoritativePosition,
        std::uint32_t acknowledgedSequence);
    [[nodiscard]] dxa::simulation::Vec2 Position() const noexcept;
    [[nodiscard]] std::size_t HistorySize() const noexcept;
    [[nodiscard]] std::uint32_t LastIssuedSequence() const noexcept;
};

class RemoteInterpolator
{
public:
    RemoteInterpolator(
        std::uint32_t interpolationDelayTicks = 3U,
        std::size_t capacity = dxa::protocol::MaxClientSnapshotBuffer);
    void Push(ReassembledSnapshot snapshot);
    [[nodiscard]] dxa::protocol::GameSnapshot Sample() const;
};
```

`ClientPredictor::AdvanceTick` assigns a nonzero increasing sequence, stores the persistent desired state and ticks NavAgent by exactly `1.0F / 30.0F`. Reconcile reconstructs NavAgent at the authoritative position, erases sequence at or below ACK and replays each remaining history item once. It throws a typed synchronization error on impossible ACK or capacity overflow.

`RemoteInterpolator::Sample` uses newest server tick minus 3, finds bracketing snapshots and linearly interpolates positions. It chooses health, alive, weapon, phase and other discrete values from the newer bracket. Without a newer bracket it holds the last complete state.

- [ ] Step 4: Run GREEN

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=SnapshotReassembler.*:ClientPredictor.*:RemoteInterpolator.*
```

Expected: all reassembly, replay, capacity and interpolation tests pass without sockets.

- [ ] Step 5: Commit

```powershell
git add -- CMakeLists.txt apps/game_client/CMakeLists.txt apps/game_client/include/dxa/game_client/SnapshotReassembler.hpp apps/game_client/src/SnapshotReassembler.cpp apps/game_client/include/dxa/game_client/ClientPredictor.hpp apps/game_client/src/ClientPredictor.cpp apps/game_client/include/dxa/game_client/RemoteInterpolator.hpp apps/game_client/src/RemoteInterpolator.cpp tests/game_snapshot_reassembler_test.cpp tests/game_client_predictor_test.cpp tests/game_remote_interpolator_test.cpp tests/CMakeLists.txt
git commit -m "feat(client): prediction과 snapshot 보정 추가" -m "이유: local actor는 ACK 뒤 input을 재실행하고 remote actor는 불완전 UDP snapshot을 world state로 노출하지 않아야 했다." -m "핵심 변경: 최신 snapshot reassembler, 256-input predictor와 3-tick remote interpolator를 추가했다." -m "검증: module 부재 RED 뒤 newer discard, CRC, impossible ACK, replay, buffer와 hold 테스트를 통과했다."
```

---

### Task 17: Shared game network session

Files:

- Create: `apps/game_client/include/dxa/game_client/GameSession.hpp`
- Create: `apps/game_client/src/GameSession.cpp`
- Create: `tests/game_session_integration_test.cpp`
- Modify: `apps/game_client/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Task 3 game TCP, Task 4 UDP, Task 16 predictor, reassembler and interpolator.
- Produces: app and bot가 쓰는 `GameSession` with internal io_context thread and bounded snapshot queue.

- [ ] Step 1: Write failing fake-server integration tests

```cpp
TEST(GameSession, AuthenticatesBindsPredictsAndPublishesScene)
{
    FakeGameServer server;
    GameSession session{BuildSurvivalArenaNavMesh()};
    session.Start(SessionStartFor(server, PlayerId{3U}));
    server.AcceptHelloAndWelcome(EntityId{0U});
    server.AcceptUdpBind();
    server.SendInitialSnapshot();
    session.FixedUpdate();
    ASSERT_TRUE(session.SetDestination({0.0F, 0.0F}));
    session.FixedUpdate();
    server.SendSnapshotForLastInput();
    session.FixedUpdate();

    const GameSceneFrame scene = session.SampleScene();
    EXPECT_TRUE(scene.connected);
    EXPECT_EQ(EntityId{0U}, scene.localActor);
    EXPECT_GT(scene.lastAckInputSequence, 0U);
}

TEST(GameSession, RejectsMapMismatchBeforeUdpBind)
{
    FakeGameServer server;
    GameSession session{BuildSurvivalArenaNavMesh()};
    session.Start(SessionStartFor(server, PlayerId{3U}));
    server.AcceptHelloAndWelcomeWithMapCrc(0xDEADBEEFU);
    session.WaitForTerminalState();
    EXPECT_EQ(GameSessionState::ProtocolError, session.State());
    EXPECT_EQ(0U, server.UdpBindCount());
}
```

Cover auth failure, hello timeout observed as close, wrong source endpoint, bind retry until accepted, duplicate snapshot, queue capacity 64 retaining newest, GameMatchResult delivery, close during Stop, Stop idempotence, destructor join, ticket and token absent from captured output and impossible ACK terminal state.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because GameSession does not exist.

- [ ] Step 3: Implement the deep app-facing session

```cpp
struct GameSessionStart
{
    dxa::protocol::PlayerId player;
    dxa::protocol::MatchTicket ticket;
    std::uint32_t expectedMapId = 1U;
    std::uint32_t expectedNavMeshCrc32 = 0U;
};

enum class GameSessionState
{
    Idle,
    Connecting,
    Authenticating,
    BindingUdp,
    Synchronizing,
    Running,
    Finished,
    ProtocolError,
    Closed
};

struct GameSceneFrame
{
    bool connected = false;
    dxa::protocol::EntityId localActor;
    dxa::simulation::Vec2 localPosition;
    std::vector<dxa::protocol::NetworkActorSnapshot> actors;
    float zoneRadius = 128.0F;
    std::uint32_t lastAckInputSequence = 0U;
    std::uint64_t snapshotCount = 0U;
};

class GameSession
{
public:
    explicit GameSession(dxa::simulation::NavMesh navMesh);
    ~GameSession();
    void Start(GameSessionStart start);
    [[nodiscard]] bool SetDestination(dxa::simulation::Vec2 destination);
    void FixedUpdate();
    [[nodiscard]] GameSceneFrame SampleScene() const;
    [[nodiscard]] GameSessionState State() const noexcept;
    [[nodiscard]] std::optional<dxa::protocol::GameMatchResult> Result() const;
    [[nodiscard]] std::uint64_t SnapshotCount() const noexcept;
    void Stop();
};
```

GameSession owns one io_context, work guard and thread. The network thread alone owns sockets and SnapshotReassembler. It receives into a 1,201-byte UDP buffer and rejects oversized datagrams before decode. It pushes complete ReassembledSnapshot values into a mutex-protected capacity-64 queue, discarding the oldest queued snapshot when full and incrementing a drop counter. The caller thread alone owns ClientPredictor, RemoteInterpolator and GameSceneFrame.

After BindAccepted, remain Synchronizing until the first complete snapshot contains the local actor. Construct ClientPredictor from that authoritative spawn position, then enter Running. On each later caller `FixedUpdate`, drain queued snapshots in server-tick order, reconcile local state, update remote buffer, create one persistent input and post the encoded send to io_context. Accept server datagrams only from the ticket endpoint. Send UdpBind every 250ms until BindAccepted or Stop. Never include token bytes in an error string.

- [ ] Step 4: Run GREEN and thread-lifetime repetition

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=GameSession.* --gtest_repeat=20 --gtest_break_on_failure
```

Expected: all twenty authentication, bind, snapshot and cleanup repetitions pass without hang.

- [ ] Step 5: Commit

```powershell
git add -- apps/game_client/CMakeLists.txt apps/game_client/include/dxa/game_client/GameSession.hpp apps/game_client/src/GameSession.cpp tests/game_session_integration_test.cpp tests/CMakeLists.txt
git commit -m "feat(client): 공유 game network session 추가" -m "이유: DX11 client와 bot이 같은 game TCP, UDP, reassembly와 prediction lifecycle을 사용해야 했다." -m "핵심 변경: 내부 io thread, ticket 인증, UDP bind, bounded snapshot queue와 app-facing scene session을 추가했다." -m "검증: interface 부재 RED 뒤 fake-server 흐름과 20회 auth, bind, map mismatch, result 및 cleanup 반복을 통과했다."
```

---

### Task 18: Bot play mode

Files:

- Create: `apps/bot_client/include/dxa/bot_client/BotCoordinator.hpp`
- Create: `apps/bot_client/src/BotCoordinator.cpp`
- Modify: `apps/bot_client/include/dxa/bot_client/BotClientOptions.hpp`
- Modify: `apps/bot_client/src/BotClientOptions.cpp`
- Modify: `apps/bot_client/src/main.cpp`
- Modify: `apps/bot_client/CMakeLists.txt`
- Modify: `tests/bot_client_options_test.cpp`
- Modify: `tests/game_server_integration_test.cpp`

Interfaces:

- Consumes: existing LobbyClient and Task 17 GameSession.
- Produces: unchanged lobby-only 1-to-23 mode and `--play` one-client game mode.

- [ ] Step 1: Write failing option and live play tests

```cpp
TEST(BotClientOptions, PlayModeRequiresExactlyOneBot)
{
    const auto valid = Parse({"--room", "7", "--count", "1", "--play"});
    ASSERT_TRUE(valid.options.has_value());
    EXPECT_TRUE(valid.options->play);
    EXPECT_FALSE(Parse({
        "--room", "7", "--count", "2", "--play"}).options.has_value());
}

TEST(GameServerIntegration, PlayBotUsesSharedGameSessionUntilResult)
{
    GameNetworkFixture fixture{ShortMatchConfig()};
    fixture.StartLobbyAndWorker();
    const auto host = fixture.CreateHost();
    BotCoordinator bot{fixture.BotIo(), PlayOptions(host.Room())};
    bot.Start();
    host.ReadyAndStartWhenBotReady();
    fixture.RunUntil([&] { return bot.GameAuthenticated(); });
    fixture.RunUntil([&] { return bot.SnapshotCount() >= 2U; });
    fixture.RunUntil([&] { return bot.Result().has_value(); });
    EXPECT_EQ(0, bot.ExitCode());
}
```

Preserve existing exit codes and lobby-only `bot tickets received` behavior. Add play timeout, lobby error, game auth error, result and shutdown tests. Do not expose a test-only disconnect CLI option.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: option parser rejects `--play` and BotCoordinator is still hidden in main.cpp.

- [ ] Step 3: Extract and extend the coordinator

Add `bool play = false` to options. Parse `--play` as a flag without a value and reject it with count other than one.

Lobby-only mode stops after all tickets as before. Play mode keeps the lobby TCP open, builds Task 9 map and fingerprint, constructs GameSession from its ticket and PlayerId and advances it every 1/30 second with an Asio timer. Schedule the next bot update from the previous absolute deadline rather than callback completion time. Every 90 client ticks it rotates through these persistent destinations:

```cpp
constexpr std::array<dxa::simulation::Vec2, 4U> Destinations{
    dxa::simulation::Vec2{0.0F, 0.0F},
    dxa::simulation::Vec2{20.0F, 20.0F},
    dxa::simulation::Vec2{-20.0F, 20.0F},
    dxa::simulation::Vec2{-20.0F, -20.0F}};
```

Use a 30-second lobby timeout and an 11-minute game timeout. Exit 0 only on GameMatchResult, 3 on protocol or game failure and 4 on timeout. Output public counts and MatchId only, never ticket or UDP token.

Expose only public diagnostic state needed by executable output and integration tests.

```cpp
[[nodiscard]] bool GameAuthenticated() const noexcept;
[[nodiscard]] std::uint64_t SnapshotCount() const noexcept;
[[nodiscard]] std::optional<dxa::protocol::GameMatchResult> Result() const;
[[nodiscard]] int ExitCode() const noexcept;
void Stop();
```

- [ ] Step 4: Run GREEN

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=BotClientOptions.*:GameServerIntegration.PlayBot*
```

Expected: lobby-only boundaries and one-bot game mode pass through the shared session.

- [ ] Step 5: Commit

```powershell
git add -- apps/bot_client/CMakeLists.txt apps/bot_client/include/dxa/bot_client/BotCoordinator.hpp apps/bot_client/src/BotCoordinator.cpp apps/bot_client/include/dxa/bot_client/BotClientOptions.hpp apps/bot_client/src/BotClientOptions.cpp apps/bot_client/src/main.cpp tests/bot_client_options_test.cpp tests/game_server_integration_test.cpp
git commit -m "feat(client): bot game play mode 추가" -m "이유: headless client도 DX11 client와 같은 GameSession으로 ticket 이후 경기 protocol을 수행해야 했다." -m "핵심 변경: coordinator를 추출하고 count 1 play mode, 30Hz destination input, result와 timeout lifecycle을 추가했다." -m "검증: parser RED 뒤 lobby-only 회귀와 실제 lobby 및 game server의 play bot integration을 통과했다."
```

---

### Task 19: Engine runtime scene seam과 ground picking 공유

Files:

- Create: `engine/include/dxa/engine/RuntimeScene.hpp`
- Create: `engine/include/dxa/engine/GroundPlanePicking.hpp`
- Create: `engine/src/windows/GroundPlanePicking.cpp`
- Modify: `engine/include/dxa/engine/EngineApp.hpp`
- Modify: `engine/src/windows/EngineApp.cpp`
- Modify: `engine/CMakeLists.txt`
- Modify: `apps/navigation_demo/src/main.cpp`
- Modify: `apps/navigation_demo/CMakeLists.txt`
- Modify: `tests/navigation_ground_pick_test.cpp`
- Modify: `tests/engine_app_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Delete: `apps/navigation_demo/include/dxa/navigation_demo/GroundPicking.hpp`
- Delete: `apps/navigation_demo/src/GroundPicking.cpp`

Interfaces:

- Consumes: InputState, stress camera, fixed renderer slots and existing HybridDeferredRenderer setters.
- Produces: network-neutral runtime scene controller and reusable ground-plane pick.

- [ ] Step 1: Write failing engine seam tests

```cpp
class ScriptedRuntimeScene final : public IRuntimeSceneController
{
public:
    void FixedUpdate(const RuntimeInputFrame& input) override
    {
        inputs.push_back(input);
    }

    [[nodiscard]] RuntimeSceneFrame SampleScene() override
    {
        return frame;
    }

    RuntimeSceneFrame frame;
    std::vector<RuntimeInputFrame> inputs;
};

TEST(EngineApp, RendersRuntimeSceneThroughWarp)
{
    ScriptedRuntimeScene scene;
    scene.frame.players[0] = SceneCharacterState{
        {0.0F, 0.0F, 0.0F}, true};
    EXPECT_EQ(0, EngineApp{}.Run(
        HiddenHybridOptions(120U),
        ShaderPath(), AssetRoot(), &scene));
}
```

Move existing center and parallel-ray tests to `dxa::engine::PointerGroundDestination`. Add behind-camera, non-finite camera, zero viewport and pointer outside viewport tests. Add EngineApp rejection when a runtime controller is supplied with forward render path and regression when controller is null.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compilation fails because RuntimeScene and engine ground picking do not exist.

- [ ] Step 3: Implement a fixed-size engine interface

```cpp
struct RuntimeInputFrame
{
    std::optional<benchmark::SceneVector3> moveDestination;
};

struct RuntimeSceneFrame
{
    RuntimeSceneFrame() noexcept
    {
        for (SceneCharacterState& state : players)
        {
            state.active = false;
        }
        for (SceneCharacterState& state : ai)
        {
            state.active = false;
        }
    }

    benchmark::SceneVector3 controlledPlayer;
    std::array<SceneCharacterState, benchmark::PlayerCount> players{};
    std::array<SceneCharacterState, benchmark::AiCount> ai{};
    float zoneRadius = 128.0F;
};

class IRuntimeSceneController
{
public:
    virtual ~IRuntimeSceneController() = default;
    virtual void FixedUpdate(const RuntimeInputFrame& input) = 0;
    [[nodiscard]] virtual RuntimeSceneFrame SampleScene() = 0;
};
```

Add a nullable controller parameter to EngineApp::Run. When present, require hybrid deferred assets, accumulate frame delta into 30Hz steps with maximum five calls per render frame, create a destination only on right-button press and apply the returned player, AI and zone state before render. Keep a clicked destination pending until the next fixed update so a high render rate cannot drop it; pass it only to the first due update and pass an empty input to additional catch-up updates. When absent, preserve every current benchmark and smoke branch.

Move the DirectXMath ground-plane implementation into engine. Navigation demo calls the engine function and converts SceneVector3 x and z to simulation Vec2; do not leave a pass-through demo library.

- [ ] Step 4: Run GREEN and WARP regressions

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=NavigationGroundPick.*:EngineApp.*:HybridDeferredRenderer.*
./scripts/test.ps1
```

Expected: runtime WARP test, ground picking and every existing client smoke test pass.

- [ ] Step 5: Commit

```powershell
git add -- engine/CMakeLists.txt engine/include/dxa/engine/RuntimeScene.hpp engine/include/dxa/engine/GroundPlanePicking.hpp engine/src/windows/GroundPlanePicking.cpp engine/include/dxa/engine/EngineApp.hpp engine/src/windows/EngineApp.cpp apps/navigation_demo/CMakeLists.txt apps/navigation_demo/src/main.cpp tests/navigation_ground_pick_test.cpp tests/engine_app_test.cpp tests/CMakeLists.txt
git rm -- apps/navigation_demo/include/dxa/navigation_demo/GroundPicking.hpp apps/navigation_demo/src/GroundPicking.cpp
git commit -m "refactor(engine): runtime scene seam 추가" -m "이유: DX11 loop가 network type을 참조하지 않고 predicted scene과 right-click destination을 주고받아야 했다." -m "핵심 변경: fixed-size runtime controller, 30Hz update, hybrid scene 적용과 shared ground-plane picking을 추가했다." -m "검증: interface 부재 RED 뒤 WARP runtime, pick 경계, renderer와 전체 client smoke 테스트를 통과했다."
```

---

### Task 20: DX11 network host flow

Files:

- Create: `apps/client/include/dxa/client/LobbyHostFlow.hpp`
- Create: `apps/client/src/LobbyHostFlow.cpp`
- Create: `apps/client/include/dxa/client/NetworkClientController.hpp`
- Create: `apps/client/src/NetworkClientController.cpp`
- Create: `tests/client_lobby_host_flow_test.cpp`
- Modify: `apps/client/include/dxa/client/ClientOptions.hpp`
- Modify: `apps/client/src/main.cpp`
- Modify: `apps/client/CMakeLists.txt`
- Modify: `tests/client_options_test.cpp`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: LobbyClient, Task 17 GameSession and Task 19 IRuntimeSceneController.
- Produces: `--network-create` actual DX11 host mode with automatic create, ready and start.

- [ ] Step 1: Write failing parser and pure lobby-flow tests

```cpp
TEST(ClientOptions, ParsesNetworkCreateAndRequiresHybridPath)
{
    const auto valid = Parse({
        "--render-path", "hybrid-deferred",
        "--network-create",
        "--expected-players", "2",
        "--lobby-host", "127.0.0.1",
        "--lobby-port", "7000"});
    ASSERT_TRUE(valid.options.has_value());
    EXPECT_TRUE(valid.options->network.has_value());
    EXPECT_EQ(2U, valid.options->network->expectedPlayers);
    EXPECT_FALSE(Parse({"--network-create"}).options.has_value());
}

TEST(LobbyHostFlow, StartsExactlyOnceWhenExpectedReadyPlayersArrive)
{
    LobbyHostFlow flow{2U};
    EXPECT_EQ(HostCommand::CreateRoom, flow.OnWelcome(PlayerId{4U}));
    EXPECT_EQ(HostCommand::SetReady, flow.OnRoomSnapshot(
        WaitingRoom(PlayerId{4U}, {{PlayerId{4U}, false}})));
    EXPECT_EQ(HostCommand::None, flow.OnRoomSnapshot(
        WaitingRoom(PlayerId{4U}, {{PlayerId{4U}, true}})));
    EXPECT_EQ(HostCommand::StartMatch, flow.OnRoomSnapshot(
        WaitingRoom(
            PlayerId{4U},
            {{PlayerId{4U}, true}, {PlayerId{8U}, true}})));
    EXPECT_EQ(HostCommand::None, flow.OnRoomSnapshot(
        WaitingRoom(
            PlayerId{4U},
            {{PlayerId{4U}, true}, {PlayerId{8U}, true}})));
}
```

Cover expected players 2 and 24, 1 and 25 rejected, benchmark incompatibility, duplicate network option, non-host snapshot, too many members, unready guest, Starting and InMatch no duplicate command, lobby error terminal state and ticket before welcome rejection.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: parser rejects network arguments and host flow headers do not exist.

- [ ] Step 3: Compose lobby and game without putting network in engine

```cpp
struct NetworkClientOptions
{
    std::string lobbyHost = "127.0.0.1";
    std::uint16_t lobbyPort = 7000U;
    std::uint8_t expectedPlayers = 2U;
};

class NetworkClientController final : public dxa::engine::IRuntimeSceneController
{
public:
    explicit NetworkClientController(NetworkClientOptions options);
    ~NetworkClientController() override;
    void Start();
    void FixedUpdate(const dxa::engine::RuntimeInputFrame& input) override;
    [[nodiscard]] dxa::engine::RuntimeSceneFrame SampleScene() override;
    [[nodiscard]] std::optional<dxa::protocol::RoomId> Room() const;
    [[nodiscard]] std::optional<dxa::protocol::GameMatchResult> Result() const;
    [[nodiscard]] std::uint64_t SnapshotCount() const noexcept;
    void Stop();
};
```

The controller owns a lobby io_context thread. At construction it builds Task 9 map 1 once, computes its fingerprint and keeps both for GameSession. It passes each typed lobby message through LobbyHostFlow, prints only room ID and public state, and starts GameSession after receiving MatchTicket. FixedUpdate converts engine x and z to simulation Vec2, advances GameSession and maps its wire scene into 24 player and 100 AI renderer slots. Missing contender slots are inactive. The local actor uses predictor position; every other actor uses interpolation output.

Client main constructs this controller only for network mode, starts it before EngineApp, passes its address to Run and stops it on every normal and exceptional exit. Non-network mode executes the current path byte-for-byte apart from the added null argument.

- [ ] Step 4: Run GREEN and existing client option suite

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=ClientOptions.*:LobbyHostFlow.*:EngineApp.*
```

Expected: host state machine, network option and existing benchmark and smoke options pass.

- [ ] Step 5: Commit

```powershell
git add -- apps/client/CMakeLists.txt apps/client/include/dxa/client/LobbyHostFlow.hpp apps/client/src/LobbyHostFlow.cpp apps/client/include/dxa/client/NetworkClientController.hpp apps/client/src/NetworkClientController.cpp apps/client/include/dxa/client/ClientOptions.hpp apps/client/src/main.cpp tests/client_lobby_host_flow_test.cpp tests/client_options_test.cpp tests/CMakeLists.txt
git commit -m "feat(client): DX11 network host flow 추가" -m "이유: 실제 DX11 client가 room 생성부터 game session scene까지 자동으로 이어져야 했다." -m "핵심 변경: network option, one-shot lobby host state machine과 runtime scene controller 조합을 추가했다." -m "검증: option과 flow 부재 RED 뒤 2명과 24명 경계, duplicate start, error state와 EngineApp 회귀 테스트를 통과했다."
```

---

### Task 21: Actual DX11 plus headless vertical scenario, records and PR

Files:

- Create: `tests/network_vertical_slice_test.cpp`
- Create: `docs/adr/0007-authoritative-game-session.md`
- Create: `docs/devlog/2026-08-24-authoritative-game-server.md`
- Modify: `tests/CMakeLists.txt`
- Modify: `apps/client/CMakeLists.txt`
- Modify: `README.md`
- Modify: `docs/PROJECT_PLAN.md`

Interfaces:

- Consumes: final lobby server, game server, NetworkClientController, EngineApp and BotCoordinator.
- Produces: Windows WARP vertical evidence, reproducible manual commands, Week 9 ADR and PR description.

- [ ] Step 1: Write the failing Windows vertical test

```cpp
TEST(NetworkVerticalSlice, WarpClientAndHeadlessBotShareOneMatch)
{
    VerticalFixture fixture{ShortMatchConfig()};
    fixture.StartServers();
    NetworkClientController host{fixture.HostOptions(2U)};
    host.Start();
    fixture.WaitForRoom(host);
    BotCoordinator bot{fixture.BotIo(), fixture.PlayBotOptions(*host.Room())};
    bot.Start();

    EXPECT_EQ(0, EngineApp{}.Run(
        fixture.HiddenWarpHybridOptions(600U),
        fixture.ShaderPath(),
        fixture.AssetRoot(),
        &host));
    fixture.WaitForResults();

    ASSERT_TRUE(host.Result().has_value());
    ASSERT_TRUE(bot.Result().has_value());
    EXPECT_EQ(bot.Result()->match, host.Result()->match);
    EXPECT_GE(host.SnapshotCount(), 2U);
    EXPECT_GE(bot.SnapshotCount(), 2U);
    EXPECT_EQ(0U, fixture.SecretLeakCount());
}
```

Register this source only inside the existing `if(WIN32)` test block. The fixture uses ephemeral ports and injected sudden death tick 30 and hard timeout tick 60. `HiddenWarpHybridOptions` keeps vsync enabled, limits the run to 600 frames and installs a 15-second test watchdog. It runs the actual WARP EngineApp with NetworkClientController and the actual BotCoordinator. It must assert both clients authenticated to the same MatchId, received snapshots, observed a result, and emitted no ticket or token bytes to captured output.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=NetworkVerticalSlice.*
```

Expected: the first end-to-end timing, lifecycle or scene integration defect fails with a focused assertion rather than a timeout-only pass.

- [ ] Step 3: Fix only reproduced vertical defects

Implement the Windows fixture in the same test file with production objects and a bounded watchdog.

```cpp
class VerticalFixture
{
public:
    explicit VerticalFixture(dxa::simulation::MatchConfig matchConfig);
    void StartServers();
    [[nodiscard]] NetworkClientOptions HostOptions(
        std::uint8_t expectedPlayers) const;
    void WaitForRoom(NetworkClientController& host);
    [[nodiscard]] boost::asio::io_context& BotIo() noexcept;
    [[nodiscard]] BotClientOptions PlayBotOptions(RoomId room) const;
    [[nodiscard]] EngineRunOptions HiddenWarpHybridOptions(
        std::uint32_t maximumFrames) const;
    void WaitForResults();
    [[nodiscard]] std::size_t SecretLeakCount() const noexcept;
    [[nodiscard]] std::filesystem::path ShaderPath() const;
    [[nodiscard]] std::filesystem::path AssetRoot() const;
};
```

Start the bot io_context on a joined thread, stop every client and server in reverse ownership order and fail the test if the 15-second watchdog fires. Capture the test log sink with deterministic ticket and token sources and count exact secret byte encodings without printing them.

Keep the test configuration injected through constructors. Do not add production CLI shortcuts for short matches, forced disconnect or known room ID. For every defect, retain the failing assertion, change the smallest owning module and rerun this single test before the full suite.

- [ ] Step 4: Run final local verification

Run:

```powershell
./scripts/build.ps1
./scripts/test.ps1
./scripts/build.ps1 -Preset windows-msvc-release
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe --gtest_filter=GameServerIntegration.*:GameSession.*:NetworkVerticalSlice.* --gtest_repeat=3 --gtest_break_on_failure
```

Then run the four real executables with the commands in the spec. Use a fresh lobby process, record the actual room ID, and verify that the hardware DX11 client on the available RTX 3050 Ti and one `--play` bot report the same MatchId. Visually confirm local prediction, remote motion, zone display and result without a DX11 debug-layer error. Close the bot game process after both clients receive snapshots and confirm the DX11 client receives the last-survivor result. Record only public IDs, counts, elapsed time and exit codes.

Expected: Debug build, full CTest including WARP smoke, Release build, three repeated network slices and the manual four-process scenario pass. If any gate fails, record the failure and keep Week 9 incomplete.

- [ ] Step 5: Commit the verified vertical gate

```powershell
git add -- tests/network_vertical_slice_test.cpp tests/CMakeLists.txt apps/client/CMakeLists.txt
git commit -m "test(network): DX11과 headless 수직 경로 고정" -m "이유: headless-only integration으로는 실제 WARP render loop와 network controller가 한 match를 공유하는지 증명할 수 없었다." -m "핵심 변경: ephemeral lobby와 worker, WARP client, play bot을 잇는 bounded vertical test와 secret redaction 검증을 추가했다." -m "검증: end-to-end RED 뒤 두 client의 같은 MatchId, snapshot 수신, result와 15초 watchdog을 통과했다."
```

- [ ] Step 6: Review the full Week 9 diff locally

Review base:

```text
69993bf4d0f93920f385e178f4a9cff51e5590db
```

Inspect protocol allocation bounds, channel direction, ticket and token secrecy, timer cancellation, late callback ownership, reservation race, disconnect rules, UDP endpoint validation, input sequence ACK, fragment memory bounds, thread shutdown, Windows and Linux compile guards and WARP false-pass paths. This review remains in the current session without subagents.

For every reproduced finding, add a focused failing test, apply one fix and commit it separately with a `fix(review):` subject and the required Korean body. Do not create a finding or commit when no failure is reproduced.

- [ ] Step 7: Write records from actual evidence

ADR 0007 records why worker control, game TCP and UDP are separate, why reservation is two-phase, why worker capacity is one, why input is persistent state and why client holds remote actors instead of extrapolating.

Devlog uses this order: symptom, reproduction, hypothesis, alternatives, implementation, result, limitation. Include only actual test counts, observed failures, actual room and MatchId, snapshot counts, elapsed time and local hardware verification. State that full-state bandwidth and 24-player load remain Week 10.

README adds the four-process command sequence and explains loopback defaults and plaintext transport limitation. Project plan marks Week 9 complete only after all gates pass and changes the next milestone to Week 10.

- [ ] Step 8: Commit Week 9 records

```powershell
git add -- README.md docs/PROJECT_PLAN.md docs/adr/0007-authoritative-game-session.md docs/devlog/2026-08-24-authoritative-game-server.md
git commit -m "docs(network): 9주차 권위형 경기 연결 기록" -m "이유: lobby reservation부터 실제 DX11 client와 headless client의 game result까지 재현 가능한 evidence를 남겨야 했다." -m "핵심 변경: WARP vertical gate, ADR, 실행 명령, 개발 기록과 프로젝트 진행 상태를 실제 결과에 맞춰 추가했다." -m "검증: 전체 Windows Debug와 Release, 반복 network slice, 수동 네 process 결과, secret redaction과 문서 명령을 확인했다."
```

- [ ] Step 9: Push and open the Week 9 PR

Push `feat/authoritative-game-server` and open a PR against `main`. Fill every section of `.github/PULL_REQUEST_TEMPLATE.md`. Separate completed behavior, local measured evidence, Windows and Ubuntu CI facts and Week 10 deferred work. Monitor all GitHub checks until green. Apply reproduced CI fixes as separate `fix(ci):` commits with focused reruns. Do not merge without a new direct user instruction.

---

## Expected Commit Sequence

1. `feat(protocol): game network 기본 계약 추가`
2. `feat(protocol): worker control message 추가`
3. `feat(protocol): game TCP 인증 계약 추가`
4. `feat(protocol): 1200바이트 UDP frame 추가`
5. `feat(protocol): full-state snapshot codec 추가`
6. `refactor(lobby): 비동기 match reservation 전환`
7. `feat(lobby): worker registry state machine 추가`
8. `feat(lobby): worker control TCP 연결`
9. `refactor(simulation): canonical arena map 공유`
10. `feat(simulation): contender disconnect 탈락 추가`
11. `feat(server): game ticket과 participant roster 추가`
12. `feat(server): 30Hz fixed tick scheduler 추가`
13. `feat(server): 권위형 match core 추가`
14. `feat(server): game worker network adapter 추가`
15. `test(server): lobby부터 game result 경로 고정`
16. `feat(client): prediction과 snapshot 보정 추가`
17. `feat(client): 공유 game network session 추가`
18. `feat(client): bot game play mode 추가`
19. `refactor(engine): runtime scene seam 추가`
20. `feat(client): DX11 network host flow 추가`
21. `test(network): DX11과 headless 수직 경로 고정`
22. `docs(network): 9주차 권위형 경기 연결 기록`

Review and CI fix commits are created only for reproduced failures. Commit count is not a target, commit dates are not changed and synthetic incident or metric is not added.

## Execution Checkpoints

- Checkpoint A after Task 5: all protocol contracts compile and pass before domain work starts.
- Checkpoint B after Task 8: actual lobby public and worker control TCP reservation completes before game simulation work starts.
- Checkpoint C after Task 15: headless server-side vertical path completes before client prediction work starts.
- Checkpoint D after Task 20: DX11 host flow is code-complete before final WARP and manual evidence.
- Checkpoint E after Task 21: local review, CI and PR are merge-ready; merge still requires user instruction.

## Spec Coverage Check

- Directory and target separation: File Structure, Tasks 8, 11, 14, 16 and 20
- Fixed constants, IDs and message values: Tasks 1 through 5
- Worker registration, capacity and lifecycle: Tasks 7, 8 and 14
- Two-phase start, rollback and disconnect rules: Tasks 6, 7, 8 and 15
- One-use ticket, UDP token and start gate: Tasks 11, 13 and 14
- Canonical NavMesh and CRC mismatch detection: Tasks 9, 13 and 17
- 30Hz simulation, five-tick catch-up and 15Hz snapshot: Tasks 12 and 13
- UDP bind, input validation, ACK and 1,200-byte fragmentation: Tasks 4, 13 and 15
- Newer-snapshot replacement and CRC reassembly: Tasks 4, 5 and 16
- Local prediction, replay and remote interpolation: Tasks 16 and 17
- DX11 right-click scene composition: Tasks 19 and 20
- Actual DX11 client plus one headless client: Task 21
- Secret redaction, loopback defaults and plaintext limitation: Global Constraints, Tasks 14, 17, 18 and 21
- Week 10 exclusions and truthful evidence boundary: Global Constraints and Task 21

No spec requirement is left without an implementation or verification task. Full 24-player load, bandwidth optimization, impairment injection, reconnect, cloud packaging and multi-match workers remain outside this plan.
