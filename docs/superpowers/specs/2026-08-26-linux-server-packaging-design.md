# 11주차 Linux 서버 패키징 설계

- 상태: 채택
- 날짜: 2026-08-26
- 기준 branch: `feat/linux-server-packaging`
- 기준 main: `c91b35f954e23d91ac7dfc525a7c7859f047de6a`

## 목표

Ubuntu 24.04 기반의 재현 가능한 서버 이미지를 만들고, 로비 서버 1개와 게임 서버 워커 2개를 Docker Compose로 실행한다. 실제 클라이언트가 접속하는 로비 TCP와 게임 TCP 및 UDP만 호스트에 공개하고 worker control TCP는 Compose 내부 네트워크에 남긴다.

이번 주차는 로컬 컨테이너 검증과 외부 접속 준비까지 다룬다. AWS 계정에서 인스턴스, 보안 그룹, 탄력적 IP 같은 리소스를 만드는 작업은 비용과 계정 확인 뒤에만 진행한다.

## 현재 상태

Linux GCC 빌드와 sanitizer 경기는 10주차 검증용 Ubuntu 컨테이너에서 통과했다. 다만 저장소에는 배포용 Dockerfile, Compose 파일, 이미지 실행 계약이 없다.

게임 서버는 worker control 연결에 Boost.Asio resolver를 사용하지만 CLI 파서는 `--lobby-control-host`에 숫자 IP만 허용한다. 따라서 현재 계약으로는 Compose의 서비스 DNS 이름을 사용할 수 없다. 고정 컨테이너 IP를 쓰면 동작은 가능하지만 네트워크 재생성에 취약하고 Compose가 제공하는 이름 기반 발견을 버리게 된다.

## 비교한 구성

### 고정 컨테이너 IP

코드 변경 없이 구성할 수 있다. 반면 subnet과 주소를 저장소에 고정해야 하고 서비스 추가나 다른 Compose project와의 충돌 가능성이 생긴다.

### 로비와 게임 서버를 한 컨테이너에서 실행

포트 연결은 단순하다. 프로세스별 종료, 재시작, 로그와 자원 경계가 섞이고 실제 멀티 서버 구조를 검증하지 못한다.

### 서비스 DNS와 분리된 worker 컨테이너

게임 서버가 `lobby-server`를 resolver로 해석하고 각 프로세스를 별도 컨테이너로 실행한다. 기존 비동기 연결 경계를 그대로 사용하면서 worker를 독립적으로 늘리거나 내릴 수 있다. 이번 주차는 이 구성을 선택한다.

## 저장소 구성

```text
deploy/
  compose.yaml
  .env.example
  docker/
    server.Dockerfile
    socket-healthcheck.sh
  README.md
  AWS_PRECHECK.md
scripts/
  test_server_compose.ps1
```

서버 이미지는 로비와 게임 서버 실행 파일을 모두 포함한다. Compose의 `command`가 컨테이너 역할을 선택한다. 같은 바이너리 집합을 두 번 빌드하지 않고 로비와 worker가 같은 protocol 및 simulation commit을 사용하게 하기 위한 선택이다.

## 이미지 계약

빌드 stage는 Ubuntu 24.04, CMake, Ninja와 고정 vcpkg baseline을 사용한다. Release 설정에서 `dxa_lobby_server`와 `dxa_game_server`만 빌드하고 CMake install 경계로 runtime stage에 복사한다.

runtime stage는 다음 조건을 지킨다.

1. root가 아닌 고정 UID로 실행한다.
2. 로비와 게임 서버 바이너리 및 socket healthcheck만 포함한다.
3. OCI revision label에 build arg로 전달된 commit SHA를 기록한다.
4. SIGTERM을 서버의 기존 signal handler에 전달한다.
5. 애플리케이션 포트 외 파일 쓰기를 요구하지 않는다.

Compose는 read-only root filesystem, capability 제거, `no-new-privileges`, init process를 적용한다. healthcheck는 연결을 만들지 않고 `/proc/net`에서 listening socket을 확인해 audit log를 오염시키지 않는다.

