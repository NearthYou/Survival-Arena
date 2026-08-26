# DX11 Survival Arena

C++20과 DirectX 11로 만드는 쿼터뷰 생존 아레나 포트폴리오다. 렌더링 엔진과 게임 클라이언트를 중심에 두고, 24인 방과 권위형 게임 서버를 같은 저장소에서 검증한다.

현재 단계는 11주차 Linux server image와 단일 host worker pool까지 구현된 상태다. lobby가 capacity 1 worker에 경기를 예약하고 ready 응답을 받은 뒤에만 참가자 ticket을 전달한다. 실제 DX11 client 1개와 같은 `GameSession`을 사용하는 play bot 23개가 game TCP 인증, UDP bind, 30Hz input, 15Hz snapshot과 경기 결과까지 진행한다.

Windows WARP 수직 테스트에서는 DX11 client와 bot 23개가 60 tick에 같은 결과를 확인한다. 이 fixture는 production 성능 수치로 사용하지 않는다. 공식 Release 한 경기에서 full-state 평균 수신량은 66.216564KiB/s로 64KiB/s 목표를 넘었고, ACK 기반 interest-delta는 4.123043KiB/s였다. 같은 seed의 네 mode는 winner 4, tick 17,430 결과를 유지했다.

100ms RTT, 2% loss와 10ms jitter를 적용한 production 경기에서는 datagram 12,579개가 drop되고 keyframe 요청 3,875회가 발생했지만 protocol error와 queue overflow 없이 끝났다. Windows soak는 네 경기와 실제 match 시간 2,324.07초, Linux ASan과 UBSan은 24인 headless 경기 1,817회를 기록했다. 진행 상태와 검증 결과는 [프로젝트 계획](docs/PROJECT_PLAN.md)과 [24인 network 기록](docs/devlog/2026-08-25-24-player-network-load.md)에 남긴다.

## 원칙

- 다른 프로젝트의 코드와 리소스를 복사하지 않는다.
- 단순한 기준 구현을 먼저 측정하고, 개선 전후 수치가 확인된 경우에만 최적화 사례로 기록한다.
- 클라이언트와 서버가 공유하는 코드는 플랫폼 중립 모듈에 둔다.
- 커밋 본문에 변경 이유와 검증 명령을 남긴다.

## 로컬 준비

필요한 기본 환경은 Visual Studio 2022 C++ 도구, Windows SDK, Git LFS다. 저장소 루트에서 다음 명령으로 구성한다.

```powershell
./scripts/bootstrap.ps1
./scripts/build.ps1
./scripts/test.ps1
```

Linux headless target은 Ubuntu 24.04 GCC `-Werror`, ASan과 UBSan Docker 검증을 통과했다. 배포 image는 같은 고정 vcpkg baseline으로 GCC 13 Release `-Werror` build를 수행한다.

## Linux lobby와 game worker 2개 실행

Docker Desktop이 실행 중이면 다음 smoke runner가 image build, lobby 1개와 game worker 2개의 health 및 registration, cleanup을 한 번에 확인한다.

```powershell
./scripts/test_server_compose.ps1
```

고정 port로 직접 실행하거나 외부 접속용 host를 지정하는 방법은 [Linux 서버 컨테이너 실행](deploy/README.md)에 있다. AWS resource를 만들기 전 확인할 계정, 비용과 security group 경계는 [AWS 확인표](deploy/AWS_PRECHECK.md)에 분리했다.

## 네 process로 2인 network 경기 실행

먼저 Debug build를 만든다.

```powershell
./scripts/build.ps1
```

첫 번째 터미널에서 lobby server를 실행한다. client용 TCP는 7000, worker control TCP는 7001을 사용한다.

```powershell
./out/build/windows-msvc-vs-debug/apps/lobby_server/Debug/dxa_lobby_server.exe `
  --bind 127.0.0.1 `
  --port 7000 `
  --worker-bind 127.0.0.1 `
  --worker-port 7001
```

두 번째 터미널에서 game server를 실행한다. worker가 lobby control endpoint에 등록되고 game TCP 7100, UDP 7101에서 기다린다.

```powershell
./out/build/windows-msvc-vs-debug/apps/game_server/Debug/dxa_game_server.exe `
  --lobby-control-host 127.0.0.1 `
  --lobby-control-port 7001 `
  --worker-id 1 `
  --advertise-host 127.0.0.1 `
  --game-bind 127.0.0.1 `
  --game-tcp-port 7100 `
  --game-udp-port 7101 `
  --replication-mode interest-delta
```

세 번째 터미널에서 hardware DX11 client를 실행한다. client가 방을 만들고 ready를 켠 뒤 `network room=<ID>`를 출력한다.

