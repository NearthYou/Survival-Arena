# ADR 0009: 단일 host Compose worker pool

- 상태: 채택
- 날짜: 2026-08-26

## 상황

로비와 권위형 게임 서버는 별도 executable이지만 배포용 process 경계가 없었다. 게임 서버 하나는 동시에 한 경기만 맡으므로, 두 방이 겹치면 최소 두 worker가 필요하다. worker control은 외부 client protocol이 아니며 public port로 열 이유가 없다.

게임 서버의 control 연결은 Boost.Asio resolver를 사용했지만 CLI parser가 숫자 IP만 허용해 Compose service DNS를 사용할 수 없었다.

## 결정

1. Ubuntu 24.04 multi-stage image 하나에 lobby와 game server executable을 함께 넣는다.
2. Compose에서 lobby 1개와 WorkerId가 다른 game server 2개를 별도 container로 실행한다.
3. game worker는 `lobby-server:7001`을 DNS로 해석한다.
4. worker control 7001/TCP는 publish하지 않는다.
5. lobby 7000/TCP와 worker별 game TCP 및 UDP만 host에 publish한다.
6. public ticket host는 `DXA_PUBLIC_HOST`에서 받고 Compose 내부 이름과 분리한다.
7. container는 non-root, read-only root filesystem, capability 제거와 `no-new-privileges`를 사용한다.

## 결과

worker process를 독립적으로 재시작할 수 있고, 한 worker가 active인 동안 다른 worker가 다음 방을 받을 수 있다. 같은 image를 사용하므로 lobby와 worker의 protocol commit이 어긋날 가능성도 줄어든다.

대신 현재 Compose는 단일 host에 묶여 있다. worker 수를 동적으로 늘리려면 WorkerId와 외부 port 할당을 자동화해야 한다. 여러 host로 확장하려면 service discovery, worker control 인증과 암호화가 추가로 필요하다.

listening socket healthcheck는 process와 bind 상태만 확인한다. worker registration은 `game_server_registered worker=<ID>` log와 smoke runner에서 별도로 확인한다.
