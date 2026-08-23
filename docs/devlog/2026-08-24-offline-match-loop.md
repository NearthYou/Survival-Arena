# 3초짜리 생존 경기를 체력 조정 없이 8분 58초로 바꿨다

## 상황

6주차가 끝났을 때 캐릭터는 NavMesh 위를 움직였고 근접형과 원거리형 AI는 command를 골랐다. 하지만 체력도 무기도 없었다. 누가 죽었는지, zone 밖에서 얼마나 버티는지, 마지막 한 명이 누구인지 판단하는 주체도 없었다.

7주차 목표는 기능 목록을 따로 만드는 것이 아니라 네트워크 없이 한 판을 끝내는 것이었다. 사용자 actor 1명, 경쟁 bot 23명, 중립 AI 100마리, 무기 3종과 loot 60개를 같은 30Hz simulation에 넣었다. canonical seed는 `20260823`, 종료 범위는 tick 14,400에서 18,000으로 잡았다.

## 첫 경기

처음 arena는 64×64였다. 참가자 24명을 반경 20에서 26 사이에 두고 중립 AI 100마리를 같은 평면에 뿌렸다. 경쟁 bot은 zone, MedKit, 무기, 공격, 추적 순으로 판단했다. 중립 AI는 6주차 behavior tree를 그대로 사용했다.

첫 자동 경기는 tick 91에 끝났다. simulation 시간으로 약 3초였다. 첫 사망은 tick 19에 나왔고 참가자 23명과 중립 AI 1마리가 combat으로 죽었다. 피해 event를 source별로 나누니 참가자 발 30건, 중립 AI 발 120건이었다. zone 사망은 0명이었다.

중립 AI만 문제인지 확인하려고 100마리를 모두 뺀 진단 실행도 했다. 참가자끼리만 싸운 경기는 tick 1,207, 약 40초에 끝났다. 64×64 안에서 모든 bot이 가까운 적을 향해 움직이는 규칙과 8분 목표가 맞지 않았다.

## 첫 대안이 실패했다

무기 피해를 낮추거나 체력을 크게 올리면 시간을 늘릴 수 있다. 하지만 Blade 24, Rifle 12, ArcPulse 18이라는 규칙을 먼저 고정했고, 피해를 10배 낮춰 맞춘 8분은 전투 감각을 설명하기 어렵다. 마지막 한 명이 일찍 나왔는데 결과만 8분까지 미루는 방법도 제외했다.

arena와 spawn, zone을 모두 4배로 키웠다. 초기 교전은 줄었지만 경기는 tick 10,561, 약 5분 52초에 끝났다. 5배 확대도 tick 10,560이었다. uniform scale은 map과 zone의 상대 비율을 유지하므로 같은 phase에서 다시 밀집했다.

문제는 전체 크기가 아니라 수렴 곡선이었다. 256×256 arena와 참가자 반경 80에서 104는 유지했다. zone은 128, 112, 96, 80, 64로 8분까지 넓게 남기고 sudden death 2분 동안 0으로 줄였다. 시간, 이동 속도, perception, 체력, 피해, cooldown은 바꾸지 않았다.

새 곡선은 tick 16,147에 끝났다. 약 8분 58초였고 winner는 ActorId 2였다. 같은 seed 두 실행의 summary와 event checksum이 같았다.

## 판정 순서를 한 곳에 모았다

`OfflineMatch`는 NavMesh 사본, actor, loot, command, event와 결과를 소유한다. public header는 pimpl로 내부 container를 숨겼다. 임시 NavMesh로 match를 만든 뒤에도 agent가 움직이는 테스트를 추가했다. NavMesh를 pimpl 안에서 먼저 복사하고 vector reserve 뒤 NavAgent를 만들었다.

같은 tick의 공격은 바로 체력에서 빼지 않았다. 모든 intent를 기존 alive 상태에서 검증하고 target별 기여 피해를 모았다. 그 다음 체력을 한 번만 줄였다. 입력 배열을 정방향과 역방향으로 넣어도 같은 damage와 death record가 나오는지 확인했다.

마지막 두 참가자가 서로를 죽이는 테스트에서 설계 빈칸이 드러났다. combat batch 뒤 alive contender가 0명이었다. zone wipe에만 있던 순위 규칙을 combat wipe에도 적용했다. 피해 직전 체력, 기존 처치 수, ActorId 순으로 한 명을 체력 1에 남기고 그 사망에 붙은 상대 처치만 취소했다.

tick 14,400에서 phase가 `SuddenDeath`로 바뀐 뒤 다음 `Step()`이 거부되는 오류도 있었다. public guard가 `Running`만 허용하고 있었다. `Running`과 `SuddenDeath`를 active phase로 묶은 뒤 tick 18,000 timeout 테스트를 통과시켰다.

## 수동 fixture와 내부 bot을 분리했다

