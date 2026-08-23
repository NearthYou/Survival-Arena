# DX11 Survival Arena

C++20과 DirectX 11로 만드는 쿼터뷰 생존 아레나 포트폴리오다. 렌더링 엔진과 게임 클라이언트를 중심에 두고, 24인 방과 권위형 게임 서버를 같은 저장소에서 검증한다.

현재 단계는 7주차 오프라인 경기 루프까지 구현된 상태다. 플랫폼 중립 30Hz simulation에서 참가자 24명과 중립 AI 100마리가 256×256 arena를 이동하고, loot 60개에서 무기와 회복 아이템을 얻어 전투한다. Blade, Rifle, ArcPulse, 4단계 축소 구역, 사망과 최후 생존자 판정을 연결했다. canonical seed `20260823`의 공식 Release 경기는 tick 16,147, 약 8분 58초에 winner 2로 끝났고 repeat mismatch는 0건이었다. 로비, 방, 실제 client-server 통신은 아직 구현하지 않았다. 진행 상태와 검증 결과는 [프로젝트 계획](docs/PROJECT_PLAN.md)과 `docs/devlog/`에 남긴다.

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

첫 프레임에서 확인한 실패와 경계는 [첫 DX11 프레임 기록](docs/devlog/2026-08-22-first-dx11-frame.md)에, 에셋 파이프라인에서 확인한 문제는 [에셋 파이프라인 기록](docs/devlog/2026-08-23-asset-pipeline.md)에 적었다. GPU query 302개 누락 과정은 [포워드 기준선 기록](docs/devlog/2026-08-23-forward-baseline.md)에, 2,240 draw를 줄인 과정은 [하이브리드 디퍼드 기록](docs/devlog/2026-08-23-hybrid-deferred.md)에 남겼다. 전수 탐색과 가속 구조를 같은 결과로 맞춘 과정은 [공간 탐색과 AI 기록](docs/devlog/2026-08-23-spatial-navigation-ai.md)에 정리했다. 3초 만에 끝난 첫 경기를 규칙 수치 조작 없이 재설계한 과정과 측정 경계는 [오프라인 경기 기록](docs/devlog/2026-08-24-offline-match-loop.md)과 [공식 Release 원본](docs/benchmarks/offline-match/20260824-021115-11abbe54-seed20260823/RESULT.md)에서 확인할 수 있다.

## 라이선스

직접 작성한 코드는 [MIT License](LICENSE)를 따른다. 외부 자산과 라이브러리는 각각의 라이선스를 따르며, 사용 시 `THIRD_PARTY_ASSETS.md`와 dependency manifest에 출처를 기록한다.
