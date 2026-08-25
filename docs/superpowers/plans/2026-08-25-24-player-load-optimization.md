# 10주차 24인 부하와 snapshot 최적화 구현 계획

> For agentic workers: REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox syntax for tracking. This milestone stays in the current session and does not use subagents unless the user explicitly requests them.

Goal: 실제 DX11 client 1개와 shared runtime의 bot session 23개가 production 세 경기를 연속 완주하고, full-state 기준선 대비 ACK 기반 관심 영역 delta snapshot의 server 비용과 client 수신량을 재현 가능한 evidence로 비교한다.

Architecture: protocol v2가 replication mode와 SnapshotId ACK를 고정한다. `SnapshotReplicator`가 server의 recipient별 관심 영역, 양자화, baseline ring과 delta를 숨기고 `ClientSnapshotStream`이 client 적용과 복구를 숨긴다. `BotCoordinator`는 `GameNetworkRuntime` 하나를 공유하는 game session 23개를 실행하며 load runner가 동일 seed의 기준선, 단계별 최적화, impairment와 soak를 수집한다.

Tech Stack: C++20, CMake, Boost.Asio, DirectX 11 WARP와 hardware, GoogleTest, PowerShell, Docker Ubuntu 24.04 GCC 13, AddressSanitizer

Spec: `docs/superpowers/specs/2026-08-25-24-player-load-optimization-design.md`

## Global Constraints

- branch는 `feat/24-player-load-optimization`을 사용한다.
- protocol integer와 float bit는 explicit little-endian codec만 사용한다.
- ProtocolVersion은 2로 올리고 version 1 binary와 혼용하지 않는다.
- TCP frame은 64KiB 이하, UDP datagram은 1,200바이트 이하, snapshot은 최대 32 fragment와 37,056 payload bytes를 유지한다.
- game server는 30Hz fixed tick, snapshot은 15Hz를 유지한다.
- participant는 24명, 중립 AI는 100개, actor 최대치는 124개다.
- bot executable 하나가 shared game network runtime에서 session 23개를 실행한다.
- recipient baseline과 client baseline은 각각 최대 32개다.
- keyframe interval은 snapshot 30개, 2초다.
- interest grid cell은 32m, enter radius는 80m, leave radius는 88m다.
- impairment는 UDP 양방향 50ms 편도 지연, 2% 손실, 10ms jitter, seed 20260825를 사용한다.
- ticket, UDP token과 packet raw bytes는 log, assertion과 evidence에 남기지 않는다.
- 공식 benchmark는 clean Release commit에서만 실행하고 원본 directory를 덮어쓰지 않는다.
- 목표는 server tick P95 33.3ms 이하와 client 평균 game 수신량 64KiB/s 이하다.
- 목표 미달 수치와 병목을 숨기지 않는다.
- remote hosted CI가 billing gate로 시작되지 않으면 Windows local과 Docker Linux 증거를 분리해 기록한다.
- commit 제목은 한국어 명사형 Conventional Commit을 사용하고 본문에 `이유`, `핵심 변경`, `검증`을 기록한다.

---

## File Structure

```text
protocol/
  include/dxa/protocol/DatagramShaper.hpp
  include/dxa/protocol/ReplicationSnapshot.hpp
  include/dxa/protocol/ReplicationSnapshotCodec.hpp
  src/DatagramShaper.cpp
  src/ReplicationSnapshotCodec.cpp

apps/game_common/
  include/dxa/game_common/NetworkMetrics.hpp
  src/NetworkMetrics.cpp

apps/game_client/
  include/dxa/game_client/GameNetworkRuntime.hpp
  include/dxa/game_client/ClientSnapshotStream.hpp
  src/GameNetworkRuntime.cpp
  src/ClientSnapshotStream.cpp

apps/game_server/
  include/dxa/game_server/InterestGrid.hpp
  include/dxa/game_server/SnapshotReplicator.hpp
  include/dxa/game_server/ServerMatchMetrics.hpp
  src/InterestGrid.cpp
  src/SnapshotReplicator.cpp
  src/ServerMatchMetrics.cpp

tests/
  protocol_replication_snapshot_codec_test.cpp
  game_network_runtime_test.cpp
  game_client_snapshot_stream_test.cpp
  game_interest_grid_test.cpp
  game_snapshot_replicator_test.cpp
  game_datagram_shaper_test.cpp
  network_load_vertical_test.cpp

scripts/
  network_load_common.ps1
  run_network_load.ps1

docs/benchmarks/network-load/
docs/adr/0008-acked-interest-replication.md
docs/devlog/2026-08-25-24-player-network-load.md
```

Existing files change only where listed in each task. Do not create a generic ECS, network framework, compression layer or reconnect abstraction.

---

### Task 1: Protocol v2 replication mode와 SnapshotId ACK

Files:

- Modify: `protocol/include/dxa/protocol/LobbyTypes.hpp`
- Modify: `protocol/include/dxa/protocol/GameTypes.hpp`
- Modify: `protocol/include/dxa/protocol/GameTcpMessages.hpp`
- Modify: `protocol/include/dxa/protocol/GameUdpMessages.hpp`
- Modify: `protocol/src/GameTcpMessageCodec.cpp`
- Modify: `protocol/src/GameUdpCodec.cpp`
- Modify: `tests/protocol_ids_test.cpp`
- Modify: `tests/protocol_game_types_test.cpp`
- Modify: `tests/protocol_game_tcp_codec_test.cpp`
- Modify: `tests/protocol_game_udp_codec_test.cpp`
- Modify: every focused test fixture that aggregate-initializes `GameServerWelcome` or `ClientInput`

Interfaces:

- Consumes: current `ProtocolVersion`, `GameServerWelcome` and `ClientInput` codecs.
- Produces: `ReplicationMode`, ProtocolVersion 2, welcome negotiation, monotonic SnapshotId ACK and keyframe request fields used by every later task.

- [ ] Step 1: Write the failing protocol contract tests

Add the following assertions before changing production headers.

```cpp
TEST(ProtocolIds, UsesReplicationProtocolVersionTwo)
{
    EXPECT_EQ(2U, dxa::protocol::ProtocolVersion);
}

TEST(GameUdpCodec, RoundTripsSnapshotAckAndKeyframeRequest)
{
    dxa::protocol::ClientInput input;
    input.match = dxa::protocol::MatchId{7U};
    input.player = dxa::protocol::PlayerId{3U};
    input.inputSequence = 11U;
    input.acknowledgedSnapshotId = 9U;
    input.requestKeyframe = true;

    const auto encoded = dxa::protocol::EncodeClientDatagram(input);
    const auto decoded = dxa::protocol::DecodeClientDatagram(encoded.bytes);
    ASSERT_TRUE(decoded.message.has_value());
    EXPECT_EQ(input, std::get<dxa::protocol::ClientInput>(*decoded.message));
}

TEST(GameTcpCodec, RoundTripsReplicationMode)
{
    dxa::protocol::GameServerWelcome welcome;
    welcome.match = dxa::protocol::MatchId{4U};
    welcome.replicationMode = dxa::protocol::ReplicationMode::InterestDelta;
    ExpectServerRoundTrip(dxa::protocol::GameServerMessage{welcome});
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=ProtocolIds.*:GameTypes.*:GameTcpCodec.*:GameUdpCodec.*
```

Expected: compile failure because `ReplicationMode`, `acknowledgedSnapshotId` and `requestKeyframe` do not exist, plus the ProtocolVersion assertion remains 1.

- [ ] Step 3: Add the minimal protocol values

Add to `GameTypes.hpp`:

```cpp
enum class ReplicationMode : std::uint8_t
{
    FullState = 1,
    InterestFullPrecision = 2,
    InterestQuantized = 3,
    InterestDelta = 4
};
```

Change `ProtocolVersion` to 2. Add `ReplicationMode replicationMode = ReplicationMode::FullState` to `GameServerWelcome`. Add the following fields after `inputSequence` in `ClientInput`:

```cpp
std::uint32_t acknowledgedSnapshotId = 0U;
bool requestKeyframe = false;
```

Encode and decode the new values with explicit bounds. Reject unknown replication enum values, SnapshotId ACK above `UINT32_MAX` by construction, invalid bool bytes and trailing bytes. Update every aggregate initializer explicitly rather than relying on omitted fields under GCC warnings-as-errors.

- [ ] Step 4: Run GREEN and broader protocol tests

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=ProtocolIds.*:GameTypes.*:GameTcpCodec.*:GameUdpCodec.*:TcpFrame.*:LobbyMessageCodec.*
```

Expected: all selected tests pass and exact byte fixtures reflect version 2.

- [ ] Step 5: Commit

```powershell
git add -- protocol/include/dxa/protocol/LobbyTypes.hpp protocol/include/dxa/protocol/GameTypes.hpp protocol/include/dxa/protocol/GameTcpMessages.hpp protocol/include/dxa/protocol/GameUdpMessages.hpp protocol/src/GameTcpMessageCodec.cpp protocol/src/GameUdpCodec.cpp tests
git commit -m "feat(protocol): snapshot ACK와 replication mode 추가" -m "이유: 손실에 안전한 delta baseline과 full-state 비교 mode를 wire에서 협상해야 했다." -m "핵심 변경: protocol v2, replication enum, SnapshotId ACK와 keyframe request codec을 추가했다." -m "검증: protocol RED 뒤 TCP, UDP, frame과 version focused test를 통과했다."
```

---

### Task 2: Shared game network runtime

Files:

- Create: `apps/game_client/include/dxa/game_client/GameNetworkRuntime.hpp`
- Create: `apps/game_client/src/GameNetworkRuntime.cpp`
- Modify: `apps/game_client/include/dxa/game_client/GameSession.hpp`
- Modify: `apps/game_client/src/GameSession.cpp`
- Modify: `apps/game_client/CMakeLists.txt`
- Create: `tests/game_network_runtime_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/game_session_integration_test.cpp`

Interfaces:

- Consumes: current `GameSession` internal `io_context`, work guard, thread and `Stop()` lifecycle.
- Produces: one `GameNetworkRuntime` shared by 23 sessions while default DX11 construction keeps an owned runtime.

- [ ] Step 1: Write the failing shared-runtime tests

```cpp
TEST(GameNetworkRuntime, StartsAndStopsIdempotently)
{
    auto runtime = std::make_shared<dxa::game_client::GameNetworkRuntime>();
    EXPECT_TRUE(runtime->Start());
    EXPECT_FALSE(runtime->Start());
    runtime->Stop();
    runtime->Stop();
}