경쟁 bot을 match 안에 연결하자 앞서 만든 command와 combat 테스트 7개가 한꺼번에 깨졌다. 테스트가 actor 1에 목적지를 넣어도 내부 bot이 같은 tick 뒤쪽에서 다른 command로 덮어썼다.

기존 assertion을 느슨하게 만들지 않았다. `enableInternalBots` 기본값은 true로 두고, 수동 command와 combat fixture에서만 false로 설정했다. canonical match와 실제 demo는 기본값을 사용한다. ID 0은 내부 bot 대상에서 항상 제외하고 auto mode가 공개 `DecideContender` 결과를 `Submit()`으로 넣는다.

arena를 256×256으로 바꾼 뒤 `{100, 100}`이 더 이상 off-mesh가 아니게 된 테스트도 있었다. server validation이 고장난 것이 아니라 fixture가 낡은 것이어서 `{200, 200}`으로 경계 밖 좌표를 다시 고정했다.

## DX11 연결

renderer는 `MatchSnapshot`을 include하지 않는다. app이 참가자 24개와 AI 100개를 `SceneCharacterState`로 바꾼다. 위치와 active flag만 renderer에 넘기고, 죽은 slot은 shadow와 G-Buffer loop 시작점에서 건너뛴다. setter를 호출하지 않으면 기존 stress scene 1,124개 object 통계가 그대로 유지된다.

`dxa_offline_match_demo` visible mode는 실제 시간 accumulator로 30Hz tick을 진행하고 한 frame에서 최대 5 tick만 따라잡는다. 지면 우클릭은 6주차 screen ray와 NavMesh 검증을 재사용한다. auto mode는 시작과 결과 frame만 렌더하고 중간 16,147 tick은 렌더하지 않는다.

WARP 실행은 다음 결과를 냈다.

```text
offline match complete: tick=16147, seconds=538.233, winner=2, reason=last_survivor, checksum=17222440337191440965
```

최종 frame에 non-clear pixel이 있었고 DX11 debug error는 0건이었다.

## Release 측정

공식 원본은 깨끗한 commit `11abbe5456c5f4984b5612bd1d4eb22819158502`에서 만들었다.

```powershell
./scripts/run_offline_match_benchmark.ps1
```

runner는 같은 seed 두 경기의 summary를 먼저 비교하고 세 번째 경기에서 tick 시간을 쟀다. `ticks.csv` 행 수는 종료 tick과 같은 16,147개다. repeat mismatch는 0건이고 validation은 `passed`다.

| 지표 | 결과 |
| --- | ---: |
| 종료 tick | 16,147 |
| simulation 시간 | 538.233초 |
| winner | 2 |
| tick P50 | 0.0439ms |
| tick P95 | 0.2219ms |
| tick max | 1.3238ms |

tick 시간은 `OfflineMatch::Step()`만 포함한다. 내부 bot, 이동, pickup, combat, zone과 결과 판정은 포함하지만 외부 actor 0 판단, snapshot 복사, event drain과 파일 쓰기는 제외한다. 전체 game frame이나 이후 네트워크 server 비용으로 바꿔 말하지 않는다.

원본은 [20260824-021115 실행](../benchmarks/offline-match/20260824-021115-11abbe54-seed20260823/RESULT.md)에 있다.

## 검증

benchmark 코드 commit 전 Windows Debug 전체 232개 테스트와 MSVC Release 빌드를 통과했다. OfflineMatchDemo WARP는 약 16초에 한 경기를 진행하고 시작 및 결과 frame을 검증했다. benchmark option과 runner guard는 dirty tree, moved HEAD, 기존 output, 잘못된 SHA, repeat mismatch, winner와 checksum 누락, 종료 tick 범위, CSV 행 수와 NaN 시간을 거부했다.

로컬 `g++`는 없었다. Docker CLI는 있었지만 daemon이 실행 중이 아니어서 Linux build를 통과했다고 쓰지 않는다. PR의 Ubuntu CI를 Linux 검증 문턱으로 남긴다.

## 남은 한계

맵은 높이와 장애물이 없는 256×256 평면이다. actor body blocking과 projectile flight가 없고 Rifle은 hitscan, ArcPulse는 즉시 범위 피해다.

경쟁 bot은 기억이 없는 우선순위 함수다. 중립 AI behavior tree도 현재 target을 매 decision마다 다시 고른다. aggro history, 위협도, squad 행동은 없다.

600초 timeout은 순위로 한 명을 남기는 개발용 종료 보장이다. 실제 무승부나 reconnect를 표현하지 못한다.

WARP 자동 실행은 결과 frame을 검증했지만 실제 RTX 3050 Ti에서 visible mode를 플레이하며 카메라와 256×256 공간의 가독성을 확인하는 절차는 아직 남아 있다. networking, lobby, room은 8주차 범위다.
