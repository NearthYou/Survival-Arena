# DX11 Survival Arena 프로젝트 계획

## 목표

C++20과 DirectX 11로 쿼터뷰 생존 아레나를 만든다. 한 명은 DX11 클라이언트로 플레이하고 23개 헤드리스 봇은 같은 프로토콜로 접속한다. 로비 서버와 권위형 게임 서버를 분리해 방 생성부터 한 경기 종료까지 검증한다.

## 구현 경계

- Windows 전용 DX11 엔진과 플랫폼 중립 시뮬레이션을 분리한다.
- 서버는 엔진을 참조하지 않는다.
- TCP는 로비와 신뢰성 이벤트, UDP는 입력과 스냅샷에 사용한다.
- 방 정원은 24명, 게임 서버는 30Hz, 스냅샷은 15Hz로 동작한다.
- 코드와 리소스는 새로 만들거나 사용 조건이 확인된 자산만 사용한다.

## 마일스톤

1. 저장소와 빌드 기반
2. DX11 엔진 코어
3. 에셋과 GPU 애니메이션
4. 포워드 렌더링 기준 측정
5. 하이브리드 디퍼드와 인스턴싱
6. 공간 탐색, NavMesh, 행동 트리
7. 오프라인 경기 루프
8. 로비와 방 흐름
9. 권위형 게임 서버와 클라이언트 보정
10. 24인 부하와 관심 영역 최적화
11. Linux 서버 패키징과 외부 접속
12. README, 다이어그램, 포트폴리오 PDF, 공개 검증

## 진행 현황

- 완료: 1주차 저장소 기반
- 완료: 2주차 DX11 엔진 코어
- 완료: 3주차 에셋과 GPU 애니메이션
- 완료: 4주차 포워드 렌더링 기준 측정
- 완료: 5주차 하이브리드 디퍼드와 인스턴싱
- 완료: 6주차 공간 탐색, NavMesh, 행동 트리
- 완료: 7주차 오프라인 경기 루프
- 완료: 8주차 로비와 방 흐름
- 완료: 9주차 권위형 게임 서버와 클라이언트 보정
- 완료: 10주차 24인 부하와 관심 영역 최적화
- 완료: 11주차 local Linux 서버 패키징과 Compose worker pool
- 미실행: 11주차 AWS 외부 접속
- 완료: 12주차 포트폴리오 기반 문서와 검증 계약
- 진행 중: 12주차 PDF, 데모 영상과 최종 공개 검증

10주차는 DX11 WARP client 1개와 bot session 23개의 production 경기, 네 replication mode 비교, 100ms RTT와 2% loss 및 10ms jitter 경기, Windows 2324.07초 soak를 완료했다. Ubuntu 24.04에서는 GCC `-Werror` build 뒤 ASan과 UBSan 24인 headless 경기를 1800초 동안 1817회 실행했다.

full-state 평균 수신량은 66.216564KiB/s로 64KiB/s 목표를 넘었다. 최종 interest-delta는 4.123043KiB/s였고 server tick P95는 1.8002ms였다. 기준선 목표 미달과 delta encode P95 증가를 포함한 원본 비교는 [24인 replication 비교](benchmarks/network-load/20260826-01ae1278-COMPARISON.md)에 있다.

11주차 local packaging 범위에서는 Ubuntu 24.04 GCC 13 Release `-Werror` server image와 lobby 1개 및 game worker 2개의 Compose 구성을 완료했다. 세 container health, worker control connection 2개, WorkerId 1과 2 registration 및 해당 local Compose project cleanup을 실제 Docker에서 확인했다. worker control 7001/TCP는 host에 publish하지 않는다.

AWS resource는 아직 만들지 않았다. 서울 `t3.small`은 첫 측정 후보이며, account와 현재 가격, public IPv4 및 EBS 비용, SSH 원본 `/32`와 종료 시각을 확인한 뒤에만 외부 접속 측정을 시작한다. 이 확인 전에는 11주차 전체를 완료로 바꾸지 않는다.

12주차 포트폴리오 기반 범위에서는 다섯 사례의 원본 근거, 네 구조 다이어그램, 통합 한계, 공개 상태 계약과 clang-uml 0.6.3 AST class diagram을 완료했다. class 결과는 기준 commit `884e5e70d68d9fcf9dfe5638d97e06623da154c2`에서 engine class 9개 및 relationship 5개, network class 9개 및 relationship 1개이며 generation manifest와 snapshot이 전체 verifier를 통과한다. 세 runtime asset도 HEAD `0658869e5cbabe8479be201b1824177dc907888c`에서 committed pointer, object-store와 hydrated worktree SHA-256 및 size가 일치했다.

같은 HEAD의 current-worktree Windows Debug build와 CTest 582/582, MSVC Release client 및 server build는 통과했다. Ubuntu 24.04 current-HEAD build는 성공으로 기록하지 않는다. Docker Desktop 4.81이 stale `C:\Users\siwon\AppData\Local\Docker\run\dockerInference` reparse point를 제거하지 못해 backend가 종료됐고, 안전 gate 뒤 시도한 PowerShell 삭제는 실행 정책이 차단됐으며 rename과 exact patch 삭제도 OS error 1920으로 실패했다. `EnableDockerAI=false` 복구도 같은 오류를 재현해 `true`로 원복했고 settings SHA-256은 원본 `30a8d70713ccd6864a4347483c005e9b5a06baa74f184e7a77ae338cb8e709f7`과 일치한다. 기존 WSL에는 `docker-desktop`만 있고 Ubuntu distro는 없다.

따라서 포트폴리오 기반 작업은 완료지만 12주차 전체는 진행 중이다. 18쪽에서 22쪽 PDF, 실제 데모 영상, 최종 공개 후보의 Ubuntu 24.04 및 hosted CI, AWS 외부 접속, 저장소 공개와 `v0.1.0`은 별도 gate로 남긴다.

PR #11의 head `e2aba12c670b288b596169b8115b1fef77d54068`은 GitHub Actions run `32935640972`에서 Windows와 Ubuntu job이 성공한 뒤 `884e5e70d68d9fcf9dfe5638d97e06623da154c2`로 main에 병합됐다. 이후 문서 커밋을 포함한 현재 branch HEAD는 hosted CI를 실행하지 않았다. 과거 runner billing 문구는 당시 미실행을 local 및 Docker 결과와 구분하기 위한 기록이며 현재 확인된 성공 run을 부정하지 않는다.

## 완료 판정

완료는 구현 여부가 아니라 최신 빌드, 자동 테스트, 실행 증거로 판정한다. 목표 수치를 달성하지 못한 경우 실제 수치와 병목을 기록하고 완료한 것처럼 표현하지 않는다.

문서 문체와 구성은 [기술 기록 문체 기준](WRITING_GUIDE.md)을 따른다.