TEST(GameSession, TwoSessionsShareOneRuntimeAndBothSynchronize)
{
    FakeGameServer first;
    FakeGameServer second;
    auto runtime = std::make_shared<dxa::game_client::GameNetworkRuntime>();
    ASSERT_TRUE(runtime->Start());

    dxa::game_client::GameSession a{ArenaNavMesh(), runtime};
    dxa::game_client::GameSession b{ArenaNavMesh(), runtime};
    a.Start(first.StartValues(dxa::protocol::PlayerId{1U}));
    b.Start(second.StartValues(dxa::protocol::PlayerId{2U}));

    first.CompleteHandshakeAndSnapshot();
    second.CompleteHandshakeAndSnapshot();
    WaitUntil([&] {
        a.FixedUpdate();
        b.FixedUpdate();
        return a.SnapshotCount() >= 1U && b.SnapshotCount() >= 1U;
    });
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compile failure because `GameNetworkRuntime` and the shared constructor do not exist.

- [ ] Step 3: Extract the runtime without changing session behavior

Use this interface:

```cpp
class GameNetworkRuntime
{
public:
    GameNetworkRuntime();
    ~GameNetworkRuntime();
    GameNetworkRuntime(const GameNetworkRuntime&) = delete;
    GameNetworkRuntime& operator=(const GameNetworkRuntime&) = delete;

    [[nodiscard]] bool Start();
    void Stop();

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
    friend class GameSession;
};
```

Add this constructor while keeping the existing one:

```cpp
GameSession(
    dxa::simulation::NavMesh navMesh,
    std::shared_ptr<GameNetworkRuntime> runtime);
```

The existing constructor creates and starts an owned runtime. The shared constructor requires a started runtime and never stops it from a session destructor. `GameSession::Stop()` cancels only its sockets, timer and callbacks. `GameNetworkRuntime::Stop()` posts shutdown, releases the work guard and joins once.

- [ ] Step 4: Run session tests repeatedly

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=GameNetworkRuntime.*:GameSession.* `
  --gtest_repeat=20 `
  --gtest_break_on_failure
```

Expected: no thread leak, deadlock, late callback or behavior change in the existing session tests.

- [ ] Step 5: Commit

```powershell
git add -- apps/game_client tests/game_network_runtime_test.cpp tests/game_session_integration_test.cpp tests/CMakeLists.txt
git commit -m "refactor(client): shared game network runtime 추가" -m "이유: play bot 23개가 session마다 network thread를 만들지 않고 한 runtime을 공유해야 했다." -m "핵심 변경: GameSession의 Asio lifetime을 GameNetworkRuntime으로 추출하고 owned와 shared 생성 경로를 제공했다." -m "검증: shared session RED 뒤 runtime과 GameSession test를 20회 반복 통과했다."
```

---

### Task 3: Play bot 23개와 per-session report

Files:

- Modify: `apps/bot_client/include/dxa/bot_client/BotClientOptions.hpp`
- Modify: `apps/bot_client/include/dxa/bot_client/BotCoordinator.hpp`
- Modify: `apps/bot_client/src/BotClientOptions.cpp`
- Modify: `apps/bot_client/src/BotCoordinator.cpp`
- Modify: `apps/bot_client/src/main.cpp`
- Modify: `tests/bot_client_options_test.cpp`
- Modify: `tests/game_server_integration_test.cpp`

Interfaces:

- Consumes: lobby bot vector, ticket callback, `GameSession`, shared `GameNetworkRuntime`.
- Produces: 1부터 23 play session, deterministic input and complete `BotCoordinatorReport`.

- [ ] Step 1: Write failing option and report tests

```cpp
TEST(BotClientOptions, PlayModeAcceptsTwentyThreeBots)
{
    const auto parsed = Parse({
        "--room", "7", "--count", "23", "--play"});
    ASSERT_TRUE(parsed.options.has_value());
    EXPECT_EQ(23U, parsed.options->count);
    EXPECT_TRUE(parsed.options->play);
}

TEST(GameServerIntegration, PlayCoordinatorReportsEverySession)
{
    GameNetworkFixture fixture{ShortTwentyFourPlayerConfig()};
    const auto room = fixture.CreateRoomWithHost(24U);
    dxa::bot_client::BotClientOptions options;
    options.room = room;
    options.count = 23U;
    options.play = true;

    dxa::bot_client::BotCoordinator bots{fixture.BotIo(), options};
    bots.Start();
    fixture.StartWhenReady();
    fixture.RunUntil([&] { return bots.Done(); });

    const auto report = bots.Report();
    ASSERT_EQ(23U, report.sessions.size());
    EXPECT_TRUE(std::ranges::all_of(report.sessions, [](const auto& session) {
        return session.exitCode == 0 && session.snapshotsApplied >= 2U;
    }));
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=BotClientOptions.*:GameServerIntegration.PlayCoordinatorReportsEverySession
```

Expected: parser rejects count 23 in play mode and `Report()` does not exist.

- [ ] Step 3: Add report values and one shared runtime

Add:

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

struct BotCoordinatorReport
{
    std::vector<BotSessionReport> sessions;
    std::optional<dxa::protocol::GameMatchResult> result;
    int exitCode = 0;
};
```

Add `BotCoordinatorReport Report() const;`. Move `gameSession` from coordinator-global state into each `BotState`. Create one `GameNetworkRuntime`, start it before lobby connect and pass it to every game session. Finish only when all 23 sessions have the same MatchId and result. Preserve lobby-only count 23 behavior.

Destination seed is `matchSeed ^ player.value * 0x9E3779B9U`; sequence remains 30Hz and starts at 1 per match.

- [ ] Step 4: Run focused tests and repeated integration

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=BotClientOptions.*:GameServerIntegration.PlayCoordinator* `
  --gtest_repeat=10 `
  --gtest_break_on_failure
```

Expected: count boundaries 1 and 23 pass, 24 fails, all report entries are present and shared runtime shutdown is clean.

- [ ] Step 5: Commit

```powershell
git add -- apps/bot_client tests/bot_client_options_test.cpp tests/game_server_integration_test.cpp
git commit -m "feat(client): 23개 play bot session 추가" -m "이유: 한 process에서 실제 game protocol session 23개를 실행하고 실패를 bot별로 남겨야 했다." -m "핵심 변경: play count 23, shared runtime, per-session lifecycle과 coordinator report를 추가했다." -m "검증: option RED 뒤 23 session integration을 10회 반복 통과했다."
```

---

### Task 4: 24인 WARP와 headless 수직 경로

Files:

- Create: `tests/network_load_vertical_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/support/game_network_fixture.hpp`

Interfaces:

- Consumes: `NetworkClientController`, WARP `EngineApp`, count 23 `BotCoordinator`, short injected `MatchConfig`.
- Produces: actual DX11 host 1개와 game session 23개의 bounded vertical gate before metrics and optimization work.

- [ ] Step 1: Write the failing Windows vertical test

```cpp
TEST(NetworkLoadVertical, WarpHostAndTwentyThreeBotSessionsFinishOneMatch)
{
    TwentyFourPlayerFixture fixture{ShortMatchConfig()};
    fixture.StartServers();

    dxa::client::NetworkClientController host{fixture.HostOptions(24U)};
    host.Start();
    fixture.WaitForRoom(host);

    dxa::bot_client::BotCoordinator bots{
        fixture.BotIo(), fixture.PlayBotOptions(*host.Room(), 23U)};
    bots.Start();

    EXPECT_EQ(0, dxa::engine::EngineApp{}.Run(
        fixture.HiddenWarpHybridOptions(420U),
        fixture.ShaderPath(),
        fixture.AssetRoot(),
        &host));
    fixture.WaitForResults(host, bots);

    ASSERT_TRUE(host.Result().has_value());
    const auto report = bots.Report();
    ASSERT_EQ(23U, report.sessions.size());
    EXPECT_EQ(*host.Result(), *report.result);
    EXPECT_EQ(0U, fixture.SecretLeakCount());
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=NetworkLoadVertical.*
```

Expected: first real 24-session lifecycle, timeout or report aggregation defect fails under a 20-second watchdog.

- [ ] Step 3: Fix only reproduced vertical defects

Use ephemeral ports and constructor-injected short match timing. Do not add short-match production CLI flags. Start the shared bot runtime once, stop host and bots before servers, and join every thread. Preserve exact ticket and token leak detection without printing secret bytes.

If 420 WARP frames exceed the 20-second watchdog, record the actual elapsed time and reduce only the frame cap while keeping at least two snapshots per session and vsync enabled.

- [ ] Step 4: Repeat the vertical gate

Run:

```powershell
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=NetworkLoadVertical.* `
  --gtest_repeat=5 `
  --gtest_break_on_failure
```

Expected: five runs pass with host and all 23 sessions on one MatchId, at least two snapshots each, one result and no secret leak.

- [ ] Step 5: Commit

```powershell
git add -- tests/network_load_vertical_test.cpp tests/CMakeLists.txt tests/support/game_network_fixture.hpp
git commit -m "test(network): 24인 game session 수직 경로 고정" -m "이유: 2인 경로만으로는 shared runtime과 24 participant lifecycle을 증명할 수 없었다." -m "핵심 변경: WARP host 1개와 play bot session 23개를 잇는 bounded vertical test를 추가했다." -m "검증: end-to-end RED 뒤 24인 수직 경로를 5회 반복 통과했다."
```

---

### Task 5: TCP와 UDP game traffic byte 계측

Files:

- Modify: `protocol/include/dxa/protocol/AsioFramedConnection.hpp`
- Modify: `protocol/src/AsioFramedConnection.cpp`
- Create: `apps/game_common/include/dxa/game_common/NetworkMetrics.hpp`
- Create: `apps/game_common/src/NetworkMetrics.cpp`
- Modify: `apps/game_common/CMakeLists.txt`
- Modify: `apps/game_client/include/dxa/game_client/GameSession.hpp`
- Modify: `apps/game_client/src/GameSession.cpp`
- Modify: `apps/game_server/src/GameServer.cpp`
- Modify: `tests/protocol_asio_framed_connection_test.cpp`
- Modify: `tests/game_session_integration_test.cpp`
- Modify: `tests/game_server_adapter_test.cpp`

Interfaces:

- Consumes: encoded TCP frame size, raw UDP datagram size and game session measurement window.
- Produces: exact per-direction byte counters without packet content and client `GameSessionMetrics` used by reports.

- [ ] Step 1: Write failing byte observer tests

```cpp
TEST(AsioFramedConnection, ReportsHeaderAndPayloadBytesByDirection)
{
    TrafficProbe traffic;
    auto pair = ConnectedSocketPair();
    auto connection = dxa::protocol::AsioFramedConnection::Create(
        std::move(pair.server), OnFrame(), OnClose(), traffic.Observer());
    connection->Start();
    ASSERT_TRUE(connection->Send(EncodedPayload(13U)));
    pair.Run();

    EXPECT_EQ(dxa::protocol::TcpFrameHeaderBytes + 13U, traffic.sent);
    EXPECT_EQ(dxa::protocol::TcpFrameHeaderBytes + pair.ClientPayloadBytes(),
              traffic.received);
}

TEST(GameSession, MetricsCountGameTrafficWithoutSecretMaterial)
{
    FakeGameServer server;
    dxa::game_client::GameSession session{ArenaNavMesh()};
    session.Start(server.StartValues());
    server.CompleteHandshakeAndSnapshot();
    WaitForSnapshot(session);

    const auto metrics = session.Metrics();
    EXPECT_GT(metrics.tcpReceivedBytes, 0U);
    EXPECT_GT(metrics.udpReceivedBytes, 0U);
    EXPECT_EQ(0U, SecretLeakCount(CapturedOutput()));
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: compile failure because byte observer and `GameSession::Metrics()` do not exist.

- [ ] Step 3: Add passive counters

Add:

```cpp
enum class TrafficDirection
{
    Sent,
    Received
};

using ByteObserver = std::function<void(TrafficDirection, std::size_t)>;

struct GameTrafficTotals
{
    std::uint64_t tcpSentBytes = 0U;
    std::uint64_t tcpReceivedBytes = 0U;
    std::uint64_t udpSentBytes = 0U;
    std::uint64_t udpReceivedBytes = 0U;
};

struct GameSessionMetrics
{
    GameTrafficTotals traffic;
    std::uint64_t snapshotsApplied = 0U;
    std::uint64_t snapshotsDiscarded = 0U;
    std::uint64_t snapshotQueueDrops = 0U;
    std::uint64_t keyframeRequests = 0U;
};
```

The framed connection observer receives only direction and encoded byte count. UDP call sites record `bytes.size()` after successful encode and actual receive length before decode. Start the measurement window at `GameServerWelcome` and freeze it after `GameMatchResult`. Lobby traffic stays outside these totals.

- [ ] Step 4: Run focused tests

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=AsioFramedConnection.*:GameSession.*:GameServerAdapter.*
```

Expected: exact frame byte counts pass and existing slow-client, close-after-flush and secret tests stay green.

- [ ] Step 5: Commit

```powershell
git add -- protocol/include/dxa/protocol/AsioFramedConnection.hpp protocol/src/AsioFramedConnection.cpp apps/game_common apps/game_client tests/protocol_asio_framed_connection_test.cpp tests/game_session_integration_test.cpp tests/game_server_adapter_test.cpp
git commit -m "feat(network): game traffic byte 계측 추가" -m "이유: full-state와 optimized mode의 client 수신량을 packet 내용 노출 없이 같은 seam에서 비교해야 했다." -m "핵심 변경: TCP frame observer, UDP byte counter와 GameSessionMetrics를 추가했다." -m "검증: byte observer RED 뒤 protocol, session과 server adapter test를 통과했다."
```

---

### Task 6: Server tick과 replication metrics

Files:

- Create: `apps/game_server/include/dxa/game_server/ServerMatchMetrics.hpp`
- Create: `apps/game_server/src/ServerMatchMetrics.cpp`
- Modify: `apps/game_server/include/dxa/game_server/AuthoritativeMatch.hpp`
- Modify: `apps/game_server/src/AuthoritativeMatch.cpp`
- Modify: `apps/game_server/include/dxa/game_server/GameServer.hpp`
- Modify: `apps/game_server/src/GameServer.cpp`
- Modify: `apps/game_server/CMakeLists.txt`
- Create: `tests/game_server_metrics_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/game_authoritative_match_test.cpp`

Interfaces:

- Consumes: fixed tick execution span, full-state encode span, fragment and recipient byte counts.
- Produces: bounded raw samples and immutable `ServerMatchMetricsSnapshot` available after each match.

- [ ] Step 1: Write failing recorder tests

```cpp
TEST(ServerMatchMetrics, SummarizesNearestRankWithoutDroppingRawSamples)
{
    dxa::game_server::ServerMatchMetrics metrics{128U};
    metrics.RecordTick(1ms);
    metrics.RecordTick(3ms);
    metrics.RecordReplication(2ms, 900U, 1U, false, 124U, 60U);

    const auto snapshot = metrics.Snapshot();
    ASSERT_EQ(2U, snapshot.tickSamples.size());
    EXPECT_EQ(3ms, snapshot.tickP95);
    EXPECT_EQ(900U, snapshot.payloadBytes);
}

TEST(ServerMatchMetrics, RejectsSampleCapacityOverflow)
{
    dxa::game_server::ServerMatchMetrics metrics{2U};
    metrics.RecordTick(1ms);
    metrics.RecordTick(2ms);
    EXPECT_THROW(metrics.RecordTick(3ms), std::overflow_error);
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: target and types do not exist.

- [ ] Step 3: Add a bounded recorder and server accessor

Use:

```cpp
struct ReplicationMetricSample
{
    std::chrono::nanoseconds encodeDuration{};
    std::uint32_t payloadBytes = 0U;
    std::uint16_t fragmentCount = 0U;
    std::uint16_t visibleActors = 0U;
    std::uint16_t visibleLoot = 0U;
    bool keyframe = false;
    bool fallbackKeyframe = false;
};

struct ServerMatchMetricsSnapshot
{
    dxa::protocol::MatchId match;
    std::vector<std::chrono::nanoseconds> tickSamples;
    std::vector<ReplicationMetricSample> replicationSamples;
    std::chrono::nanoseconds tickP95{};
    std::chrono::nanoseconds replicationP95{};
    std::uint64_t tcpBytes = 0U;
    std::uint64_t udpBytes = 0U;
    std::uint64_t payloadBytes = 0U;
    std::uint64_t schedulerOverruns = 0U;
};
```

`GameServer::CompletedMetrics()` returns a copy of completed match snapshots in MatchId order. Store no raw packet or secret. Capacity comes from hard timeout tick plus a bounded recipient replication sample count. Overflow is a load failure, not silent truncation.

- [ ] Step 4: Run focused and scheduler tests

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=ServerMatchMetrics.*:AuthoritativeMatch.*:FixedTickScheduler.*:GameServerAdapter.*
```

Expected: tick order, result and existing scheduler overrun behavior remain unchanged.

- [ ] Step 5: Commit

```powershell
git add -- apps/game_server tests/game_server_metrics_test.cpp tests/game_authoritative_match_test.cpp tests/CMakeLists.txt
git commit -m "feat(server): match tick과 replication metrics 추가" -m "이유: 최적화 전후 server 비용과 payload 크기를 raw sample에서 다시 계산할 수 있어야 했다." -m "핵심 변경: bounded tick, encode, fragment와 traffic recorder 및 completed metrics accessor를 추가했다." -m "검증: recorder RED 뒤 metrics, authoritative match와 scheduler test를 통과했다."
```

---
### Task 7: Full-state load runner와 evidence guard

Files:

- Modify: `apps/game_server/include/dxa/game_server/GameServerOptions.hpp`
- Modify: `apps/game_server/src/GameServerOptions.cpp`
- Modify: `apps/game_server/src/main.cpp`
- Modify: `apps/client/include/dxa/client/ClientOptions.hpp`
- Modify: `apps/client/src/NetworkClientController.cpp`
- Modify: `engine/include/dxa/engine/RuntimeScene.hpp`
- Modify: `engine/src/windows/EngineApp.cpp`
- Create: `scripts/network_load_common.ps1`
- Create: `scripts/run_network_load.ps1`
- Create: `tests/network_load_runner_test.ps1`
- Modify: `tests/game_server_options_test.cpp`
- Modify: `tests/client_options_test.cpp`
- Modify: `tests/engine_app_test.cpp`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: server completed metrics, 24-session bot report, flushed client room and result lines.
- Produces: clean-commit one-match full-state runner, unique evidence directory and an automatic DX11 exit path.

- [ ] Step 1: Write failing option and evidence guard tests

```cpp
TEST(GameServerOptions, ParsesFullStateMetricsOutput)
{
    const auto parsed = Parse({
        "--replication-mode", "full-state",
        "--metrics-output-root", "out/network-load"});
    ASSERT_TRUE(parsed.options.has_value());
    EXPECT_EQ(dxa::protocol::ReplicationMode::FullState,
              parsed.options->replicationMode);
    EXPECT_EQ("out/network-load", parsed.options->metricsOutputRoot);
}

TEST(ClientOptions, NetworkResultCanCloseHiddenClientWithoutFrameLimit)
{
    const auto parsed = Parse({
        "--hidden", "--render-path", "hybrid-deferred",
        "--network-create", "--expected-players", "24",
        "--exit-on-match-result"});
    ASSERT_TRUE(parsed.options.has_value());
    EXPECT_TRUE(parsed.options->network->exitOnMatchResult);
    EXPECT_EQ(0U, parsed.options->frameLimit);
}
```

In `network_load_runner_test.ps1`, create a temporary git repository and assert that a dirty tree, missing commit SHA, reused output directory and count other than 23 fail before a process starts.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=GameServerOptions.*:ClientOptions.*:EngineApp.*
```

Expected: option members and result-driven close seam do not exist.

- [ ] Step 3: Add the minimum runner seam

Add to `IRuntimeSceneController` with a default implementation:

```cpp
[[nodiscard]] virtual bool ShouldClose() const noexcept
{
    return false;
}
```

`NetworkClientController` returns true only after a terminal result or terminal error when `exitOnMatchResult` is set. `EngineApp` exits after the current frame without bypassing renderer verification or destructor order.

`run_network_load.ps1` accepts:

```powershell
param(
    [ValidateSet('full-state')]
    [string]$ReplicationMode = 'full-state',
    [ValidateRange(1, 3)]
    [int]$Matches = 1,
    [uint32[]]$Seeds = @(20260825),
    [Parameter(Mandatory)]
    [string]$CommitSha,
    [switch]$Impairment,
    [switch]$Release
)
```

The script starts lobby and game server once. For each match it starts a hidden WARP client with expected player count 24, reads the flushed actual RoomId, then starts one bot process with count 23 and play mode. It waits at most 11 minutes per process, stops in reverse ownership order and fails on nonzero exit or inconsistent result.

Write `environment.json`, `command.txt`, raw process logs, `server-ticks.csv`, `replication.csv`, `clients.csv`, `summary.json` and `RESULT.md` under a new run directory. The runner scans outputs for deterministic test secrets but never receives production secret values.

- [ ] Step 4: Run guard and one short fixture smoke

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=GameServerOptions.*:ClientOptions.*:EngineApp.*

powershell -NoProfile -File tests/network_load_runner_test.ps1 `
  -RepositoryRoot .
```

Expected: option tests, result-driven close and all evidence boundary failures pass. The test script uses fake executables and does not wait for a production match.

- [ ] Step 5: Commit

```powershell
git add -- apps/game_server apps/client engine/include/dxa/engine/RuntimeScene.hpp engine/src/windows/EngineApp.cpp scripts/network_load_common.ps1 scripts/run_network_load.ps1 tests
git commit -m "feat(benchmark): full-state network 기준선 runner 추가" -m "이유: 24인 production 경기와 raw traffic 및 tick evidence를 같은 process topology에서 재현해야 했다." -m "핵심 변경: result-driven DX11 종료, server metrics output과 clean-commit load runner를 추가했다." -m "검증: option RED 뒤 runner guard, client close와 server metrics test를 통과했다."
```

---

### Task 8: Full-state 24인 기준선 기록

Files:

- Create: runner-generated immutable directory under `docs/benchmarks/network-load/` containing `environment.json`
- Create: same generated directory `command.txt`
- Create: same generated directory `server-ticks.csv`
- Create: same generated directory `replication.csv`
- Create: same generated directory `clients.csv`
- Create: same generated directory `summary.json`
- Create: same generated directory `RESULT.md`

Interfaces:

- Consumes: clean Release commit from Task 7 and seed 20260825.
- Produces: immutable full-state one-match baseline used to judge every optimization stage.

- [ ] Step 1: Build Release and verify clean state

Run:

```powershell
./scripts/build.ps1 -Preset windows-msvc-release
git status --short
git rev-parse HEAD
```

Expected: Release build succeeds, status is empty and HEAD equals the `-CommitSha` passed next.

- [ ] Step 2: Run the production baseline

Run:

```powershell
$sha = git rev-parse HEAD
$runDirectory = ./scripts/run_network_load.ps1 `
  -ReplicationMode full-state `
  -Matches 1 `
  -Seeds 20260825 `
  -CommitSha $sha `
  -Release
```

Expected: one DX11 WARP client and 23 bot sessions finish the same MatchId and result before 11 minutes. The runner prints the new directory only after all evidence files are flushed.

- [ ] Step 3: Verify evidence consistency

Check that `summary.json` totals equal the raw CSV rows, participant count is 24, bot session count is 23, neutral count is 100, replication mode is full-state, impairment is disabled and secret leak count is 0. Do not add target claims before reading the actual result.

- [ ] Step 4: Commit the baseline without rewriting its values

```powershell
git add -- $runDirectory
git commit -m "bench(network): full-state 24인 기준선 기록" -m "이유: 관심 영역과 delta 적용 전의 server tick, payload와 client 수신량 원본을 잠가야 했다." -m "핵심 변경: production 24인 full-state 한 경기의 환경, raw CSV, summary와 결과 문서를 추가했다." -m "검증: raw 합계, participant와 AI count, seed, commit SHA, mode, result와 secret leak 0을 대조했다."
```

`$runDirectory` must be the exact path printed by the successful runner. Do not rename its timestamp or commit segment after generation.

---

### Task 9: Quantized replication value와 bounds

Files:

- Create: `protocol/include/dxa/protocol/ReplicationSnapshot.hpp`
- Modify: `protocol/CMakeLists.txt`
- Create: `tests/protocol_replication_snapshot_codec_test.cpp`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: `GameSnapshot`, arena minimum and maximum, protocol v2.
- Produces: bounded wire values, payload kind, field masks and pure quantize/dequantize functions used by server and client.

- [ ] Step 1: Write failing quantization tests

```cpp
TEST(ReplicationSnapshot, QuantizedCoordinateRoundTripStaysWithinHalfStep)
{
    constexpr float minimum = -128.0F;
    constexpr float maximum = 128.0F;
    for (const float value : {-128.0F, -31.25F, 0.0F, 79.5F, 128.0F})
    {
        const std::uint16_t encoded = QuantizeCoordinate(value, minimum, maximum);
        const float decoded = DequantizeCoordinate(encoded, minimum, maximum);
        EXPECT_LE(std::abs(decoded - value), (maximum - minimum) / 131070.0F);
    }
}

TEST(ReplicationSnapshot, RejectsOutOfArenaAndNonFiniteCoordinates)
{
    EXPECT_THROW(QuantizeCoordinate(129.0F, -128.0F, 128.0F),
                 std::out_of_range);
    EXPECT_THROW(QuantizeCoordinate(
        std::numeric_limits<float>::quiet_NaN(), -128.0F, 128.0F),
        std::invalid_argument);
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: header and quantization functions do not exist.

- [ ] Step 3: Define the wire values

Define:

```cpp
enum class SnapshotPayloadKind : std::uint8_t
{
    FullState = 1,
    Keyframe = 2,
    Delta = 3
};

enum class ActorField : std::uint8_t
{
    Position = 1U << 0U,
    HealthAlive = 1U << 1U,
    WeaponCooldown = 1U << 2U,
    Eliminations = 1U << 3U
};

enum class GlobalField : std::uint8_t
{
    Phase = 1U << 0U,
    SafeZone = 1U << 1U,
    AliveContenders = 1U << 2U,
    Result = 1U << 3U,
    EventChecksum = 1U << 4U
};

struct QuantizedVec2
{
    std::uint16_t x = 0U;
    std::uint16_t z = 0U;
};
```

Add full enter/keyframe values, mutable delta values, sorted removed ID vectors and `SnapshotPayloadHeader`. Use uint8 health and eliminations, uint16 cooldown and safe-zone radius. Validate mask bits and numeric ranges in constructors or encode-time validation, never by silent clamp.

- [ ] Step 4: Run focused tests

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=ReplicationSnapshot.*:GameTypes.*
```

Expected: exact bounds and half-step error tests pass.

- [ ] Step 5: Commit

```powershell
git add -- protocol/include/dxa/protocol/ReplicationSnapshot.hpp protocol/CMakeLists.txt tests/protocol_replication_snapshot_codec_test.cpp tests/CMakeLists.txt
git commit -m "feat(protocol): quantized replication value 추가" -m "이유: 관심 영역 keyframe과 delta가 bounded integer와 field mask를 공유해야 했다." -m "핵심 변경: payload kind, quantized world value, mask와 coordinate mapping contract를 추가했다." -m "검증: value 부재 RED 뒤 coordinate, range와 mask focused test를 통과했다."
```

---

### Task 10: Replication snapshot codec

Files:

- Create: `protocol/include/dxa/protocol/ReplicationSnapshotCodec.hpp`
- Create: `protocol/src/ReplicationSnapshotCodec.cpp`
- Modify: `protocol/CMakeLists.txt`
- Modify: `tests/protocol_replication_snapshot_codec_test.cpp`
- Modify: `tests/protocol_game_udp_codec_test.cpp`

Interfaces:

- Consumes: Task 9 payload values and current `ByteWriter`, `ByteReader`.
- Produces: canonical little-endian payload bytes and bounded decode result passed to the unchanged fragment codec.

- [ ] Step 1: Write failing exact-byte and malformed tests

```cpp
TEST(ReplicationSnapshotCodec, RoundTripsKeyframeDeltaEnterAndRemove)
{
    const auto source = RepresentativeDeltaPayload();
    const auto bytes = EncodeSnapshotPayload(source);
    const auto decoded = DecodeSnapshotPayload(bytes);
    ASSERT_TRUE(decoded.payload.has_value());
    EXPECT_EQ(source, *decoded.payload);
}

TEST(ReplicationSnapshotCodec, RejectsDeltaWithoutBase)
{
    auto source = RepresentativeDeltaPayload();
    source.header.baseSnapshotId = 0U;
    EXPECT_THROW(EncodeSnapshotPayload(source), std::invalid_argument);
}

TEST(ReplicationSnapshotCodec, RejectsNonCanonicalAndDuplicateIds)
{
    auto bytes = EncodeSnapshotPayload(RepresentativeKeyframePayload());
    MutateSecondActorIdToFirst(bytes);
    EXPECT_EQ(SnapshotPayloadDecodeError::DuplicateEntity,
              DecodeSnapshotPayload(bytes).error);
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: codec functions and decode error enum do not exist.

- [ ] Step 3: Implement explicit codec validation

Expose:

```cpp
[[nodiscard]] std::vector<std::byte> EncodeSnapshotPayload(
    const SnapshotPayload& payload);

[[nodiscard]] SnapshotPayloadDecodeResult DecodeSnapshotPayload(
    std::span<const std::byte> bytes);
```

Validate kind and base combination, payload SnapshotId, known mask bits, count before allocation, sorted unique IDs, field presence, result consistency, trailing bytes and `MaxSnapshotPayloadBytes`. FullState delegates to the existing `EncodeGameSnapshot` and `DecodeGameSnapshot` behind the new payload envelope so the baseline uses the same fragmentation path.

- [ ] Step 4: Run codec and fragment tests

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=ReplicationSnapshotCodec.*:GameSnapshotCodec.*:GameUdpCodec.*
```

Expected: representative payloads round trip, every malformed class is rejected and 1,200-byte plus 32-fragment limits remain green.

- [ ] Step 5: Commit

```powershell
git add -- protocol/include/dxa/protocol/ReplicationSnapshotCodec.hpp protocol/src/ReplicationSnapshotCodec.cpp protocol/CMakeLists.txt tests/protocol_replication_snapshot_codec_test.cpp tests/protocol_game_udp_codec_test.cpp
git commit -m "feat(protocol): replication snapshot codec 추가" -m "이유: keyframe과 delta payload가 bounded canonical wire representation을 가져야 했다." -m "핵심 변경: full-state envelope, quantized keyframe, delta와 malformed decode 검증을 추가했다." -m "검증: codec 부재 RED 뒤 payload, 기존 snapshot과 fragment test를 통과했다."
```

---

### Task 11: Recipient 관심 영역 grid

Files:

- Create: `apps/game_server/include/dxa/game_server/InterestGrid.hpp`
- Create: `apps/game_server/src/InterestGrid.cpp`
- Modify: `apps/game_server/CMakeLists.txt`
- Create: `tests/game_interest_grid_test.cpp`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: sorted `GameSnapshot` actor and loot positions.
- Produces: recipient visible IDs matching brute-force enter and hysteresis leave policy.

- [ ] Step 1: Write failing brute-force parity tests

```cpp
TEST(InterestGrid, MatchesBruteForceForSeededQueries)
{
    const auto world = SeededWorld(20260825U, 124U, 60U);
    dxa::game_server::InterestGrid grid{ArenaBounds(), 32.0F};
    grid.Rebuild(world);

    for (const auto& recipient : RecipientPositions())
    {
        const auto actual = grid.Query(recipient, 80.0F);
        const auto expected = BruteForceQuery(world, recipient, 80.0F);
        EXPECT_EQ(expected, actual);
    }
}

TEST(InterestGrid, UsesEightyEnterAndEightyEightLeaveRadius)
{
    dxa::game_server::InterestGrid grid{ArenaBounds(), 32.0F};
    grid.Rebuild(WorldWithActorsAt(79.99F, 80.01F, 87.99F, 88.01F));

    auto visible = grid.UpdateVisibility({}, Origin(), 80.0F, 88.0F);
    EXPECT_TRUE(visible.actors.contains(EntityAt(79.99F)));
    EXPECT_FALSE(visible.actors.contains(EntityAt(80.01F)));

    visible = grid.UpdateVisibility(visible, Origin(), 80.0F, 88.0F);
    EXPECT_TRUE(visible.actors.contains(EntityAt(87.99F)));
    EXPECT_FALSE(visible.actors.contains(EntityAt(88.01F)));
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: target and interface do not exist.

- [ ] Step 3: Implement the pure grid module

Use an 8 by 8 grid for map 1, store IDs in sorted vectors per cell and query only cells intersecting the radius AABB before exact squared-distance checks. Expose:

```cpp
struct VisibleSet
{
    std::vector<dxa::protocol::EntityId> actors;
    std::vector<std::uint32_t> loot;
};

class InterestGrid
{
public:
    InterestGrid(dxa::simulation::Aabb2 bounds, float cellSize);
    void Rebuild(const dxa::protocol::GameSnapshot& world);
    [[nodiscard]] VisibleSet UpdateVisibility(
        const VisibleSet& previous,
        dxa::protocol::NetworkVec2 center,
        float enterRadius,
        float leaveRadius) const;
};
```

Reject non-finite bounds, invalid radii, duplicate IDs and out-of-bounds positions. Sort and unique results before returning.

- [ ] Step 4: Run parity tests repeatedly

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=InterestGrid.* `
  --gtest_repeat=20 `
  --gtest_break_on_failure
```

Expected: grid matches brute force for every seed and boundary run.

- [ ] Step 5: Commit

```powershell
git add -- apps/game_server/include/dxa/game_server/InterestGrid.hpp apps/game_server/src/InterestGrid.cpp apps/game_server/CMakeLists.txt tests/game_interest_grid_test.cpp tests/CMakeLists.txt
git commit -m "feat(server): recipient 관심 영역 grid 추가" -m "이유: recipient별 actor와 loot visibility를 전수 검색 없이 같은 규칙으로 계산해야 했다." -m "핵심 변경: 32m grid, 80m enter, 88m leave와 sorted visibility 결과를 추가했다." -m "검증: target 부재 RED 뒤 brute-force parity를 20회 반복 통과했다."
```

---

### Task 12: 관심 영역 keyframe replication

Files:

- Create: `apps/game_server/include/dxa/game_server/SnapshotReplicator.hpp`
- Create: `apps/game_server/src/SnapshotReplicator.cpp`
- Modify: `apps/game_server/CMakeLists.txt`
- Create: `tests/game_snapshot_replicator_test.cpp`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: Task 9 and 10 payload codec contract, Task 11 InterestGrid and arena definition.
- Produces: one deep replication interface supporting FullState, InterestFullPrecision and InterestQuantized keyframes before delta state exists.

- [ ] Step 1: Write failing mode and visibility tests

```cpp
TEST(SnapshotReplicator, FullStatePreservesTheExistingWorld)
{
    SnapshotReplicator replicator{Arena(), FullStateConfig()};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    const auto build = replicator.Build(PlayerId{1U}, 1U, World());
    EXPECT_EQ(SnapshotPayloadKind::FullState, build.payload.header.kind);
    EXPECT_EQ(124U, build.visibleActorCount);
    EXPECT_EQ(60U, build.visibleLootCount);
}

TEST(SnapshotReplicator, InterestKeyframeAlwaysContainsLocalAndGlobalState)
{
    SnapshotReplicator replicator{Arena(), InterestFullConfig()};
    replicator.RegisterRecipient(PlayerId{7U}, EntityId{0U});
    const auto build = replicator.Build(PlayerId{7U}, 1U, SparseWorld());
    EXPECT_EQ(SnapshotPayloadKind::Keyframe, build.payload.header.kind);
    EXPECT_TRUE(ContainsActor(build.payload, EntityId{0U}));
    EXPECT_EQ(SparseWorld().result, DecodeGlobal(build.payload).result);
}

TEST(SnapshotReplicator, QuantizedKeyframeRespectsHalfStepError)
{
    SnapshotReplicator replicator{Arena(), InterestQuantizedConfig()};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    const auto decoded = DecodeApplied(replicator.Build(
        PlayerId{1U}, 1U, World()));
    ExpectWorldWithinQuantizationError(World(), decoded);
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: `SnapshotReplicator` target does not exist.

- [ ] Step 3: Implement the deep keyframe interface

Use the exact interface from the spec:

```cpp
class SnapshotReplicator
{
public:
    SnapshotReplicator(
        const dxa::simulation::ArenaMapDefinition& arena,
        ReplicationConfig config);
    void RegisterRecipient(PlayerId player, EntityId controlledActor);
    [[nodiscard]] bool AcceptAcknowledgement(
        PlayerId player, std::uint32_t snapshotId);
    void RequestKeyframe(PlayerId player);
    [[nodiscard]] ReplicationBuild Build(
        PlayerId player,
        std::uint32_t snapshotId,
        const GameSnapshot& world);
    void RemoveRecipient(PlayerId player);
};
```

For this task, `AcceptAcknowledgement` records only issued ID bounds and every non-FullState Build is a keyframe. Full precision keeps floats. Quantized mode uses Task 9 values. Store the resulting recipient world view in a bounded ring even though delta consumption lands in Task 13.

- [ ] Step 4: Run focused mode tests

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=SnapshotReplicator.*:InterestGrid.*:ReplicationSnapshot*
```

Expected: full-state exact equality, interest visibility and quantization error pass.

- [ ] Step 5: Commit

```powershell
git add -- apps/game_server/include/dxa/game_server/SnapshotReplicator.hpp apps/game_server/src/SnapshotReplicator.cpp apps/game_server/CMakeLists.txt tests/game_snapshot_replicator_test.cpp tests/CMakeLists.txt
git commit -m "feat(server): 관심 영역 keyframe replication 추가" -m "이유: full-state, interest와 quantization 효과를 같은 recipient interface에서 단계별로 비교해야 했다." -m "핵심 변경: SnapshotReplicator와 세 keyframe mode, visible set과 bounded baseline 저장을 추가했다." -m "검증: module 부재 RED 뒤 full-state, interest와 quantized keyframe test를 통과했다."
```

---

### Task 13: ACK 기반 delta baseline

Files:

- Modify: `apps/game_server/include/dxa/game_server/SnapshotReplicator.hpp`
- Modify: `apps/game_server/src/SnapshotReplicator.cpp`
- Modify: `tests/game_snapshot_replicator_test.cpp`

Interfaces:

- Consumes: Task 12 recipient world ring and Task 10 delta codec.
- Produces: monotonic ACK validation, field bitset, enter, leave, periodic and fallback keyframe behavior.

- [ ] Step 1: Write failing ACK and delta tests

```cpp
TEST(SnapshotReplicator, BuildsDeltaAgainstAcknowledgedRecipientView)
{
    SnapshotReplicator replicator{Arena(), InterestDeltaConfig()};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    const auto keyframe = replicator.Build(PlayerId{1U}, 1U, WorldAtTick(2U));
    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 1U));

    auto changed = WorldAtTick(4U);
    MoveActor(changed, EntityId{3U}, NetworkVec2{10.0F, 11.0F});
    const auto delta = replicator.Build(PlayerId{1U}, 2U, changed);

    EXPECT_EQ(SnapshotPayloadKind::Delta, delta.payload.header.kind);
    EXPECT_EQ(1U, delta.payload.header.baseSnapshotId);
    EXPECT_EQ(ActorField::Position, OnlyActorDelta(delta.payload).fields);
}

TEST(SnapshotReplicator, EnterAndLeaveUseFullRecordAndRemoveId)
{
    SnapshotReplicator replicator{Arena(), InterestDeltaConfig()};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    const auto first = replicator.Build(PlayerId{1U}, 1U, EntryWorld(false));
    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 1U));

    const auto entered = replicator.Build(PlayerId{1U}, 2U, EntryWorld(true));
    EXPECT_TRUE(ContainsFullActor(entered.payload, EntityId{9U}));
    ASSERT_TRUE(replicator.AcceptAcknowledgement(PlayerId{1U}, 2U));

    const auto left = replicator.Build(PlayerId{1U}, 3U, LeaveWorld());
    EXPECT_THAT(left.payload.removedActors, ElementsAre(EntityId{9U}));
}

TEST(SnapshotReplicator, UnknownAckAndThirtySnapshotIntervalForceKeyframe)
{
    SnapshotReplicator replicator{Arena(), InterestDeltaConfig()};
    replicator.RegisterRecipient(PlayerId{1U}, EntityId{0U});
    EXPECT_FALSE(replicator.AcceptAcknowledgement(PlayerId{1U}, 999U));
    EXPECT_TRUE(replicator.Build(PlayerId{1U}, 1U, World()).keyframe);
    ExpectKeyframeAtOrdinal(replicator, 31U);
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=SnapshotReplicator.*
```

Expected: builds remain keyframes and ACK has no delta effect.

- [ ] Step 3: Implement recipient baseline lifecycle

Each recipient stores issued high watermark, monotonic acknowledged ID, keyframe ordinal, visible set and up to 32 reconstructed quantized world views. Same and older ACK return true without mutation. Future ACK returns false. Issued but evicted ACK resets the usable baseline and forces a fallback keyframe without closing the session.

Field comparison operates on quantized values. Immutable role and archetype mismatch throws an internal invariant error. Store a new baseline only after payload encode validation succeeds. Never evict the currently acknowledged baseline unless capacity forces fallback keyframe state.

- [ ] Step 4: Run delta and parity tests repeatedly

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=SnapshotReplicator.*:ReplicationSnapshotCodec.* `
  --gtest_repeat=20 `
  --gtest_break_on_failure
```

Expected: reconstructed delta equals the corresponding quantized keyframe for every seeded test, ring stays at 32 and keyframe interval is exactly 30 snapshots.

- [ ] Step 5: Commit

```powershell
git add -- apps/game_server/include/dxa/game_server/SnapshotReplicator.hpp apps/game_server/src/SnapshotReplicator.cpp tests/game_snapshot_replicator_test.cpp
git commit -m "feat(server): ACK 기반 delta replication 추가" -m "이유: packet 손실 뒤에도 client가 실제 보유한 baseline에서만 delta를 만들어야 했다." -m "핵심 변경: monotonic ACK, 32개 recipient ring, field delta, enter, leave와 periodic 및 fallback keyframe을 추가했다." -m "검증: keyframe-only RED 뒤 delta reconstruction과 ring test를 20회 반복 통과했다."
```

---

### Task 14: Client snapshot stream

Files:

- Create: `apps/game_client/include/dxa/game_client/ClientSnapshotStream.hpp`
- Create: `apps/game_client/src/ClientSnapshotStream.cpp`
- Modify: `apps/game_client/include/dxa/game_client/SnapshotReassembler.hpp`
- Modify: `apps/game_client/src/SnapshotReassembler.cpp`
- Modify: `apps/game_client/CMakeLists.txt`
- Create: `tests/game_client_snapshot_stream_test.cpp`
- Modify: `tests/game_snapshot_reassembler_test.cpp`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: complete reassembled payload bytes and Task 10 decode result.
- Produces: applied world, last ACK, missing-base recovery and remove semantics used by `GameSession`.

- [ ] Step 1: Write failing stream tests

```cpp
TEST(ClientSnapshotStream, AppliesKeyframeThenDeltaAndAdvancesAck)
{
    ClientSnapshotStream stream{32U};
    const auto keyframe = stream.Apply(1U, KeyframePayload(WorldA()));
    ASSERT_TRUE(keyframe.world.has_value());
    EXPECT_EQ(1U, keyframe.acknowledgedSnapshotId);

    const auto delta = stream.Apply(2U, DeltaPayload(1U, WorldA(), WorldB()));
    ASSERT_TRUE(delta.world.has_value());
    EXPECT_EQ(Quantized(WorldB()), Quantized(*delta.world));
    EXPECT_EQ(2U, delta.acknowledgedSnapshotId);
}

TEST(ClientSnapshotStream, MissingBaseDoesNotMutateAndRequestsKeyframe)
{
    ClientSnapshotStream stream{32U};
    const auto result = stream.Apply(8U, DeltaPayload(7U, WorldA(), WorldB()));
    EXPECT_FALSE(result.world.has_value());
    EXPECT_EQ(0U, result.acknowledgedSnapshotId);
    EXPECT_TRUE(result.requestKeyframe);
}

TEST(ClientSnapshotStream, RemoveAndReenterResetInterpolationIdentity)
{
    ClientSnapshotStream stream{32U};
    stream.Apply(1U, KeyframePayload(WorldWithActor(9U)));
    const auto removed = stream.Apply(2U, RemoveActorDelta(1U, 9U));
    EXPECT_FALSE(ContainsActor(*removed.world, EntityId{9U}));
    const auto entered = stream.Apply(3U, EnterActorDelta(2U, Actor(9U)));
    EXPECT_TRUE(entered.reenteredActors.contains(EntityId{9U}));
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: stream target and reassembler byte payload output do not exist.

- [ ] Step 3: Implement apply and recovery

`SnapshotReassembler` returns complete bytes, SnapshotId, server tick and input ACK after length and CRC checks. It no longer calls `DecodeGameSnapshot` itself.

Use the spec interface:

```cpp
struct SnapshotApplyResult
{
    std::optional<dxa::protocol::GameSnapshot> world;
    std::vector<dxa::protocol::EntityId> removedActors;
    std::vector<dxa::protocol::EntityId> reenteredActors;
    std::uint32_t acknowledgedSnapshotId = 0U;
    bool requestKeyframe = false;
};
```

Reject stale and duplicate SnapshotIds without changing ACK. Missing base keeps the previous applied world internal but returns no new world and requests keyframe. Keyframe replaces the complete visible view and clears the request. Delta applies global fields, removals, enters and field masks in canonical order, then stores the result under the new SnapshotId. Keep at most 32 worlds.

- [ ] Step 4: Run stream and reassembler tests

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=ClientSnapshotStream.*:SnapshotReassembler.* `
  --gtest_repeat=20 `
  --gtest_break_on_failure
```

Expected: base recovery, stale handling, enter, leave and ring bounds pass repeatedly.

- [ ] Step 5: Commit

```powershell
git add -- apps/game_client/include/dxa/game_client/ClientSnapshotStream.hpp apps/game_client/src/ClientSnapshotStream.cpp apps/game_client/include/dxa/game_client/SnapshotReassembler.hpp apps/game_client/src/SnapshotReassembler.cpp apps/game_client/CMakeLists.txt tests/game_client_snapshot_stream_test.cpp tests/game_snapshot_reassembler_test.cpp tests/CMakeLists.txt
git commit -m "feat(client): delta snapshot stream 적용" -m "이유: reassembled payload를 ACK baseline에 안전하게 적용하고 missing base를 keyframe으로 복구해야 했다." -m "핵심 변경: ClientSnapshotStream, byte reassembly, remove, re-enter와 32개 client baseline을 추가했다." -m "검증: module 부재 RED 뒤 stream과 reassembler test를 20회 반복 통과했다."
```

---

### Task 15: Replication server와 client 수직 연결

Files:

- Modify: `apps/game_server/include/dxa/game_server/GameServerOptions.hpp`
- Modify: `apps/game_server/src/GameServerOptions.cpp`
- Modify: `apps/game_server/include/dxa/game_server/GameServer.hpp`
- Modify: `apps/game_server/src/GameServer.cpp`
- Modify: `apps/game_server/src/AuthoritativeMatch.cpp`
- Modify: `apps/game_client/include/dxa/game_client/GameSession.hpp`
- Modify: `apps/game_client/src/GameSession.cpp`
- Modify: `apps/client/include/dxa/client/ClientOptions.hpp`
- Modify: `apps/client/src/NetworkClientController.cpp`
- Modify: `tests/game_server_options_test.cpp`
- Modify: `tests/client_options_test.cpp`
- Modify: `tests/game_authoritative_match_test.cpp`
- Modify: `tests/game_session_integration_test.cpp`
- Modify: `tests/game_server_integration_test.cpp`
- Modify: `tests/network_load_vertical_test.cpp`

Interfaces:

- Consumes: protocol v2, `SnapshotReplicator`, `ClientSnapshotStream`, metrics and 24-session vertical fixture.
- Produces: four replication modes over actual game TCP and UDP, ACK processing, keyframe request and comparable results.

- [ ] Step 1: Write failing end-to-end mode tests

```cpp
TEST(GameServerIntegration, InterestDeltaRecoversMissingBaseAndFinishes)
{
    GameNetworkFixture fixture{
        ShortTwentyFourPlayerConfig(),
        dxa::protocol::ReplicationMode::InterestDelta};
    auto clients = fixture.AuthenticateTwentyFourPlayers();
    fixture.DropCompleteSnapshotFor(clients[3], 2U);
    fixture.RunUntilResults(clients);

    EXPECT_GE(clients[3]->KeyframeRequests(), 1U);
    EXPECT_GE(clients[3]->KeyframesApplied(), 2U);
    ExpectSameResult(clients);
}

TEST(AuthoritativeMatch, FutureSnapshotAckIsProtocolViolation)
{
    auto match = StartedMatch(ReplicationMode::InterestDelta);
    auto input = ValidInput(PlayerId{1U}, 1U);
    input.acknowledgedSnapshotId = 999U;
    const auto result = match.ReceiveClientDatagram(Peer1(), input);
    EXPECT_THAT(result.closeTcp, Contains(Connection1()));
    EXPECT_EQ(GameServerErrorCode::ProtocolViolation,
              OnlyServerError(result).code);
}

TEST(NetworkLoadVertical, EveryReplicationModeMatchesFullStateResult)
{
    const auto baseline = RunTwentyFourPlayerMode(ReplicationMode::FullState);
    for (const auto mode : {ReplicationMode::InterestFullPrecision,
                            ReplicationMode::InterestQuantized,
                            ReplicationMode::InterestDelta})
    {
        EXPECT_EQ(baseline.result, RunTwentyFourPlayerMode(mode).result);
    }
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=AuthoritativeMatch.*:GameSession.*:GameServerIntegration.*:NetworkLoadVertical.*
```

Expected: server still emits legacy full-state payload and client does not send snapshot ACK.

- [ ] Step 3: Wire server mode and recipient build

Parse all four `--replication-mode` values and pass `ReplicationConfig` through `GameServerConfig` into `AuthoritativeMatch::Create`. Include mode in `GameServerWelcome` and reject mismatch before UDP bind.

At every valid input, validate SnapshotId ACK against the session's issued high watermark. Same and old values are ignored. A future value sends protocol error and closes TCP. `requestKeyframe` sets recipient recovery state.

On even tick, build the world and grid once, then call replicator per connected UDP-bound recipient. Record encode duration and payload metadata before fragmenting.

- [ ] Step 4: Wire client apply, prediction and visibility

`GameSession` stores last applied SnapshotId and keyframe request. Every 30Hz input repeats both values. Complete payloads go through `ClientSnapshotStream`; only returned worlds update predictor and interpolation.

Remove actor IDs clear interpolation samples. Re-enter IDs start from the new authoritative sample without prior velocity. FullState mode remains exact to Week 9 behavior. Update `GameSceneFrame` metrics and bot report counts.

- [ ] Step 5: Run all replication modes repeatedly

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=AuthoritativeMatch.*:GameSession.*:GameServerIntegration.*:NetworkLoadVertical.* `
  --gtest_repeat=5 `
  --gtest_break_on_failure
```

Expected: every mode produces the same result, missing base recovers, full-state tests remain unchanged and 24-session WARP runs finish.

- [ ] Step 6: Commit

```powershell
git add -- apps/game_server apps/game_client apps/client tests
git commit -m "test(network): optimized snapshot 수직 경로 고정" -m "이유: recipient replication과 client delta apply가 실제 TCP, UDP와 WARP 24인 경기에서 같은 result를 만들어야 했다." -m "핵심 변경: mode negotiation, ACK 처리, recipient payload, client recovery와 four-mode vertical test를 연결했다." -m "검증: legacy full-state RED 뒤 네 mode의 24인 경로를 5회 반복 통과했다."
```

---

### Task 16: 결정적 UDP DatagramShaper

Files:

- Create: `protocol/include/dxa/protocol/DatagramShaper.hpp`
- Create: `protocol/src/DatagramShaper.cpp`
- Modify: `protocol/CMakeLists.txt`
- Create: `tests/game_datagram_shaper_test.cpp`
- Modify: `tests/CMakeLists.txt`

Interfaces:

- Consumes: direction seed, peer identity, datagram ordinal and impairment config.
- Produces: deterministic drop and delivery delay decision shared by client and server adapters.

- [ ] Step 1: Write failing deterministic model tests

```cpp
TEST(DatagramShaper, RepeatsDropAndDelayForSameSeed)
{
    const DatagramShaperConfig config{50ms, 10ms, 200U, 20260825U};
    DatagramShaper first{config, DatagramDirection::ClientToServer};
    DatagramShaper second{config, DatagramDirection::ClientToServer};

    for (std::uint64_t ordinal = 1U; ordinal <= 10000U; ++ordinal)
    {
        EXPECT_EQ(first.Decide(Peer1(), ordinal),
                  second.Decide(Peer1(), ordinal));
    }
}

TEST(DatagramShaper, FixedSampleHasExactlyTwoPercentLoss)
{
    DatagramShaper shaper{{50ms, 10ms, 200U, 20260825U},
                           DatagramDirection::ServerToClient};
    std::uint32_t dropped = 0U;
    for (std::uint64_t ordinal = 1U; ordinal <= 10000U; ++ordinal)
    {
        dropped += shaper.Decide(Peer1(), ordinal).drop ? 1U : 0U;
    }
    EXPECT_EQ(213U, dropped);
}

TEST(DatagramShaper, DisabledConfigHasZeroDelayAndNoDrop)
{
    DatagramShaper shaper{{}, DatagramDirection::ClientToServer};
    EXPECT_EQ(ShapedDatagramDecision{}, shaper.Decide(Peer1(), 1U));
}
```

The locked value 213 comes from the exact SplitMix64 constants and input combination defined in Step 3. The RED must fail because the module is absent, not because the expected count is adjusted after observing another algorithm.

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
```

Expected: shaper header and target do not exist.

- [ ] Step 3: Implement a pure bounded decision module

Use:

```cpp
enum class DatagramDirection : std::uint8_t
{
    ClientToServer = 1,
    ServerToClient = 2
};

struct ShapedDatagramDecision
{
    bool drop = false;
    std::chrono::milliseconds delay{0};
};

class DatagramShaper
{
public:
    DatagramShaper(DatagramShaperConfig config, DatagramDirection direction);
    [[nodiscard]] ShapedDatagramDecision Decide(
        std::uint64_t peerKey,
        std::uint64_t ordinal) const noexcept;
};
```

Hash seed, direction, peer and ordinal with SplitMix64. XOR direction multiplied by `0xD6E8FEB86659FD93`, peer multiplied by `0xA0761D6478BD642F`, and ordinal multiplied by `0xE7037ED1A0B428DB` into the seed before the standard SplitMix64 mix. Drop when `mixed % 10000 < lossBasisPoints`; this produces 213 drops for the locked 10,000-packet server-to-client fixture. Use integer basis points and integer milliseconds. Derive jitter from a second mix, clamp negative latency plus jitter to 0, and reject loss above 10000, negative durations and seed 0 when impairment is enabled.

- [ ] Step 4: Run deterministic tests repeatedly

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=DatagramShaper.* `
  --gtest_repeat=50 `
  --gtest_break_on_failure
```

Expected: exact decisions remain identical across 50 runs and directions use different sequences.

- [ ] Step 5: Commit

```powershell
git add -- protocol/include/dxa/protocol/DatagramShaper.hpp protocol/src/DatagramShaper.cpp protocol/CMakeLists.txt tests/game_datagram_shaper_test.cpp tests/CMakeLists.txt
git commit -m "feat(network): 결정적 UDP datagram shaper 추가" -m "이유: RTT, loss와 jitter를 OS 도구 없이 Windows와 Linux에서 같은 seed로 재현해야 했다." -m "핵심 변경: direction, peer와 ordinal 기반 drop 및 delay model과 fixed-sample contract를 추가했다." -m "검증: target 부재 RED 뒤 deterministic test를 50회 반복 통과했다."
```

---

### Task 17: UDP impairment adapter와 recovery integration

Files:

- Modify: `apps/game_server/include/dxa/game_server/GameServerOptions.hpp`
- Modify: `apps/game_server/include/dxa/game_server/GameServer.hpp`
- Modify: `apps/game_server/src/GameServerOptions.cpp`
- Modify: `apps/game_server/src/GameServer.cpp`
- Modify: `apps/game_client/include/dxa/game_client/GameSession.hpp`
- Modify: `apps/game_client/src/GameSession.cpp`
- Modify: `apps/bot_client/include/dxa/bot_client/BotClientOptions.hpp`
- Modify: `apps/bot_client/src/BotClientOptions.cpp`
- Modify: `apps/client/include/dxa/client/ClientOptions.hpp`
- Modify: `tests/game_server_options_test.cpp`
- Modify: `tests/bot_client_options_test.cpp`
- Modify: `tests/client_options_test.cpp`
- Modify: `tests/game_server_integration_test.cpp`
- Modify: `tests/game_session_integration_test.cpp`

Interfaces:

- Consumes: Task 16 decision model and existing Asio UDP send and receive loops.
- Produces: bounded delayed queues, CLI impairment profile, counters and end-to-end recovery under 100ms RTT, 2% loss and 10ms jitter.

- [ ] Step 1: Write failing queue and integration tests

```cpp
TEST(GameServerIntegration, ImpairmentProfileFinishesWithKeyframeRecovery)
{
    GameNetworkFixture fixture{
        ShortTwentyFourPlayerConfig(),
        ReplicationMode::InterestDelta,
        ImpairmentProfile{50ms, 10ms, 200U, 20260825U}};
    const auto report = fixture.RunTwentyFourPlayersToResult();
    EXPECT_EQ(24U, report.clientsFinished);
    EXPECT_GT(report.datagramsDropped, 0U);
    EXPECT_GT(report.keyframeRequests, 0U);
    EXPECT_EQ(0U, report.protocolErrors);
}

TEST(GameSession, ShapedQueueOverflowIsVisibleFailure)
{
    auto session = SessionWithPausedDeliveryAndQueueLimit(256U);
    EnqueueServerDatagrams(session, 257U);
    EXPECT_EQ(GameSessionState::ProtocolError, session.State());
    EXPECT_EQ(1U, session.Metrics().shapedQueueOverflows);
}
```

- [ ] Step 2: Run RED

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=GameServerOptions.*:ClientOptions.*:BotClientOptions.*:GameServerIntegration.*:GameSession.*
```

Expected: CLI values and delayed adapters do not exist.

- [ ] Step 3: Add shared option validation and queues

Parse the exact CLI values from the spec. All zero means disabled. Any nonzero impairment requires nonzero network seed. Loss must be at most 10000 basis points, latency and jitter at most 5000ms.

Each UDP adapter increments an ordinal per peer and direction, calls `Decide`, records drops, and otherwise schedules delivery on its existing executor. Keep at most 256 delayed datagrams per peer. Equal delivery time uses ordinal order. `Stop()` cancels timers and releases queued bytes before thread join.

Do not shape TCP, worker control or lobby traffic. Do not decode before the inbound delay is applied.

- [ ] Step 4: Run impairment integration repeatedly

Run:

```powershell
./scripts/build.ps1
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=DatagramShaper.*:GameServerIntegration.*Impairment*:GameSession.*Shaped* `
  --gtest_repeat=10 `
  --gtest_break_on_failure
```

Expected: same seed produces same drop counters and results in every run, no raw bytes or token enters output and shutdown has no pending timer failure.

- [ ] Step 5: Commit

```powershell
git add -- apps/game_server apps/game_client apps/bot_client apps/client tests
git commit -m "test(network): 지연과 손실 복구 경로 고정" -m "이유: ACK delta가 100ms RTT, 2% loss와 10ms jitter에서 실제 keyframe 복구로 끝나는지 증명해야 했다." -m "핵심 변경: client와 server UDP shaping queue, shared CLI profile, counters와 24인 impairment integration을 추가했다." -m "검증: adapter 부재 RED 뒤 impairment 경로를 10회 반복 통과했다."
```

---

### Task 18: 세 경기와 soak load runner

Files:

- Modify: `scripts/network_load_common.ps1`
- Modify: `scripts/run_network_load.ps1`
- Modify: `tests/network_load_runner_test.ps1`
- Modify: `apps/game_server/src/main.cpp`
- Modify: `apps/bot_client/src/main.cpp`
- Modify: `apps/client/src/main.cpp`

Interfaces:

- Consumes: all four replication modes, metrics, process result and impairment options.
- Produces: staged one-match comparison, production three-match gate, Windows soak and Docker Linux ASan command.

- [ ] Step 1: Write failing report aggregation tests

In the PowerShell test, create three fake match directories with controlled metrics. Assert that the runner:

1. rejects different commit SHA, seed count or participant count,
2. calculates nearest-rank P95 from raw rows,
3. divides game bytes by measurement seconds and 1024,
4. reports participant average and per-recipient P95 separately,
5. detects monotonically increasing working set over the configured window,
6. refuses to write RESULT.md when any match exit code is nonzero.

- [ ] Step 2: Run RED

Run:

```powershell
powershell -NoProfile -File tests/network_load_runner_test.ps1 `
  -RepositoryRoot .
```

Expected: three-match and soak aggregation functions do not exist.

- [ ] Step 3: Extend runner modes without hiding failures

Allow all four replication modes, Matches 1 through 3 and `-Impairment`. Add `-SoakMinutes 30`, which repeats sequential matches until the measured time is at least 30 minutes and records process working set every 10 seconds.

The runner writes one child directory per match and a parent summary. It fails if room, match or result IDs disagree, any of 24 clients fails, any protocol or shaped queue error occurs, raw counts do not sum, secret scan is nonzero or memory increases in every sample across the last 15 minutes.

Add a Docker command file to evidence for the Linux ASan headless 24-session 30-minute run. Docker output must have zero AddressSanitizer, UndefinedBehaviorSanitizer, leak and fatal markers.

- [ ] Step 4: Run fake guard tests and a short real smoke

Run:

```powershell
powershell -NoProfile -File tests/network_load_runner_test.ps1 `
  -RepositoryRoot .

./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=NetworkLoadVertical.*:GameServerIntegration.*Impairment*
```

Expected: report calculations, memory guard and real short path pass.

- [ ] Step 5: Commit

```powershell
git add -- scripts/network_load_common.ps1 scripts/run_network_load.ps1 tests/network_load_runner_test.ps1 apps/game_server/src/main.cpp apps/bot_client/src/main.cpp apps/client/src/main.cpp
git commit -m "feat(benchmark): 24인 세 경기와 soak runner 추가" -m "이유: 단계별 replication 비교, production 세 경기와 30분 안정성을 한 명령으로 재현해야 했다." -m "핵심 변경: multi-match aggregation, impairment, working set guard와 Linux ASan command 기록을 추가했다." -m "검증: report RED 뒤 fake evidence guard와 짧은 실제 24인 경로를 통과했다."
```

---

### Task 19: 공식 replication 비교와 안정성 evidence

Files:

- Create: four or more exact run directories under `docs/benchmarks/network-load/`
- Modify: none outside generated evidence for this task

Interfaces:

- Consumes: final clean Release commit from Task 18.
- Produces: comparable raw evidence for all stages, final three-match, impairment and soak gates.

- [ ] Step 1: Run final local build gates

Run:

```powershell
./scripts/build.ps1
./scripts/test.ps1
./scripts/build.ps1 -Preset windows-msvc-release
git status --short
```

Expected: Windows Debug tests including WARP and runner guard pass, Release builds and status is empty.

- [ ] Step 2: Run the four comparable one-match stages

Run each command with the same HEAD and seed 20260825:

```powershell
$sha = git rev-parse HEAD
foreach ($mode in @(
    'full-state',
    'interest-full',
    'interest-quantized',
    'interest-delta')) {
    ./scripts/run_network_load.ps1 `
      -ReplicationMode $mode `
      -Matches 1 `
      -Seeds 20260825 `
      -CommitSha $sha `
      -Release
}
```

Expected: all four matches have the same result for the locked seed. Do not interpret improvement until raw consistency passes.

- [ ] Step 3: Run final production three-match and impairment gates

```powershell
$sha = git rev-parse HEAD
./scripts/run_network_load.ps1 `
  -ReplicationMode interest-delta `
  -Matches 3 `
  -Seeds 20260825,20260826,20260827 `
  -CommitSha $sha `
  -Release

./scripts/run_network_load.ps1 `
  -ReplicationMode interest-delta `
  -Matches 1 `
  -Seeds 20260825 `
  -CommitSha $sha `
  -Impairment `
  -Release
```

Expected: every match finishes with 24 clients, result parity and no protocol error. Record actual elapsed time.

- [ ] Step 4: Run Windows and Linux 30-minute soak

```powershell
$sha = git rev-parse HEAD
./scripts/run_network_load.ps1 `
  -ReplicationMode interest-delta `
  -Matches 3 `
  -Seeds 20260825,20260826,20260827 `
  -CommitSha $sha `
  -Impairment `
  -SoakMinutes 30 `
  -Release
```

Then run the generated Docker Ubuntu 24.04 ASan command. Stop and diagnose any crash, sanitizer marker, leak, queue overflow or monotonic memory guard before committing evidence.

- [ ] Step 5: Compare raw metrics and write only observed claims

Generate each RESULT.md from raw rows. The comparison must include payload bytes, datagram bytes, fragment count, visible counts, server tick P95, replication encode P95, average client KiB/s, recipient P95, keyframe recovery and working set.

If 64KiB/s or 33.3ms targets fail, state the actual value and bottleneck. Do not remove a mode because it regressed server CPU.

- [ ] Step 6: Commit immutable evidence

```powershell
git add -- docs/benchmarks/network-load
git commit -m "bench(network): 24인 replication 비교 기록" -m "이유: full-state, 관심 영역, 양자화와 ACK delta의 실제 server 비용 및 client traffic을 같은 조건에서 비교해야 했다." -m "핵심 변경: 단계별 one-match, final three-match, impairment와 soak raw evidence 및 결과 문서를 추가했다." -m "검증: commit, seed, participant, result, raw 합계, target, sanitizer와 secret scan을 대조했다."
```

---

### Task 20: ADR, 개발 기록, full review와 PR

Files:

- Create: `docs/adr/0008-acked-interest-replication.md`
- Create: `docs/devlog/2026-08-25-24-player-network-load.md`
- Modify: `README.md`
- Modify: `docs/PROJECT_PLAN.md`

Interfaces:

- Consumes: final code diff, immutable evidence, local and hosted CI results.
- Produces: truthful Week 10 record and merge-ready PR, without merging.

- [ ] Step 1: Review the full Week 10 diff locally

Review from merge commit:

```text
38547c89b751a54256fea245b5f68ead1a48e547
```

Inspect protocol allocation bounds, version direction, future and stale ACK, baseline ring eviction, local actor visibility, enter and leave ordering, quantization error, delta reconstruction, queue and timer lifetime, shared runtime shutdown, 24-session report aggregation, byte accounting, metric percentile calculation, secret exclusion, Windows and Linux guards.

This review stays in the current session without subagents. For each reproduced defect, retain a focused failing test, make one fix and commit it separately with `fix(review):`. Do not create findings or commits for unverified suspicions.

- [ ] Step 2: Write ADR 0008

Record why the design uses recipient ACK baseline instead of unacknowledged delta, why AOI keyframes are recipient-full rather than global-full, why one bot process shares a runtime, why application impairment is UDP-only and why baseline memory is capped at 32.

- [ ] Step 3: Write the development record from actual evidence

Use this order:

```text
증상
재현
가설
비교한 대안
구현
결과
남은 한계
```

Include the full-state baseline, every staged mode, server CPU trade-off, final three matches, impairment, soak and any target miss. Do not call a fast fixture a production result.

- [ ] Step 4: Update README and project status

README adds one-process 23 bot load commands, replication mode commands, evidence links and billing-CI caveat. `PROJECT_PLAN.md` marks Week 10 complete only after all non-billing gates pass and sets Week 11 as next. If a required non-billing gate fails, keep Week 10 in progress.

- [ ] Step 5: Commit records

```powershell
git add -- README.md docs/PROJECT_PLAN.md docs/adr/0008-acked-interest-replication.md docs/devlog/2026-08-25-24-player-network-load.md
git commit -m "docs(network): 10주차 부하 최적화 기록" -m "이유: 24인 기준선, 단계별 선택, 비용과 남은 한계를 interview에서 재현 가능한 형태로 남겨야 했다." -m "핵심 변경: ADR, 개발 기록, 실행 명령, evidence link와 프로젝트 상태를 실제 결과에 맞춰 갱신했다." -m "검증: raw 수치, SHA, seed, command, target, sanitizer, secret와 문서 link를 대조했다."
```

- [ ] Step 6: Run final verification

```powershell
./scripts/build.ps1
./scripts/test.ps1
./scripts/build.ps1 -Preset windows-msvc-release
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=NetworkLoadVertical.*:GameServerIntegration.*Impairment*:ClientSnapshotStream.*:SnapshotReplicator.* `
  --gtest_repeat=3 `
  --gtest_break_on_failure
```

Run Docker Linux build, CTest and ASan focused integration. Verify clean status and compare local HEAD with origin branch after push.

- [ ] Step 7: Push and open the Week 10 PR

Push `feat/24-player-load-optimization` and open a PR against `main`. Fill every section of `.github/PULL_REQUEST_TEMPLATE.md`. Separate completed behavior, measured evidence, target misses, billing-blocked hosted CI and Week 11 deferred work.

Monitor GitHub checks. If billing still prevents runner allocation, do not rerun the same failed run repeatedly. Record one fresh-run annotation and rely on the explicitly separated local Windows plus Docker Linux evidence until the user decides whether to merge. Do not merge without a new direct instruction.

---

## Expected Commit Sequence

1. `feat(protocol): snapshot ACK와 replication mode 추가`
2. `refactor(client): shared game network runtime 추가`
3. `feat(client): 23개 play bot session 추가`
4. `test(network): 24인 game session 수직 경로 고정`
5. `feat(network): game traffic byte 계측 추가`
6. `feat(server): match tick과 replication metrics 추가`
7. `feat(benchmark): full-state network 기준선 runner 추가`
8. `bench(network): full-state 24인 기준선 기록`
9. `feat(protocol): quantized replication value 추가`
10. `feat(protocol): replication snapshot codec 추가`
11. `feat(server): recipient 관심 영역 grid 추가`
12. `feat(server): 관심 영역 keyframe replication 추가`
13. `feat(server): ACK 기반 delta replication 추가`
14. `feat(client): delta snapshot stream 적용`
15. `test(network): optimized snapshot 수직 경로 고정`
16. `feat(network): 결정적 UDP datagram shaper 추가`
17. `test(network): 지연과 손실 복구 경로 고정`
18. `feat(benchmark): 24인 세 경기와 soak runner 추가`
19. `bench(network): 24인 replication 비교 기록`
20. `docs(network): 10주차 부하 최적화 기록`

Review와 CI fix commit은 실제 실패를 재현한 경우에만 추가한다. commit count는 목표가 아니며 날짜 조작, synthetic incident와 synthetic metric을 만들지 않는다.

## Execution Checkpoints

- Checkpoint A after Task 4: shared runtime의 24 actual sessions가 full-state short match를 완주한다.
- Checkpoint B after Task 8: clean Release full-state production baseline이 immutable evidence로 잠긴다.
- Checkpoint C after Task 12: full-state, interest-full과 interest-quantized keyframe mode가 같은 result를 만든다.
- Checkpoint D after Task 15: ACK delta가 actual 24-session TCP와 UDP 경로에서 복구되고 result parity를 지킨다.
- Checkpoint E after Task 17: deterministic impairment 경로가 100ms RTT, 2% loss와 10ms jitter에서 반복된다.
- Checkpoint F after Task 19: production 세 경기와 Windows 및 Linux soak evidence가 완전하다.
- Checkpoint G after Task 20: local review와 PR이 merge-ready evidence를 가지며 merge는 사용자 지시를 기다린다.

## Spec Coverage Check

- One process with 23 sessions: Tasks 2, 3 and 4
- Protocol v2 and SnapshotId ACK: Tasks 1, 10, 13, 14 and 15
- Full-state baseline before optimization: Tasks 5 through 8
- Interest grid and hysteresis: Tasks 11, 12 and 15
- Quantization and field bitset: Tasks 9, 10, 12 and 13
- Recipient baseline ring and periodic keyframe: Tasks 13 through 15
- Deterministic UDP impairment: Tasks 16 and 17
- Three production matches and 30-minute soak: Tasks 18 and 19
- Metrics, immutable evidence and target handling: Tasks 5 through 8, 18 and 19
- ADR, devlog, README, review and PR: Task 20

