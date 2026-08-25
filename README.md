# DX11 Survival Arena

C++20과 DirectX 11로 만드는 쿼터뷰 생존 아레나 포트폴리오다. 렌더링 엔진과 게임 클라이언트를 중심에 두고, 24인 방과 권위형 게임 서버를 같은 저장소에서 검증한다.

현재 단계는 9주차 권위형 게임 서버와 클라이언트 보정까지 구현된 상태다. lobby가 capacity 1 worker에 경기를 예약하고 ready 응답을 받은 뒤에만 참가자 ticket을 전달한다. 실제 DX11 client 1개와 같은 `GameSession`을 사용하는 play bot 1개가 game TCP 인증, UDP bind, 30Hz input, 15Hz snapshot과 disconnect 결과까지 loopback에서 진행한다.

Windows WARP 수직 테스트에서는 두 client가 RoomId 1과 MatchId 1을 공유하고 각각 snapshot 29개를 받은 뒤 60 tick에 같은 결과를 확인했다. 마지막 반복 시간은 3,011ms였지만 짧은 test 설정의 기능 검증값이며 production 경기 성능이 아니다. 수동 hardware 검증에서는 NVIDIA GeForce RTX 3050 Ti Laptop GPU에서 두 client가 snapshot을 받은 뒤 bot 연결 종료가 winner 0, tick 2,789 결과로 이어졌고 DX11 debug layer error 0을 확인했다.

24인 로비 계약은 유지되지만 `--play` bot은 9주차에 한 개만 허용한다. 실제 DX11 client 1개와 play bot 23개의 부하, 관심 영역과 snapshot 대역폭 최적화는 10주차 범위다. 진행 상태와 검증 결과는 [프로젝트 계획](docs/PROJECT_PLAN.md)과 `docs/devlog/`에 남긴다.

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

Linux 서버 빌드는 이후 마일스톤에서 Docker와 CI로 함께 검증한다.

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
  --game-udp-port 7101
```

세 번째 터미널에서 hardware DX11 client를 실행한다. client가 방을 만들고 ready를 켠 뒤 `network room=<ID>`를 출력한다.

```powershell
./out/build/windows-msvc-vs-debug/apps/client/Debug/dxa_client.exe `
  --render-path hybrid-deferred `
  --network-create `
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

모든 bind 기본값은 `127.0.0.1`이다. game TCP와 UDP는 암호화되지 않았고 worker control에도 외부 network용 상호 인증이 없다. 11주차 배포 보안 경계를 정하기 전에는 public interface에 노출하지 않는다. ticket과 UDP token은 console과 log에 출력하지 않는다.

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

첫 프레임에서 확인한 실패와 경계는 [첫 DX11 프레임 기록](docs/devlog/2026-08-22-first-dx11-frame.md)에, 에셋 파이프라인에서 확인한 문제는 [에셋 파이프라인 기록](docs/devlog/2026-08-23-asset-pipeline.md)에 적었다. GPU query 302개 누락 과정은 [포워드 기준선 기록](docs/devlog/2026-08-23-forward-baseline.md)에, 2,240 draw를 줄인 과정은 [하이브리드 디퍼드 기록](docs/devlog/2026-08-23-hybrid-deferred.md)에 남겼다. 전수 탐색과 가속 구조를 같은 결과로 맞춘 과정은 [공간 탐색과 AI 기록](docs/devlog/2026-08-23-spatial-navigation-ai.md)에 정리했다. 3초 만에 끝난 첫 경기를 규칙 수치 조작 없이 재설계한 과정과 측정 경계는 [오프라인 경기 기록](docs/devlog/2026-08-24-offline-match-loop.md)과 [공식 Release 원본](docs/benchmarks/offline-match/20260824-023134-1ede6a23-seed20260823/RESULT.md)에서 확인할 수 있다. 로비 domain, TCP session, 24인 실제 socket 검증 과정은 [로비와 방 기록](docs/devlog/2026-08-24-lobby-room-flow.md)과 [ADR 0006](docs/adr/0006-lobby-domain-and-tcp-adapter.md)에 남겼다. 실제 worker 예약부터 DX11 result까지 이어진 과정은 [권위형 게임 서버 기록](docs/devlog/2026-08-24-authoritative-game-server.md)과 [ADR 0007](docs/adr/0007-authoritative-game-session.md)에 정리했다.

## 라이선스

직접 작성한 코드는 [MIT License](LICENSE)를 따른다. 외부 자산과 라이브러리는 각각의 라이선스를 따르며, 사용 시 `THIRD_PARTY_ASSETS.md`와 dependency manifest에 출처를 기록한다.
