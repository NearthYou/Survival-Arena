# 로비 ticket에서 DX11 경기 결과까지 연결했다

## 증상

8주차의 start 성공은 실제 game server 시작을 뜻하지 않았다. lobby는 고정된 endpoint를 ticket과 함께 보냈고, client와 bot은 ticket을 받은 시점에 종료했다. 같은 protocol로 방을 시작해도 30Hz simulation, UDP input, snapshot과 disconnect 결과까지 이어지는 process가 없었다.

렌더러와 network를 바로 묶는 것도 피해야 했다. `EngineApp`가 game packet이나 `OfflineMatch`를 알게 되면 Windows renderer와 Linux server의 경계가 흐려진다. 반대로 network thread가 renderer 장면을 직접 바꾸면 snapshot callback과 render frame이 같은 state를 동시에 수정하게 된다.

## 재현

첫 수직 테스트는 lobby와 worker, WARP client와 play bot을 ephemeral port로 띄우고 sudden death 30 tick, hard timeout 60 tick을 주입했다. production 기본값을 CLI로 줄이지 않고 test fixture 생성자에서만 설정했다.

처음에는 `ValidateMatchConfig`가 14,400과 18,000 tick만 허용해 fixture가 경기 연결 전에 중단됐다. 30Hz와 6 tick bot 판단 주기는 그대로 고정하되, 양수 sudden death가 hard timeout보다 앞서는 구성을 허용하도록 validation 경계를 나눴다. production executable의 기본값은 바꾸지 않았다.

계획한 600 WARP frame을 vsync 상태로 돌린 첫 실행은 23,408ms가 걸려 15초 watchdog을 넘었다. 이 실행은 network 결과를 기다리기 전에 timeout으로 실패했다. CI용 수직 테스트의 목적은 장시간 렌더 성능 측정이 아니라 실제 render loop가 같은 경기 결과를 소비하는지 확인하는 것이므로 frame 상한을 300으로 줄였다. watchdog 15초와 vsync는 유지했다.

최종 자동 재현 명령은 다음과 같다.

```powershell
./scripts/build.ps1
./scripts/test.ps1
./scripts/build.ps1 -Preset windows-msvc-release
./out/build/windows-msvc-vs-debug/tests/Debug/dxa_tests.exe `
  --gtest_filter=GameServerIntegration.*:GameSession.*:NetworkVerticalSlice.* `
  --gtest_repeat=3 `
  --gtest_break_on_failure
