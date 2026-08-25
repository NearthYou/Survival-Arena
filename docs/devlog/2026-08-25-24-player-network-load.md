# 24명 snapshot을 줄이면서 손실 뒤에도 한 경기를 끝냈다

10주차 작업은 2026년 8월 25일에 시작했고 공식 측정은 26일에 끝났다. 9주차에는 DX11 client와 play bot 하나만 실제 game session에 들어갔다. 이번에는 DX11 client 1개와 같은 `GameSession`을 쓰는 bot 23개가 production 설정 한 경기를 끝내야 했다.

## 증상

첫 문제는 부하 구조였다. bot 23개가 각각 network thread를 만들면 server가 아니라 client process scheduling 비용까지 함께 측정하게 된다.

두 번째 문제는 snapshot 크기였다. full-state는 참가자마다 actor 124개와 loot 60개를 15Hz로 보낸다. 공식 기준선의 참가자 평균 수신량은 66.216564KiB/s로 목표 64KiB/s를 넘었다.

세 번째 문제는 delta의 기준이었다. UDP에서는 server가 보낸 snapshot과 client가 실제 적용한 snapshot이 같다고 가정할 수 없다. 손실된 packet을 기준으로 다음 delta를 만들면 client는 복원할 수 없다.

## 재현

기능 회귀는 sudden death 30 tick, hard timeout 60 tick을 생성자에서만 주입한 WARP fixture로 돌렸다. 이 수치는 production 성능이 아니라 실제 DX11 render loop와 23개 bot session이 같은 result를 받는지 빠르게 확인하기 위한 값이다.

공식 수치는 Release binary와 production match config를 사용했다. 네 mode는 commit `01ae1278a5f304dfd83e289f671a01f1095489f3`, seed `20260825`, 1920x1080 WARP, DX11 client 1개, bot 23개와 중립 AI 100개를 같게 유지했다.

```powershell
$sha = '01ae1278a5f304dfd83e289f671a01f1095489f3'

./scripts/run_network_load.ps1 `
  -ReplicationMode interest-delta `
  -Matches 3 `
  -Seeds 20260825,20260826,20260827 `
  -CommitSha $sha `
  -Release

./scripts/run_network_load.ps1 `
  -ReplicationMode interest-delta `
  -Matches 3 `
  -Seeds 20260825,20260826,20260827 `
  -CommitSha $sha `
  -Impairment `
  -SoakMinutes 30 `
  -Release
```

첫 full-state 사전 실행은 한 경기를 완주했다. 이어서 interest-full을 실행하자 DX11 client가 welcome 직후 `protocol_errors=1`로 끝났다. server에만 `--replication-mode interest-full`을 전달하고 client는 기본 full-state를 기대한 것이 원인이었다. 같은 option을 양쪽에 전달한 뒤 네 mode를 새 commit에서 다시 측정했다.

Linux ASan 첫 build도 실패했다. GCC가 16비트 조합 결과의 축소 변환, `Recipient`와 test option aggregate의 생략 초기화를 `-Werror`로 거부했다. 명시적 변환과 완전 초기화 뒤 build가 통과했다.

## 가설

먼 객체와 비활성 loot를 보내지 않으면 full-state의 가장 큰 중복을 먼저 줄일 수 있다고 봤다. 그 다음 position과 bounded gameplay 값을 16비트로 바꾸면 fragment가 줄고, 마지막으로 ACK baseline과 변경 field만 보내면 움직임이 적은 snapshot의 payload를 더 줄일 수 있다고 예상했다.

loss recovery는 재전송보다 recipient ACK가 맞다고 봤다. server가 client의 마지막 적용 snapshot을 알면 ACK 이전 baseline은 지워도 되고, baseline이 없을 때만 keyframe을 보낼 수 있다.

## 비교한 대안

full-state 압축만 적용하는 방법은 interest 판정 비용이 없다. 하지만 모든 recipient에게 동일한 먼 객체를 계속 보낸다.

마지막 송신 snapshot 기준 delta는 ACK field가 필요 없다. 한 datagram 손실이 다음 delta 실패로 이어져 제외했다.

모든 손실을 TCP 재전송으로 해결하면 순서는 보장된다. 오래된 snapshot이 최신 snapshot을 막는 비용 때문에 UDP를 유지했다.

bot process를 23개 띄우는 방법은 process 격리가 분명하다. 이번 측정에서는 thread와 process overhead를 줄이기 위해 한 coordinator와 shared runtime을 사용했다.

## 구현

`InterestGrid`는 arena를 cell로 나누고 recipient 중심에서 leave radius에 닿는 cell만 조회한다. 이전에 보이던 ID는 88, 새 ID는 80 반경을 사용한다. 결과 ID는 정렬하고 중복을 제거하며 local actor는 항상 포함한다.

