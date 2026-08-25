# ADR 0008: 수신 확인 snapshot을 기준으로 관심 영역 delta를 생성함

- 상태: 채택
- 날짜: 2026-08-26

## 상황

9주차 game server는 15Hz full-state snapshot을 모든 client에게 보냈다. 24명과 중립 AI 100개, loot 60개를 넣은 공식 한 경기에서 참가자 평균 수신량은 66.216564KiB/s였다. 목표 64KiB/s를 넘었고 payload 910473696바이트, server UDP 968476695바이트가 기록됐다.

10주차에는 관심 영역, 양자화와 delta를 단계별로 적용하되 UDP 손실 뒤에도 client와 server가 같은 baseline을 사용해야 했다. server가 직전에 보낸 snapshot을 기준으로 delta를 만들면 그 snapshot이 손실됐을 때 다음 delta도 적용할 수 없다. 모든 손실을 reliable 재전송으로 바꾸면 최신 상태가 중요한 snapshot 경로에 오래된 packet이 쌓인다.

23개 bot을 process 23개로 띄우면 각 process가 `io_context` thread를 소유해 thread 수와 shutdown 비용이 부하 결과에 섞인다. network impairment도 TCP와 UDP에 함께 적용하면 room 생성, ticket과 최종 결과까지 손실되어 snapshot 복구와 control failure를 구분하기 어려웠다.

## 결정

server는 recipient가 마지막으로 확인한 snapshot을 delta baseline으로 사용한다.

client input에는 `acknowledgedSnapshotId`와 `requestKeyframe`을 넣는다. server는 해당 recipient에게 실제로 발급한 snapshot ID까지만 ACK로 받는다. 같은 ACK와 과거 ACK는 상태를 뒤로 돌리지 않고, 발급하지 않은 미래 ACK는 protocol violation으로 처리한다.

server는 recipient별 quantized baseline을 최대 32개 보관한다. 확인된 baseline을 가능한 한 유지하면서 오래된 미확인 baseline부터 버린다. ACK baseline이 이미 제거됐으면 connection을 끊지 않고 recipient keyframe을 보낸다. client도 최대 32개 baseline을 보관하고 delta가 참조한 baseline이 없으면 world를 바꾸지 않은 채 keyframe을 요청한다.

관심 영역 keyframe은 global full-state가 아니라 해당 recipient의 현재 visible set 전체를 담는다. local controlled actor는 거리와 관계없이 항상 포함한다. 새로 들어온 객체는 완전한 record, 나간 객체는 ID, 계속 보이는 객체는 변경 field만 delta에 기록한다. enter 반경은 80, leave 반경은 88로 두어 경계 왕복을 줄인다.

위치와 safe zone 값은 arena bounds 안에서 16비트로 양자화한다. world position은 전달하지 않고 client가 같은 bounds로 복원한다. health, cooldown과 elimination도 protocol 상한 안에서 bounded value로 바꾼다.

bot 23개는 하나의 `BotCoordinator` process와 하나의 `GameNetworkRuntime`을 공유한다. 각 bot은 별도 `GameSession`, TCP socket, UDP socket과 prediction state를 유지하지만 Asio thread는 하나다. 종료 시 session을 먼저 닫고 shared runtime을 마지막에 중지한다.

application impairment는 game UDP 송신에만 적용한다. client 방향과 server 방향은 각각 한 번만 shaping해 편도 50ms가 왕복 100ms가 되게 한다. lobby TCP, worker control과 game TCP는 건드리지 않는다. peer별 delayed queue는 256개로 제한하고 drop, delay, delivery와 overflow를 따로 기록한다.

## 비교한 대안

마지막으로 보낸 snapshot을 baseline으로 쓰면 server state가 단순하다. UDP 손실 한 번이 연속 delta 실패로 이어지고 client가 어떤 baseline을 갖고 있는지 알 수 없어서 제외했다.

모든 snapshot을 keyframe으로 보내면 복구 요청이 필요 없다. interest-full은 평균 수신량을 26.790149KiB/s까지 줄였지만 payload와 fragment는 계속 매 snapshot 전체 visible set 크기에 비례했다.

global full-state keyframe을 주기적으로 보내는 방법은 구현이 쉽다. 한 recipient의 손실 복구 때문에 actor 124개와 loot 60개를 다시 보내므로 관심 영역의 이점을 잃는다. recipient-full keyframe을 선택했다.

bot마다 runtime thread를 두는 방법은 기존 단일 bot 구조를 반복하기 쉽다. 23개 thread scheduling과 독립 timer가 server 비용 비교에 섞여 하나의 runtime을 공유했다.

OS network emulator를 쓰는 방법은 application code가 단순하다. CI와 개발 PC에서 같은 seed와 drop ordinal을 재현하기 어렵고 administrator 설정이 필요해, 이번 비교에서는 application UDP adapter를 사용했다.

TCP에도 impairment를 적용하는 방법은 전체 장애를 흉내 낼 수 있다. 이번 질문은 snapshot ACK와 keyframe 복구였으므로 control과 result를 안정적으로 유지하고 UDP만 격리했다.

## 결과

같은 commit, seed와 24명 조건에서 interest-delta는 full-state 대비 payload를 94.499%, server UDP bytes를 91.560%, 참가자 평균 수신량을 93.773%, fragment 합계를 74.998% 줄였다. 평균 수신량은 4.123043KiB/s, recipient P95는 4.246719KiB/s였다.

replication encode P95는 full-state 0.0294ms에서 interest-delta 0.0672ms로 2.286배가 됐다. network traffic을 줄이는 대신 recipient visibility, quantization과 delta 비교 비용이 추가됐다. server tick P95는 네 mode 모두 33.3ms 안이었고 interest-delta 한 경기 값은 1.8002ms였다.

100ms RTT, 2% loss와 10ms jitter 경기에서는 datagram 12579개가 drop됐고 keyframe 요청 3875회가 발생했다. protocol error와 queue overflow 없이 winner 4, tick 17430 결과를 유지했다.

Windows impairment soak는 네 경기와 실제 match 시간 2324.07초를 완료했다. drop 50488개와 keyframe 요청 15690회가 있었고 protocol error, queue overflow와 scheduler overrun은 0이었다. game server working set 232개 표본은 마지막 15분 동안 매번 증가하지 않았다.

원본과 비교 수치는 [24인 replication 비교](../benchmarks/network-load/20260826-01ae1278-COMPARISON.md)에 연결한다.

## 결과에 따른 제약

baseline 32개는 고정 상한이다. RTT가 길거나 client input이 오래 끊기면 확인 baseline이 제거되어 keyframe 비율이 올라갈 수 있다. v1은 memory 상한을 우선하고 adaptive capacity를 넣지 않는다.

application shaper는 실제 kernel queue, NAT와 route change를 재현하지 않는다. drop과 jitter 복구의 deterministic regression 도구로 사용한다.

game TCP와 UDP는 여전히 평문이며 reconnect와 endpoint rebind가 없다. 11주차 외부 접속 전에 transport와 firewall 경계를 별도로 결정한다.

자동 그래픽 부하는 WARP에서 수행했다. RTX 3050 Ti hardware 성능을 이 network 수치로 주장하지 않는다.
