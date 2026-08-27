# ACK 기준 delta로 24인 수신량과 손실 복구를 함께 다룬 과정

## 상황

full-state는 15Hz로 actor 124개와 loot 60개를 보냈다. 24인 평균 수신량 `66.216564KiB/s`는 목표 64KiB/s를 넘었다. 손실 복구와 수신량을 다뤄야 했다.

## 재현

WARP client 1개, bot 23개와 AI 100개를 commit `01ae1278` Release에서 실행했다. 첫 interest-full은 server에만 mode가 전달돼 protocol error 1로 끝났다. option을 맞춰 네 mode를 다시 쟀다. 과정은 [개발 기록](../../devlog/2026-08-25-24-player-network-load.md)에 있다.

## 관찰

full-state는 payload 910,473,696바이트를 보냈다. 마지막 송신 snapshot을 delta 기준으로 쓰면 그 datagram이 유실된 다음 delta도 적용할 수 없다. bot별 runtime thread도 server 비교에 client scheduling 비용을 섞었다.

## 가설과 비교한 대안

interest, 16비트 양자화, delta를 단계별로 비교했다. full-state 압축은 먼 객체가 남고 TCP는 오래된 snapshot이 최신 상태를 막는다. ACK snapshot을 기준으로 삼는 대신 baseline 메모리, 비교 연산과 keyframe 복구 비용을 받아들였다.

## 선택

baseline을 최대 32개씩 보관하고 없으면 keyframe을 보냈다. local actor를 포함하고 enter 80, leave 88 반경을 썼다. bot은 runtime 하나를 공유했다. 계약은 [replication ADR](../../adr/0008-acked-interest-replication.md)에 있다.

## 구현

keyframe은 visible 객체를 16비트로 기록하고 delta는 enter, remove와 변경 bitset으로 나눴다. client는 baseline 복사본에 적용한다. 미래 ACK는 거부하고 과거 ACK는 상태를 되돌리지 않는다. shaper는 game UDP에 편도 50ms, jitter 10ms, loss 2%를 적용했다.

## 검증

[비교 원본](../../benchmarks/network-load/20260826-01ae1278-COMPARISON.md)에서 평균 수신량은 `66.216564KiB/s`에서 `4.123043KiB/s`로 줄고 encode P95는 0.0294ms에서 0.0672ms로 늘었다. impairment 경기의 drop `12579`, keyframe request `3875`, protocol error `0`, queue overflow `0`으로 winner 4와 tick 17430을 유지했다.

## 남은 한계

그래픽은 WARP였고 shaper는 kernel queue, NAT와 route change를 재현하지 않는다. Windows 성능과 Linux sanitizer 원본 commit도 다르며 외부 cloud 수치가 아니다. traffic 감소를 RTX 성능이나 장기 서비스 안정성으로 확대하지 않는다.
