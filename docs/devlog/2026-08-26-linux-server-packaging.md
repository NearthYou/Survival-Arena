# Linux 서버 image를 실제 worker pool로 묶기

## 상황

10주차에는 Ubuntu 24.04 container 안에서 Debug build와 sanitizer 경기를 실행했다. 이 container는 검증 명령을 수행하고 사라지는 도구였고, 실제 server executable만 담은 runtime image나 여러 worker 실행 계약은 없었다.

11주차의 질문은 하나였다. 로비와 게임 서버를 외부 접속 준비가 가능한 process 경계로 어떻게 옮길 것인가.

## 재현

처음에는 server install component가 없었다. 다음 CTest는 `cmake --install --component Server` 뒤 `bin/dxa_lobby_server.exe`를 찾지 못해 실패했다.

```powershell
ctest `
  --test-dir out/build/windows-msvc-vs-debug `
  --build-config Debug `
  --output-on-failure `
  -R '^ServerInstall\.InstallsExecutables$'
```

install 경계를 추가한 뒤 첫 Linux Release image build를 실행했다.

```powershell
docker build `
  --progress=plain `
  --tag dxa-server:week11-dev `
  --build-arg VCS_REF=working-tree `
  --file deploy/docker/server.Dockerfile `
  .
```

## 관찰

`.dockerignore` 적용 뒤 첫 build context는 약 1.14MB였다. source build는 Ubuntu 24.04 GCC 13의 Release `-O3 -Werror`에서 멈췄다. `LobbyService`가 `ServerMessage` variant를 가진 aggregate 임시 객체를 vector로 옮길 때 GCC가 비활성 variant 대안의 ID를 `maybe-uninitialized`로 판단했다.

ID와 ticket 정의를 확인했지만 관련 정수 field에는 모두 0 기본값이 있었다. 같은 코드는 Linux Debug와 Windows Release에서 이미 동작했다. 경고를 끄거나 배포 image를 Debug로 낮추면 Release 경계는 계속 검증되지 않는다.

Compose smoke에서는 별도 도구 차이도 확인했다. Docker Compose v5.2.0의 `ps --format json`은 JSON 배열 하나가 아니라 한 줄에 객체 하나를 출력했다. 배열로 한 번에 decode한 첫 runner는 세 container가 healthy인 상태에서도 JSON 추가 텍스트 오류로 멈췄다.

## 가설과 비교한 대안

고정 container IP는 parser를 바꾸지 않아도 되지만 network 재생성에 취약하다. 로비와 게임 서버를 한 container에서 실행하면 service discovery는 사라지지만 process별 종료와 재시작 경계도 사라진다.

Release 경고에는 target 전체의 `-Wmaybe-uninitialized`를 끄는 방법도 있었다. 하지만 이후 실제 미초기화 경고까지 숨긴다. 경고가 발생한 message만 vector storage에 직접 생성하면 variant aggregate 이동 자체를 없앨 수 있다고 봤다.

## 선택

Compose service DNS를 사용하고 lobby 1개와 game worker 2개를 분리했다. `--lobby-control-host`는 숫자 IP 외에 제한된 DNS hostname을 받도록 했다. worker control 7001/TCP는 내부 network에서만 사용하고 host에는 publish하지 않았다.

Release 경고 경로는 `OutboundMessage`를 message alternative에서 직접 만들 수 있게 하고, ticket과 worker 실패 응답을 `emplace_back`으로 생성했다. GCC 경고 option은 바꾸지 않았다.

## 구현

server image는 고정 vcpkg baseline으로 두 executable만 Release build한 뒤 CMake `Server` component를 runtime stage에 설치한다. runtime에는 두 binary와 socket healthcheck만 복사한다. 최종 image는 UID 10001로 실행하고 root filesystem을 read-only로 쓴다.

Compose에는 WorkerId 1과 2를 명시했다. 두 worker는 서로 다른 TCP와 UDP port를 등록하고 기본 `interest-delta` replication을 사용한다. 외부 client에게 전달할 host는 `DXA_PUBLIC_HOST`로 따로 받는다.

smoke runner는 비어 있는 host port를 고른다. 고유 project를 만든 뒤 세 service health, lobby의 worker connection 2개와 각 worker의 registration log를 확인한다. 성공과 실패 모두 자신이 만든 project만 내리고 container와 network 잔존 수를 검사한다.

## 검증

다음 결과를 실제로 확인했다.

1. `ServerInstall.InstallsExecutables` RED 뒤 GREEN
2. `ServerCompose.ValidatesRuntimeContract` RED 뒤 GREEN
3. Ubuntu 24.04 GCC 13 Release `-Werror` image build 통과
4. shellcheck v0.11.0 통과
5. runtime UID 10001
6. lobby와 game server의 `ldd` 의존성 누락 0
7. 개발 image 크기 30,723,573 bytes
8. read-only lobby container의 7000과 7001 socket health 통과
9. Compose container 3개 healthy
10. worker control connection 2개와 WorkerId 1 및 2 registration 확인
11. smoke 종료 뒤 해당 project container와 network 잔존 0

개발 중 image의 OCI revision은 `working-tree`로 기록했다. commit SHA가 붙은 최종 smoke는 branch가 깨끗한 상태에서 다시 실행하며, 이 개발 run을 commit 고정 benchmark로 표현하지 않는다.

## 남은 한계

socket health는 worker가 로비에 등록됐는지 직접 질의하지 않는다. registration은 log 기반 smoke로 별도 확인한다.

두 worker가 동시에 production 24인 경기를 수행하는 Linux cloud 수치는 아직 없다. 10주차 Windows soak의 game server working set P95 48.61MiB와 tick P95 1.8424ms는 첫 instance 후보를 고르는 참고값이지 EC2 통과 수치가 아니다.

TCP와 UDP는 암호화되지 않았고 worker control 상호 인증도 없다. AWS resource는 아직 만들지 않았다. 다음 변경은 account, 서울 region 가격, SSH 원본 IP와 종료 시간을 먼저 확인한 뒤 짧은 외부 접속 측정을 수행하는 것이다.
