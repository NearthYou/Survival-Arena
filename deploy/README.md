# Linux 서버 컨테이너 실행

이 구성은 같은 server image에서 lobby server 1개와 game server worker 2개를 별도 container로 실행한다. worker control TCP 7001은 Compose network 안에서만 사용하고 host에 publish하지 않는다.

## 로컬 실행

저장소 루트에서 환경 예시를 복사한다.

```powershell
Copy-Item deploy/.env.example deploy/.env
```

로컬 client가 같은 PC에서 접속할 때 `DXA_PUBLIC_HOST=127.0.0.1`을 유지한다. 다른 PC에서 접속할 때는 `deploy/.env`의 값을 client가 실제로 해석하고 접근할 수 있는 공인 IP 또는 DNS 이름으로 바꾼다. 내부 이름인 `lobby-server`를 넣으면 안 된다.

image를 만들고 세 서비스를 시작한다.

```powershell
docker compose `
  --file deploy/compose.yaml `
  --env-file deploy/.env `
  build

docker compose `
  --file deploy/compose.yaml `
  --env-file deploy/.env `
  up --detach --no-build --wait --wait-timeout 120
```

상태와 log는 다음 명령으로 확인한다.

```powershell
docker compose --file deploy/compose.yaml --env-file deploy/.env ps
docker compose --file deploy/compose.yaml --env-file deploy/.env logs --no-color
```

정상 실행이면 lobby log에 worker control connection 두 개가 나타나고 각 game server log에 다음 행이 나타난다.

```text
game_server_registered worker=1
game_server_registered worker=2
```

종료할 때는 이 project의 container와 network를 내린다. image는 다음 실행의 build cache를 위해 남는다.

```powershell
docker compose `
  --file deploy/compose.yaml `
  --env-file deploy/.env `
  down --remove-orphans --timeout 15
```

## 자동 smoke

빈 host port를 고르고 build부터 registration과 cleanup까지 확인하려면 다음 runner를 사용한다.

```powershell
./scripts/test_server_compose.ps1
```

runner는 고유 Compose project를 만들고 다음 항목을 확인한다.

1. lobby와 game worker 2개의 healthy 상태
2. worker control connection 2개
3. WorkerId 1과 2의 registration 완료
4. image revision label
5. 종료 뒤 해당 project container와 network 잔존 0

이미 build한 image만 다시 검사하려면 image 이름을 넘긴다.

```powershell
./scripts/test_server_compose.ps1 `
  -ImageName dxa-server:local `
  -SkipBuild
```

## 공개 포트

| 용도 | 기본 port | protocol | 공개 여부 |
| --- | --- | --- | --- |
| lobby client | 7000 | TCP | client 접속에 필요 |
| worker control | 7001 | TCP | host에 공개하지 않음 |
| game worker 1 | 7100 | TCP | game 인증에 필요 |
| game worker 1 | 7101 | UDP | input과 snapshot에 필요 |
| game worker 2 | 7200 | TCP | game 인증에 필요 |
| game worker 2 | 7201 | UDP | input과 snapshot에 필요 |

port를 바꾸면 `deploy/.env`의 값을 바꾼다. Compose는 container bind port와 host publish port, ticket에 들어가는 port를 같은 값으로 사용한다.

## 보안 경계

container는 UID 10001, read-only root filesystem, capability 제거와 `no-new-privileges`로 실행한다. healthcheck는 연결을 만들지 않고 `/proc/net`의 listening socket을 확인한다.

현재 game TCP와 UDP payload는 암호화되지 않았고 worker control에는 상호 인증이 없다. 이 구성은 짧은 포트폴리오 검증용이며 장기 공개 운영용이 아니다. AWS에서 실행하기 전에는 [AWS 비용 및 보안 확인표](AWS_PRECHECK.md)를 먼저 확정한다.
