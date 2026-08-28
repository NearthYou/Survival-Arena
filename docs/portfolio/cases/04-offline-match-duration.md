# 체력 수치 대신 경기 수렴 곡선을 바꾼 오프라인 경기

## 상황

이동과 AI 명령은 있었지만 체력, 무기, zone과 승자 판정은 없었다. 24명과 중립 AI 100마리를 30Hz 권위 tick에서 진행해 seed `20260823`의 종료를 tick 14,400에서 18,000 사이로 재현하는 것이 기준이었다.

## 재현

첫 64×64 경기는 tick 91, 약 3초에 끝났다. 중립 AI를 뺀 진단도 tick 1,207, 약 40초였다. arena, spawn, zone을 4배와 5배 키운 실행은 tick 10,561과 10,560이었다. 이 순서는 [개발 기록](../../devlog/2026-08-24-offline-match-loop.md)에 있다.

## 관찰

작은 공간에서 bot이 가까운 적을 추적해 교전이 빨리 수렴했다. 균일 확대도 map과 zone의 상대 비율이 같아 같은 phase에서 밀집했다. 문제는 체력보다 encounter density와 zone 수렴 시점이었다.

## 가설과 비교한 대안

피해를 낮추거나 체력을 높이면 고정한 무기 규칙이 달라진다. 생존자가 나온 뒤 결과만 지연하는 방법도 제외했다. 256×256 arena에서 zone 곡선만 늦추는 대신 canonical workload에 맞춘 조정이라 다른 맵에 일반화할 수 없는 비용을 남겼다.

## 선택

이동 속도, 체력, 피해와 cooldown은 유지했다. zone을 8분까지 128, 112, 96, 80, 64로 줄이고 sudden death에서 0으로 수렴시켰다. 판정은 `OfflineMatch::Step()`에 두고 [오프라인 경기 ADR](../../adr/0005-authoritative-offline-match.md)에 고정했다.

## 구현

한 tick은 마지막 command, bot 판단, 이동, pickup, combat batch, zone 피해, 승자 판정 순이다. 공격은 pre-damage 상태에서 검증해 target별로 적용했다. 전원이 쓰러지면 직전 체력, 처치 수, ActorId 순으로 한 명을 남겼다. 수동 fixture는 내부 bot을 껐다.

## 검증

[채택 원본](../../benchmarks/offline-match/20260824-023134-1ede6a23-seed20260823/RESULT.md)은 종료 tick `16,147`, simulation `538.233초`, repeat mismatch `0`, tick P95 `0.2292ms`를 기록했다. winner와 checksum도 같은 seed 두 실행에서 같아 약 8분 58초로 잠긴 범위 안에 들었다.

## 남은 한계

tick 시간은 `Step()`만 측정해 외부 actor 판단, snapshot, event drain과 파일 쓰기를 제외했다. RTX visible play와 network 비용도 없어 전체 game frame이나 서버 시간으로 표현하지 않는다.
