# 실험용 클라이언트 상태

상태: 미완료, 원본 게임 실행 경로와 분리. 실험용 gameplay 구현의 개발 브랜치는 main에 병합하지 않는다.

아래 기록은 2026-09-06 로컬 작업트리의 실험용 구현에 대한 것이다. 해당 구현 변경은 이번 원본 실행 경로 커밋에 포함하지 않고 그대로 보존한다. 이 문서는 미완료 범위를 명시하는 기록이며, 커밋된 원본 게임 실행 기능의 검증 결과가 아니다.

아래 `dxa_client`는 원본 게임 실행 타깃과 다른 독립 실험 경로다. 원본 전체 게임은 [원본 게임 실행](reference-game-runtime.md)의 `play_reference_game`으로 실행한다. 이 실험 경로는 인자 없이 실행하면 hybrid deferred gameplay 장면을 열며, 우클릭 지면 이동, 좌클릭 monster 선택과 기본 공격, 마우스 휠 확대와 축소, Q/W/E/R skill, F loot 상호작용을 사용한다.

```powershell
./out/build/windows-msvc-vs-debug/apps/client/Debug/dxa_client.exe
```

이 경로의 자동 검증은 wall clock과 무관하게 render frame마다 30Hz fixed tick 하나를 진행한다. 아래 명령은 300 frame 시나리오와 증거 출력을 요청한다. 2026-09-06 비앙카의 이동 목표 갱신과 니키의 자동 입력 순서를 수정했고, R 캡처는 실제 처치와 스킬 학습 시간을 반영해 100틱에서 170틱으로 갱신했다. 게임 수치는 변경하지 않았다. 전체 실행에서 C++ 테스트 1,142개가 통과했고, 외부 리소스 경로를 연결해 재실행한 나머지 13개도 모두 통과했다. 합계 1,155개이며 실패는 0개다. 숨김 WARP 300 frame 실행은 검증 항목 17개 통과, 그래픽 오류 0개다. 증거는 `out/experimental-script-final-regression.xml`, `out/experimental-script-external-regression.xml`, `out/experimental-script-final-runtime/`에 있다.

기존 100틱 외부 visual manifest는 다시 생성해야 하며, 새 R 캡처의 시각 승인은 아직 받지 않았다. 위 결과는 실험 경로의 자동 검증이며 원본 게임과의 일치를 뜻하지 않는다. 승인된 원본 실행판은 변경하지 않았다.

2026-09-06 실제 외부 맵 검증에서는 R 시각 승인을 거부했다. 기존 R 기준 이미지는 원본 게임이 아닌 과거 실험 클라이언트 화면이었고, 캡처 경로도 현재 NavMesh 밖에 있었다. 별도 진단 복사본에서 원본 실행의 R 이미지를 연결하고 경로를 실제 시작 위치로 바꾼 뒤에도 지형 깊이 채움 비율 0.323504, 최대 빈 영역 비율 0.672442로 실패했다. 일반 실행의 첫 캡처는 27틱으로, 170틱 R 화면의 증거가 아니다. 실험용 경로에는 캡처 기준 재구성과 HUD 및 지형 표현 검토가 남아 있다. 원본 실행 경로의 완료 상태와 구분한다.

캡처 스크립트의 경계 검증도 통과했다. 옛 100틱 R 설정 거부를 포함한 결과는 `out/build/windows-msvc-vs-debug/out-experimental-capture-runner.xml`에 있다.

```powershell
./out/build/windows-msvc-vs-debug/apps/client/Debug/dxa_client.exe `
  --scene gameplay `
  --warp `
  --hidden `
  --no-vsync `
  --frames 300 `
  --scripted-gameplay `
  --verify-gameplay `
  --gameplay-output out/gameplay-run-001
```