```

## 가설

방 시작과 game server 준비 사이에 commit 지점이 없어서 고정 endpoint가 필요했다. worker가 roster와 ticket을 실제로 받아들인 뒤에만 client에게 ticket을 넘기면 준비되지 않은 endpoint로 참가자를 보내지 않을 수 있다고 봤다.

입력과 snapshot의 지연 요구도 달랐다. 인증, 예약과 결과는 순서와 전달 보장이 필요하지만 최신 이동 상태는 오래된 packet 재전송보다 새 packet이 더 중요하다. 그래서 control과 결과는 TCP, 지속 input과 snapshot은 UDP에 두는 것이 맞다고 판단했다.

renderer 결합 문제는 engine이 network 객체를 소유해서가 아니라 필요한 장면 입력의 경계가 없어서 생겼다. engine에는 `IRuntimeSceneController`만 보이고, app adapter가 snapshot과 prediction 결과를 `RuntimeSceneFrame`으로 바꾸면 platform 경계를 유지할 수 있다고 봤다.

## 비교한 대안

game traffic 전체를 lobby TCP에 넣는 방법은 구현 파일이 줄어든다. 그러나 snapshot이 lobby request와 같은 queue를 사용하고 worker process 분리의 의미가 작아진다.

UDP input을 click event로 한 번만 보내는 방법은 packet 수가 적다. 한 packet이 유실되면 server가 목적지를 영원히 알 수 없으므로 현재 destination을 매 tick sequence와 함께 보내는 상태형 input을 선택했다.

remote actor를 최신 속도로 계속 외삽하면 packet 공백에서도 움직인다. 장애물, 사망과 방향 전환 뒤 틀린 위치를 그리는 비용이 더 커서 3 tick 늦은 보간과 마지막 위치 hold를 선택했다.

network callback이 renderer vector를 직접 고치는 방법도 제외했다. snapshot은 network thread의 64개 bounded queue에 넣고 render thread의 30Hz fixed update에서만 꺼내 장면으로 변환했다.

## 구현

lobby에 `WorkerRegistry`와 worker control TCP server를 추가했다. worker는 capacity 1로 등록하고, lobby는 room을 Starting으로 바꾼 뒤 reservation을 보낸다. worker ready가 오면 InMatch와 ticket 전달을 확정한다. timeout, reject와 disconnect에서는 임시 ticket을 revoke하고 Waiting으로 되돌린다. cancel ACK 전에는 같은 worker를 다시 배정하지 않는다.

game server는 reservation마다 participant를 ActorId 0부터 순서대로 배치하고 `OfflineMatch`를 30Hz로 실행한다. timer deadline은 실제 callback 종료 시각이 아니라 이전 목표 시각에서 계산한다. 늦었을 때 한 callback의 catch-up은 최대 5 tick으로 제한했다. 짝수 tick마다 15Hz full-state snapshot을 만들고 1,200바이트 UDP datagram으로 나눈다.

client는 TCP ticket 인증 뒤 발급받은 UDP token으로 endpoint를 bind한다. server가 보낸 map ID와 NavMesh CRC가 local canonical arena와 다르면 UDP bind 전에 중단한다. snapshot은 길이와 CRC가 맞는 완성본만 적용하고, 더 새로운 snapshot이 오면 불완전한 이전 조립을 버린다.

local actor는 server ACK 위치에서 아직 확인되지 않은 input을 다시 적용한다. 다른 actor와 중립 AI는 3 tick 늦은 시점을 보간하고 새 표본이 없으면 마지막 위치를 유지한다. DX11 client는 우클릭 ray와 y=0 평면의 교점을 destination으로 보내며 server가 같은 값을 NavMesh에서 다시 검사한다.

최종 수직 테스트는 production `EngineApp`, `NetworkClientController`, `BotCoordinator`, lobby와 game server를 그대로 사용한다. deterministic secret source는 test에서만 주입해 ticket과 UDP token의 정확한 16바이트 표현이 stdout과 stderr에 없는지 검사한다.

전체 diff 검토에서는 네 가지 상태 오류를 실제 테스트로 재현했다. 동기화 전 결과와 다른 MatchId 결과 수락, local actor 사망을 장면에 반영하지 않는 문제, 완료 직전 queue에 있던 snapshot이 Finished 상태를 되돌리는 문제, 다른 MatchId snapshot fragment 수락을 각각 별도 회귀 테스트와 수정 commit으로 남겼다.

## 결과

리뷰 수정 뒤 Windows Debug CTest 488개가 통과했고 MSVC Release build도 완료됐다. server integration, `GameSession`, WARP 수직 경로 27개는 세 번 연속 통과했다. 마지막 반복의 공개 출력은 다음과 같다.

```text
room=1
match=1
host_snapshots=29
bot_snapshots=29
finished_tick=60
elapsed_ms=3011
secret_leak_count=0
```

이 3,011ms는 중립 AI와 loot를 끄고 60 tick에 끝나도록 주입한 test fixture 결과다. production 경기 시간이나 server 성능 수치가 아니다.

수동 검증은 네 process를 loopback에서 실행했다. hardware client와 play bot은 RoomId 1, MatchId 1을 받았고 둘 다 snapshot 2개 이상을 확인했다. 이후 bot process를 의도적으로 종료하자 client가 `winner=0`, `tick=2789` 결과를 받았다. client는 exit code 0으로 끝났다. bot은 연결 종료 처리를 확인하려고 강제 종료했으므로 정상 exit code는 측정하지 않았다.

화면에서는 우클릭 뒤 local character 이동, remote actor 위치 변화와 축소 zone 표현을 확인했다. 별도 Debug hardware 실행은 `NVIDIA GeForce RTX 3050 Ti Laptop GPU`, 120 frame, DX11 debug layer error 0, exit code 0이었다.

첫 PR CI의 Ubuntu build는 두 이벤트에서 같은 GCC 오류로 중단됐다. `GameSession` 생성자 초기화 목록이 멤버 선언 순서와 달라 `-Wreorder`가 발생했다. 선언 순서에 맞춘 `a695d0a` 뒤에는 build가 더 진행됐고, 축약 aggregate 세 곳에서 빈 `optional`을 생략한 `-Wmissing-field-initializers`가 드러났다. `WorkerRecord`, `PlayerSession`, `ParticipantSlot`의 의도된 빈 상태를 명시한 `7d6ee11`로 수정했다. 두 오류 모두 CI의 warnings-as-errors가 경고를 build 실패로 바꾼 경우였다.

Ubuntu 24.04와 고정 vcpkg baseline을 사용한 CI 동형 Docker 검증에서 146/146 build target과 Linux CTest 435/435가 통과했다. 코드 변경 head `7d6ee11`에서는 push와 PR 이벤트의 Ubuntu job이 각각 5분 18초와 5분 4초, Windows job이 각각 22분 17초와 23분 55초에 통과했다. Windows job에는 전체 CTest와 WARP, shader와 asset 등록 검사가 포함된다.

## 남은 한계

9주차 수동 경기는 두 participant만 사용했다. 실제 DX11 client 1개와 play bot 23개의 세 경기 연속 완주, 평균 수신량과 server tick 비용은 아직 검증하지 않았다.

snapshot은 full state라 actor와 loot가 늘면 fragment와 대역폭이 그대로 증가한다. 관심 영역 grid, 위치 양자화, 변경 bitset, 100ms RTT, 2퍼센트 손실과 10ms jitter는 10주차에서 같은 baseline과 비교한다.

game TCP와 UDP는 평문이고 reconnect와 UDP endpoint rebind가 없다. 기본 loopback 밖에서 실행하지 않는다. worker process가 죽으면 진행 중인 match를 복구하지 못한다.

수동 장면에서는 고정 camera가 local character나 zone geometry와 가까워질 때 화면 일부가 가려졌다. network 상태와 result는 확인할 수 있었지만 camera collision이나 zoom 보정은 이번 변경에 포함하지 않았다.

Windows와 Ubuntu CI는 최종 head에서 모두 통과했다. 이 결과는 9주차 기능과 현재 dependency baseline의 build 재현 근거이며, 10주차 24인 부하와 network impairment 성능을 대신하지 않는다.
