# DX11 Survival Arena

C++20과 DirectX 11로 만드는 쿼터뷰 생존 아레나 포트폴리오다. 렌더링 엔진과 게임 클라이언트를 중심에 두고, 24인 방과 권위형 게임 서버를 같은 저장소에서 검증한다.

현재 단계는 5주차 하이브리드 디퍼드와 인스턴싱까지 구현된 상태다. 고정 seed 장면에 캐릭터 24명, AI 100개, 정적 객체 1,000개와 동적 광원 32개를 배치하고 포워드와 하이브리드 경로를 같은 camera path로 비교한다. RTX 3050 Ti의 1920×1080 Release 측정에서 GPU P95는 3.885056ms에서 2.174976ms, draw call P50은 2,240회에서 1,148회로 줄었다. 게임 로직과 네트워크는 아직 없다. 진행 상태와 검증 결과는 [프로젝트 계획](docs/PROJECT_PLAN.md)과 `docs/devlog/`에 남긴다.

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

원본 에셋을 다시 변환하려면 다음 명령을 사용한다.

```powershell
./out/build/windows-msvc-vs-debug/apps/asset_tool/Debug/dxa_asset_tool.exe model --input Character.fbx --output cyber-runner.dxam --sample-rate 30
./out/build/windows-msvc-vs-debug/apps/asset_tool/Debug/dxa_asset_tool.exe texture --input colormap.png --output colormap.dds
```

벤치마크는 깨끗한 commit에서만 실행된다. 인자를 생략하면 포워드 경로를 측정한다.

```powershell
./scripts/run_benchmark.ps1
```

하이브리드 측정과 잠긴 포워드 원본 비교는 다음 순서로 실행한다.

```powershell
./scripts/run_benchmark.ps1 -RenderPath hybrid-deferred

./scripts/compare_benchmarks.ps1 `
  -ForwardRun docs/benchmarks/forward-baseline/20260823-033736-80988ef7-seed20260823 `
  -HybridRun docs/benchmarks/hybrid-deferred/20260823-145749-54a54e5c-seed20260823
```

첫 프레임에서 확인한 실패와 경계는 [첫 DX11 프레임 기록](docs/devlog/2026-08-22-first-dx11-frame.md)에, 에셋 파이프라인에서 확인한 문제는 [에셋 파이프라인 기록](docs/devlog/2026-08-23-asset-pipeline.md)에 적었다. GPU query 302개 누락 과정은 [포워드 기준선 기록](docs/devlog/2026-08-23-forward-baseline.md)에, 2,240 draw를 줄인 과정은 [하이브리드 디퍼드 기록](docs/devlog/2026-08-23-hybrid-deferred.md)에 남겼다.

## 라이선스

직접 작성한 코드는 [MIT License](LICENSE)를 따른다. 외부 자산과 라이브러리는 각각의 라이선스를 따르며, 사용 시 `THIRD_PARTY_ASSETS.md`와 dependency manifest에 출처를 기록한다.
