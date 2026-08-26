# 11주차 Linux 서버 패키징 구현 계획

> 기준 설계: `docs/superpowers/specs/2026-08-26-linux-server-packaging-design.md`

목표는 로비 1개와 게임 worker 2개를 재현 가능한 Linux image와 Compose로 실행하고, 외부 비용이 생기기 전까지의 검증 증거를 남기는 것이다.

## Task 1: Compose 서비스명 해석

수정 파일:

- `tests/game_server_options_test.cpp`
- `apps/game_server/src/GameServerOptions.cpp`

순서:

1. `lobby-server`, multi-label hostname, 잘못된 label을 고정하는 parser 테스트를 추가한다.
2. 새 hostname 테스트만 실행해 실패를 확인한다.
3. 숫자 IP 또는 제한된 DNS hostname을 받는 validator를 구현한다.
4. game server options 테스트와 전체 CTest를 실행한다.
5. `fix(server): worker control DNS 이름 허용`으로 커밋한다.

## Task 2: CMake server install 경계와 runtime image

수정 파일:

- `apps/lobby_server/CMakeLists.txt`
- `apps/game_server/CMakeLists.txt`
- `.dockerignore`
- `deploy/docker/server.Dockerfile`
- `deploy/docker/socket-healthcheck.sh`

순서:

1. 두 server executable의 `Runtime` install component를 추가한다.
2. Linux Release server target만 빌드하는 multi-stage Dockerfile을 작성한다.
3. runtime user, OCI revision, SIGTERM, read-only 실행 조건을 반영한다.
4. listening TCP socket을 검사하는 shell healthcheck를 추가한다.
5. image를 build하고 `ldd`, UID, revision label과 실행 파일 존재를 확인한다.
6. `feat(deploy): Linux 서버 이미지 추가`로 커밋한다.

## Task 3: 두 worker Compose 구성

수정 파일:

- `deploy/compose.yaml`
- `deploy/.env.example`
- `tests/server_compose_contract_test.ps1`
- `tests/CMakeLists.txt`

순서:

1. 공개 포트, 내부 worker port, worker ID와 advertise host를 검증하는 contract 테스트를 추가한다.
2. 테스트 실패를 확인한다.
3. 로비 1개, 게임 worker 2개, 내부 control network를 구성한다.
4. read-only filesystem, capability 제거, init, restart와 healthcheck를 적용한다.
5. contract 테스트와 `docker compose config --quiet`를 통과시킨다.
6. `feat(deploy): 두 game worker Compose 구성`으로 커밋한다.

## Task 4: 실제 컨테이너 smoke와 운영 기록

수정 파일:

- `apps/game_server/src/GameServer.cpp`
- `scripts/test_server_compose.ps1`
- `deploy/README.md`
- `deploy/AWS_PRECHECK.md`
- `README.md`
- `docs/PROJECT_PLAN.md`
- `docs/devlog/2026-08-26-linux-server-packaging.md`

순서:

1. worker registration 완료를 token 없이 식별할 수 있는 운영 log를 추가한다.
2. 고유 Compose project 이름으로 build, start, health 및 registration log를 검사하고 항상 정리하는 smoke script를 작성한다.
3. 실제 image와 Compose를 실행해 세 container가 healthy이고 worker 1과 2가 등록되는지 확인한다.
4. 종료 후 해당 project의 container와 network가 남지 않았는지 확인한다.
5. 로컬 실행 방법, 외부 host 설정, 공개 포트와 AWS 비용 전 확인 지점을 문서화한다.
6. 실제 실행 명령과 관찰 결과만 devlog에 기록한다.
7. `docs(deploy): 11주차 컨테이너 검증 기록`으로 커밋한다.

## Task 5: 최종 검증과 마일스톤 준비

1. Windows Debug 전체 CTest를 실행한다.
2. Windows Release build를 실행한다.
3. Docker image를 no-cache가 아닌 clean source context에서 다시 build한다.
4. Compose smoke를 다시 실행한다.
5. worktree status와 commit history를 검토한다.
6. branch를 push하고 PR을 만든다.
7. hosted CI가 실행되지 않으면 결제 blocker와 local 및 Docker evidence를 분리해 기록한다.
8. AWS 리소스 생성 전 사용자에게 계정, 리전, 공인 IP, 예상 비용 확인을 요청한다.