```powershell
./out/build/windows-msvc-vs-debug/apps/client/Debug/dxa_client.exe `
  --render-path hybrid-deferred `
  --network-create `
  --replication-mode interest-delta `
  --expected-players 2 `
  --lobby-host 127.0.0.1 `
  --lobby-port 7000
```

네 번째 터미널에서 출력된 실제 RoomId로 play bot 하나를 넣는다. 새 lobby process의 첫 방은 보통 1이지만 출력값을 우선한다.

```powershell
./out/build/windows-msvc-vs-debug/apps/bot_client/Debug/dxa_bot_client.exe `
  --host 127.0.0.1 `
  --port 7000 `
  --room 1 `
  --count 1 `
  --play
```

두 process가 같은 MatchId와 `state=synchronized`를 출력하면 실제 game TCP 인증과 UDP snapshot 수신이 끝난 상태다. 창에서 지면을 우클릭하면 local destination을 보내며 server가 NavMesh에서 다시 검증한다. bot의 game connection을 종료하면 다음 server tick에 탈락 처리되고 DX11 client에 최후 생존 결과가 전달된다.

## 24인 부하와 replication 비교

Release binary를 만든 뒤 clean commit SHA로 runner를 실행한다. bot process 하나가 내부 `GameSession` 23개와 network runtime 하나를 공유한다.

```powershell
./scripts/build.ps1 -Preset windows-msvc-release
$sha = git rev-parse HEAD

./scripts/run_network_load.ps1 `
  -ReplicationMode interest-delta `
  -Matches 3 `
  -Seeds 20260825,20260826,20260827 `
  -CommitSha $sha `
  -Release
```

`-ReplicationMode`은 `full-state`, `interest-full`, `interest-quantized`, `interest-delta`를 받는다. network 장애를 재현하려면 `-Impairment`를 붙인다. 30분 이상 반복과 working set guard까지 실행하려면 `-SoakMinutes 30`을 추가한다.

```powershell
./scripts/run_network_load.ps1 `
  -ReplicationMode interest-delta `
  -Matches 3 `
  -Seeds 20260825,20260826,20260827 `
  -CommitSha $sha `
  -Impairment `
  -SoakMinutes 30 `
  -Release
```

runner는 match마다 child evidence를 만들고 parent `summary.json`과 `RESULT.md`를 raw CSV에서 계산한다. 1MB가 넘는 CSV와 log는 Git LFS에 저장한다. clone 뒤 원본 evidence가 필요하면 `git lfs pull`을 실행한다.

공식 비교와 target 판정은 [24인 replication 비교](docs/benchmarks/network-load/20260826-01ae1278-COMPARISON.md)에 있다. full-state의 64KiB/s 목표 미달과 delta encode 비용 증가도 같은 문서에 남겼다.

모든 binary bind 기본값은 `127.0.0.1`이다. Compose는 명시적으로 `0.0.0.0`에 bind하되 worker control 7001/TCP를 host에 publish하지 않는다. game TCP와 UDP는 암호화되지 않았고 worker control에도 외부 network용 상호 인증이 없다. 짧은 demo 검증 외 장기 public 운영에는 사용하지 않는다. ticket과 UDP token은 console과 log에 출력하지 않는다.

렌더 경로만 짧게 확인하려면 다음 명령을 사용한다.

```powershell
./out/build/windows-msvc-vs-debug/apps/client/Debug/dxa_client.exe --warp --hidden --no-vsync --frames 3 --verify-render --verify-asset-scene
```

하이브리드 경로는 `--render-path`로 선택한다.

```powershell
./out/build/windows-msvc-vs-debug/apps/client/Debug/dxa_client.exe --warp --hidden --no-vsync --frames 3 --verify-render --verify-asset-scene --render-path hybrid-deferred
```

NavMesh 이동 수직 기능은 별도 데모에서 확인한다. 창을 띄운 실행에서는 지면을 우클릭하고, 자동 검증에서는 같은 `NavAgent` 경로를 60Hz로 진행한다.

```powershell
./out/build/windows-msvc-vs-debug/apps/navigation_demo/Debug/dxa_navigation_demo.exe

./out/build/windows-msvc-vs-debug/apps/navigation_demo/Debug/dxa_navigation_demo.exe --warp --hidden --frames 120 --auto-destination 20 10 --verify-render
```

오프라인 경기는 별도 demo에서 실행한다. 창을 띄운 실행에서는 지면을 우클릭해 actor 0을 움직일 수 있다. WARP 자동 실행은 같은 공개 command 경계를 사용해 한 경기를 끝내고 결과 frame과 checksum을 검증한다.

```powershell
./out/build/windows-msvc-vs-debug/apps/offline_match_demo/Debug/dxa_offline_match_demo.exe