quantized keyframe은 global state와 visible actor 및 loot 전체를 16비트 값으로 기록한다. delta는 global field mask, enter record, remove ID와 변경 bitset을 나눠 보낸다. client는 baseline 복사본에 remove, enter, change 순서로 적용하고 world를 복원한다.

server는 recipient별 baseline을 32개로 제한한다. client input의 ACK가 미래 snapshot이면 protocol violation, 같거나 오래된 값이면 상태를 뒤로 움직이지 않는다. ACK baseline이 없거나 client가 요청하면 recipient keyframe을 보낸다.

UDP impairment는 client 송신과 server 송신에 각각 한 번씩 적용한다. 편도 50ms, jitter 10ms, loss 200 basis points와 seed `20260825`를 사용했다. delayed queue는 peer당 256개다.

runner는 경기마다 새 client와 bot process를 만들고 lobby와 game server는 유지한다. 각 child 디렉터리에 client 행, tick, replication, command, working set과 log를 남기고 parent summary는 원본에서 P95와 KiB/s를 다시 계산한다.

전체 review에서는 queue 수명 문제 두 건을 추가로 재현했다. move assignment가 이전 timer를 남긴 문제와 delivery callback 안에서 `Stop()`할 때 삭제된 peer를 다시 읽던 access violation을 각각 focused test와 별도 fix commit으로 남겼다. runner가 result metadata를 summary에서 빠뜨린 문제도 다음 실행부터 누락을 거부하도록 고쳤다.

## 결과

네 mode는 모두 Match 1, winner 4, tick 17430, reason 1로 끝났다.

| mode | payload bytes | server UDP bytes | fragment | tick P95 ms | encode P95 ms | 평균 KiB/s | recipient P95 KiB/s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| full-state | 910473696 | 968476695 | 836640 | 1.9100 | 0.0294 | 66.216564 | 66.217100 |
| interest-full | 365383696 | 405445812 | 409446 | 2.1375 | 0.0635 | 26.790149 | 27.401476 |
| interest-quantized | 216819982 | 248485500 | 209527 | 1.9740 | 0.0673 | 15.798970 | 16.145715 |
| interest-delta | 50088338 | 81738218 | 209173 | 1.8002 | 0.0672 | 4.123043 | 4.246719 |

interest-delta는 full-state보다 payload 94.499%, server UDP bytes 91.560%, 평균 수신량 93.773%를 줄였다. full-state는 64KiB/s 목표를 넘었고 나머지 mode는 통과했다. encode P95는 0.0294ms에서 0.0672ms로 늘었다. traffic 감소만 성공으로 적고 CPU 비용도 같이 남긴 이유다.

production 세 경기는 seed별 winner 4, 1, 2로 끝났고 72개 client 행의 protocol error와 queue overflow는 0이었다. 평균 수신량은 4.148225KiB/s, server tick P95는 1.7595ms였다.

impairment 한 경기에서는 datagram 12579개가 drop되고 keyframe 요청 3875회가 발생했다. discarded snapshot은 1개였고 protocol error와 queue overflow 없이 winner 4, tick 17430 결과를 유지했다.

Windows soak는 네 경기, 2324.07초를 실행했다. drop 50488개, keyframe 요청 15690회가 있었고 96개 client 행에서 protocol error와 queue overflow는 0이었다. game server working set 232개 표본은 마지막 15분 동안 모두 증가하지 않았다.

Ubuntu 24.04에서는 GCC `-Werror` build 뒤 ASan과 UBSan 24인 headless 경기를 1800초 동안 1817회 정상 종료했다. build와 runtime negative marker는 0이었다.

원본은 [24인 replication 비교](../benchmarks/network-load/20260826-01ae1278-COMPARISON.md)와 각 run 디렉터리에 있다.

## 남은 한계

자동 graphics 부하는 WARP였다. RTX 3050 Ti hardware에서 같은 24인 network 수치를 측정하지 않았다.

application shaper는 kernel queue, NAT, route change를 재현하지 않는다. 실제 외부 network 검증은 11주차 배포 뒤 별도로 한다.

Windows 성능 수치는 `01ae1278`, Linux sanitizer 수치는 경고 수정 뒤 `442f115`를 기준으로 한다. 차이는 이식성 초기화와 정수 변환이며 gameplay 변경은 없지만 같은 SHA 실행은 아니다.

full-state는 목표를 넘었다. 최종 delta mode가 목표를 통과했다는 이유로 기준선 실패를 지우지 않는다.

현재 summary schema의 기존 26일 run은 winner와 tick을 `results`에 넣지 않았다. 비교 문서는 client 원본 로그에서 결과를 다시 읽었다. review fix 뒤 새 run은 result field 누락을 거부한다.

hosted GitHub Actions는 이 branch를 아직 실행하지 않았다. local Windows와 Docker Linux 결과를 hosted CI 통과로 표현하지 않는다.
