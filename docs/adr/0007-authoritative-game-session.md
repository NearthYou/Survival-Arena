# ADR 0007: 권위형 경기 session의 통신 경계를 세 채널로 분리함

- 상태: 채택
- 날짜: 2026-08-24

## 상황

8주차 로비는 방을 InMatch로 바꾸고 참가자별 ticket을 발급했지만, worker endpoint는 고정 문자열이었다. 실제 game server가 준비됐는지 확인하지 않았고 ticket을 소비하는 process도 없었다. 9주차에는 lobby reservation, game 인증, 30Hz simulation, UDP input과 15Hz snapshot, match result를 실제 process 사이에서 이어야 했다.

이 흐름을 한 TCP connection에 넣으면 lobby의 수명이 경기 packet 양과 묶인다. 반대로 모든 message를 UDP로 보내면 worker 예약과 ticket 인증, 최종 결과에 별도 재전송과 순서 보장이 필요하다. input을 click event로 보내는 방식은 packet 하나가 사라졌을 때 이동 의도 자체가 없어지는 문제도 있었다.

## 결정

통신 경계를 worker control TCP, game TCP, game UDP로 나눈다.

1. worker control TCP는 lobby와 game worker 사이의 등록, 경기 예약, 취소, 준비 완료와 종료 통지를 담당한다.
2. game TCP는 참가자의 일회성 match ticket 인증, session 설정과 신뢰성이 필요한 경기 결과를 담당한다.
3. game UDP는 endpoint bind 뒤 이동과 공격 input, 15Hz world snapshot을 담당한다.

각 채널은 자기 방향에 속하지 않는 message를 decode 단계에서 거부한다. TCP는 기존 64KiB frame 상한을 사용하고 UDP datagram은 1,200바이트를 넘기지 않는다. snapshot payload가 한 datagram에 들어가지 않으면 snapshot ID, fragment 번호, 전체 길이와 CRC32를 붙여 최대 32개로 나눈다. CRC32는 조립 오류 검출용이며 인증 수단으로 사용하지 않는다.

방 시작은 두 단계 reservation으로 처리한다. lobby가 조건을 확인하면 room을 Starting으로 바꾸고 MatchId와 참가자 ticket을 임시로 만든 뒤 idle worker 하나에 예약을 보낸다. worker가 참가자 roster와 endpoint를 받아들였다는 응답을 보낸 뒤에만 room을 InMatch로 바꾸고 client에게 ticket을 전달한다. 거부, timeout 또는 control 연결 종료가 먼저 발생하면 ticket을 폐기하고 room을 Waiting으로 되돌린다.

worker 하나의 capacity는 v1에서 경기 하나로 고정한다. 한 `io_context` thread가 control TCP, game TCP, UDP와 simulation timer를 순서대로 처리하므로 match state에 mutex를 추가하지 않는다. 여러 경기를 한 worker에 넣는 결정은 room별 strand, timer와 메모리 상한을 측정한 뒤 다시 내린다.

client input은 button edge가 아니라 현재 유지 중인 이동 목적지와 공격 대상을 담은 상태다. 매 30Hz client tick마다 증가하는 sequence와 함께 보낸다. server는 endpoint, token, MatchId와 PlayerId를 먼저 확인한 뒤 새 sequence만 처리한다. 이동 목적지는 server NavMesh에서 다시 검증하며, semantic 검증에 실패한 새 input도 ACK를 전진시켜 같은 잘못된 값의 반복 재생을 막는다.

local actor는 server와 같은 고정 delta로 예측한다. snapshot ACK 이하의 input을 버리고 권위 위치에서 남은 input을 다시 적용한다. 다른 actor는 최신 snapshot보다 3 tick 늦은 시점을 보간한다. target 뒤의 새 snapshot이 없으면 마지막 위치를 유지하고 외삽하지 않는다. 첫 버전에서는 순간 정지보다 존재하지 않는 이동을 만들어 충돌과 공격 표시를 어긋나게 하는 위험이 더 크다고 판단했다.

## 비교한 대안

로비 TCP 하나에 모든 경기 traffic을 합치는 방법은 port와 session 수가 줄어든다. 다만 lobby 장애와 고빈도 snapshot 부하가 같은 event loop에 들어가고, 경기 worker를 독립적으로 늘릴 수 없다.

game TCP로 input과 snapshot까지 보내는 방법은 순서와 재전송을 직접 만들 필요가 없다. 오래된 snapshot이 새 snapshot 앞을 막는 head-of-line blocking이 생기므로 실시간 상태는 UDP로 분리했다. 인증과 결과는 손실되면 안 되므로 TCP에 남겼다.

input을 destination click 한 번만 보내는 방법은 packet 양이 적다. 손실된 click을 복구하려면 별도 reliable message가 필요하고, client와 server가 현재 의도를 다르게 기억할 수 있다. 현재 상태를 sequence와 함께 반복 전송하는 쪽을 선택했다.

remote actor를 최신 속도로 외삽하는 방법도 검토했다. packet 간격이 길 때 화면은 덜 멈추지만, server가 장애물이나 사망으로 멈춘 뒤에도 actor가 계속 이동할 수 있다. 10주차 network impairment 측정 전에는 3 tick 보간과 hold를 기준으로 둔다.

## 결과

lobby와 worker control, game TCP와 UDP를 실제 loopback socket으로 연결했다. Windows 수직 테스트에서는 WARP DX11 client와 headless play bot이 RoomId 1과 MatchId 1을 공유했다. 두 client가 각각 snapshot 29개를 받은 뒤 60 tick에 같은 결과를 확인했다. 가장 최근 3회 반복 중 마지막 실행 시간은 3,011ms였다.

수동 실행에서는 hardware DX11 client와 play bot이 RoomId 1, MatchId 1로 인증되고 각각 snapshot 2개 이상을 받았다. bot의 game connection을 종료하자 server가 이를 다음 tick의 탈락으로 처리했고 client는 winner 0, finished tick 2,789를 받았다. 별도 120 frame Debug 실행에서 NVIDIA GeForce RTX 3050 Ti Laptop GPU와 DX11 debug layer error 0을 확인했다.

## 결과에 따른 제약

기본 bind는 모두 loopback이다. game TCP와 UDP는 암호화되지 않았고 worker control에도 외부 network용 상호 인증이 없다. 이 상태로 public interface에 노출하지 않는다. 11주차 외부 접속 전에 security group과 transport 보안 경계를 별도로 결정한다.

재접속, session resume와 UDP endpoint rebind는 지원하지 않는다. game TCP가 끊기면 참가자는 탈락한다. worker control 연결이 끊기면 진행 중인 경기를 복구하지 못한다.

snapshot은 모든 actor와 loot를 담는 full state다. 24인 play bot 부하, 관심 영역, 양자화, 변경 bitset, 손실과 jitter 조건의 대역폭은 아직 측정하지 않았다. 이는 10주차 범위다.

client correction은 즉시 반영하며 시각적 smoothing이 없다. remote actor는 외삽하지 않기 때문에 snapshot이 늦으면 마지막 위치에서 멈춘다. 실제 수동 장면에서는 고정 camera가 캐릭터나 zone geometry와 겹쳐 시야가 가려지는 구간도 확인했다.