./out/build/windows-msvc-vs-debug/apps/offline_match_demo/Debug/dxa_offline_match_demo.exe --warp --hidden --auto-match --verify-match
```

원본 에셋을 다시 변환하려면 다음 명령을 사용한다.

```powershell
./out/build/windows-msvc-vs-debug/apps/asset_tool/Debug/dxa_asset_tool.exe model --input Character.fbx --output cyber-runner.dxam --sample-rate 30
./out/build/windows-msvc-vs-debug/apps/asset_tool/Debug/dxa_asset_tool.exe texture --input colormap.png --output colormap.dds
```

벤치마크는 깨끗한 commit에서만 실행된다. 인자를 생략하면 포워드 경로를 측정한다.

```powershell
./scripts/run_benchmark.ps1
```

공간 탐색과 AI 비교는 별도 Release runner로 실행한다. 결과 동등성 검사를 통과한 뒤에만 5회 시간 sample을 기록한다.

```powershell
./scripts/run_simulation_benchmark.ps1
```

오프라인 경기 runner는 깨끗한 commit에서 같은 seed 경기를 반복 검증한 뒤 세 번째 경기의 `OfflineMatch::Step()` 시간을 기록한다.

```powershell
./scripts/run_offline_match_benchmark.ps1
```

하이브리드 측정과 잠긴 포워드 원본 비교는 다음 순서로 실행한다.

```powershell
./scripts/run_benchmark.ps1 -RenderPath hybrid-deferred

./scripts/compare_benchmarks.ps1 `
  -ForwardRun docs/benchmarks/forward-baseline/20260823-033736-80988ef7-seed20260823 `
  -HybridRun docs/benchmarks/hybrid-deferred/20260823-145749-54a54e5c-seed20260823
```

첫 프레임에서 확인한 실패와 경계는 [첫 DX11 프레임 기록](docs/devlog/2026-08-22-first-dx11-frame.md)에, 에셋 파이프라인에서 확인한 문제는 [에셋 파이프라인 기록](docs/devlog/2026-08-23-asset-pipeline.md)에 적었다. GPU query 302개 누락 과정은 [포워드 기준선 기록](docs/devlog/2026-08-23-forward-baseline.md)에, 2,240 draw를 줄인 과정은 [하이브리드 디퍼드 기록](docs/devlog/2026-08-23-hybrid-deferred.md)에 남겼다. 전수 탐색과 가속 구조를 같은 결과로 맞춘 과정은 [공간 탐색과 AI 기록](docs/devlog/2026-08-23-spatial-navigation-ai.md)에 정리했다. 3초 만에 끝난 첫 경기를 규칙 수치 조작 없이 재설계한 과정과 측정 경계는 [오프라인 경기 기록](docs/devlog/2026-08-24-offline-match-loop.md)과 [공식 Release 원본](docs/benchmarks/offline-match/20260824-023134-1ede6a23-seed20260823/RESULT.md)에서 확인할 수 있다. 로비 domain, TCP session, 24인 실제 socket 검증 과정은 [로비와 방 기록](docs/devlog/2026-08-24-lobby-room-flow.md)과 [ADR 0006](docs/adr/0006-lobby-domain-and-tcp-adapter.md)에 남겼다. 실제 worker 예약부터 DX11 result까지 이어진 과정은 [권위형 게임 서버 기록](docs/devlog/2026-08-24-authoritative-game-server.md)과 [ADR 0007](docs/adr/0007-authoritative-game-session.md)에 정리했다. ACK baseline, 관심 영역과 impairment 선택은 [ADR 0008](docs/adr/0008-acked-interest-replication.md)에, 24인 기준선부터 soak까지의 시행착오는 [10주차 개발 기록](docs/devlog/2026-08-25-24-player-network-load.md)에 남겼다. Linux Release 경고와 두 worker container 경계는 [11주차 개발 기록](docs/devlog/2026-08-26-linux-server-packaging.md)과 [ADR 0009](docs/adr/0009-single-host-compose-worker-pool.md)에 정리했다.

## CI 상태

local Windows 전체 CTest, WARP와 shader 배포 검사, Ubuntu 24.04 Docker GCC build와 sanitizer 검증을 완료했다. 11주차에는 GCC 13 Release server image와 세 container Compose smoke를 추가했다. 이 branch의 GitHub hosted CI는 PR을 올린 뒤 별도로 확인한다. runner allocation이 billing 문제로 시작되지 않으면 같은 실패를 반복 실행하지 않고 local 및 Docker evidence와 hosted CI 미실행 상태를 분리해 기록한다.

## 라이선스

직접 작성한 코드는 [MIT License](LICENSE)를 따른다. 외부 자산과 라이브러리는 각각의 라이선스를 따르며, 사용 시 `THIRD_PARTY_ASSETS.md`와 dependency manifest에 출처를 기록한다.