## Compose 프로세스와 포트

| 서비스 | 역할 | 외부 포트 | 내부 전용 포트 |
| --- | --- | --- | --- |
| `lobby-server` | 방, 준비, ticket, worker 배정 | 7000/TCP | 7001/TCP |
| `game-server-1` | worker 1, 한 경기 | 7100/TCP, 7101/UDP | 없음 |
| `game-server-2` | worker 2, 한 경기 | 7200/TCP, 7201/UDP | 없음 |

세 서비스는 `control` 내부 네트워크를 공유한다. game worker는 `lobby-server:7001`에 등록한다. worker control 포트 7001은 `ports`에 넣지 않아 호스트와 인터넷에 공개하지 않는다.

게임 서버가 ticket에 넣는 host는 `DXA_PUBLIC_HOST` 환경 변수에서 받는다. 기본값은 로컬 검증용 `127.0.0.1`이다. 외부 배포에서는 클라이언트가 해석할 수 있는 공인 IP 또는 DNS 이름을 명시해야 한다. 내부 Compose 이름을 ticket에 넣지 않는다.

두 worker는 서로 다른 WorkerId와 게임 포트를 등록한다. 한 worker가 active인 동안 두 번째 worker가 다음 방을 받을 수 있지만, 이번 주차의 컨테이너 smoke는 동시 두 경기의 부하 수치를 새로 주장하지 않는다.

## CLI 변경

`--lobby-control-host`는 숫자 IPv4 및 IPv6 외에 DNS hostname을 받는다. DNS 이름은 전체 253자 이하, label 63자 이하, 영문자와 숫자 및 하이픈만 허용하고 label의 처음과 끝에 하이픈을 허용하지 않는다.

`--game-bind`는 실제 local interface이므로 기존처럼 숫자 IP만 받는다. `--advertise-host`도 기존 공개 계약을 유지한다. DNS 해석 실패와 연결 실패는 기존 reconnect 경로를 사용한다.

## 검증

다음 순서로 완료 여부를 판정한다.

1. hostname 허용 테스트를 먼저 실패시키고 최소 parser 변경 뒤 통과시킨다.
2. Windows 전체 CTest에서 기존 numeric host와 invalid host 회귀를 확인한다.
3. `docker compose config --quiet`로 환경 변수와 port 계약을 확인한다.
4. clean build context에서 server image를 만든다.
5. Compose 세 서비스를 시작하고 모두 healthy인지 확인한다.
6. 로비 log의 worker control 연결 2개와 각 game log의 registration 완료를 확인한다.
7. 컨테이너를 내린 뒤 남은 container와 network가 없는지 확인한다.

## 보안과 외부 접속 경계

첫 AWS 후보는 기존 계획대로 서울 리전의 EC2 `t3.small`이지만, 실제 선택은 이미지의 runtime RSS와 두 worker 동시 실행 결과를 확인한 뒤 확정한다. 외부 보안 그룹은 7000/TCP, 7100/TCP, 7101/UDP, 7200/TCP, 7201/UDP만 게임 이용자에게 연다. 7001/TCP는 열지 않는다. SSH 22/TCP는 사용자의 현재 공인 IP 한 개로 제한한다.

현재 TCP와 UDP payload는 암호화되지 않았고 worker control에 상호 인증이 없다. 따라서 v1 외부 검증은 포트 제한과 일회성 ticket을 경계로 사용한다. 공개 서비스 운영, 개인정보 처리와 장기 노출은 이번 범위가 아니다.

## 남은 한계

1. healthcheck는 listening socket과 process 생존을 확인하지만 worker registration 상태를 조회하는 관리 API는 아니다.
2. Compose는 단일 호스트 배포이며 여러 EC2 인스턴스의 service discovery를 다루지 않는다.
3. image registry push, TLS termination, DDoS 방어와 자동 배포는 포함하지 않는다.
4. AWS 리소스 생성과 비용 발생은 사용자 확인 전까지 실행하지 않는다.
