# 오프라인 경기 Release 측정

## 실행 조건

- run ID: `20260824-021115-11abbe54-seed20260823`
- commit: `11abbe5456c5f4984b5612bd1d4eb22819158502`
- branch: `feat/offline-match-loop`
- seed: `20260823`
- Windows 11 Home 10.0.26200
- AMD Ryzen 7 6800HS Creator Edition, 8 core, 16 logical processor
- MSVC `194435228`, Release
- 참가자 24명, 중립 AI 100마리
- 30Hz fixed tick

```powershell
./scripts/run_offline_match_benchmark.ps1
```

runner는 깨끗한 commit과 HEAD를 build 전후에 확인했다. 같은 seed 경기를 두 번 먼저 실행해 summary를 비교하고, 세 번째 경기에서 tick 시간을 기록했다. 세 결과가 다르면 output 디렉터리를 만들지 않는다. 이번 repeat mismatch는 0건이고 validation은 `passed`다.

## 경기 결과

| 항목 | 결과 |
| --- | ---: |
| 종료 tick | 16,147 |
| simulation 시간 | 538.233초 |
| winner | ActorId 2 |
| 종료 이유 | LastSurvivor |
| 생존 참가자 | 1명 |
| event checksum | `17222440337191440965` |

8분은 tick 14,400, 10분은 tick 18,000이다. 이번 경기는 약 8분 58초에 끝나 잠긴 범위 안에 들어왔다.

## tick 시간

| 지표 | 시간 |
| --- | ---: |
| P50 | 0.0439ms |
| P95 | 0.2219ms |
| 최대 | 1.3238ms |

P95는 30Hz tick 예산 33.3ms보다 짧았다. 이 시간은 `OfflineMatch::Step()` 호출만 잰 값이다. 내부 경쟁 bot 23개와 중립 AI 100개의 판단, 이동, 파밍, 전투, zone과 결과 처리는 포함한다. 외부 actor 0의 `DecideContender`, snapshot 복사, event drain, CSV와 JSON 쓰기는 측정 구간 밖이다. 따라서 이 수치를 전체 process frame 시간이나 네트워크 서버 비용으로 표현하지 않는다.

## 원본 파일

- [ticks.csv](ticks.csv): tick 1부터 16,147까지 elapsed time, 생존 참가자와 중립 AI 수, event 수
- [result.json](result.json): winner, 종료 tick, checksum, repeat mismatch와 P50, P95, max
- [environment.json](environment.json): Git, MSVC Release, OS, CPU와 runner validation

SHA-256은 다음과 같다.

| 파일 | SHA-256 |
| --- | --- |
| `ticks.csv` | `1575c35b1840b367e25e7d571a64e5d915303652d0ea9a4be1eb56aa1bfb2779` |
| `result.json` | `cf5d1e9ed8683c4f028733c93692b6b7922c0a8fa3815f7445e3a5f4557705ab` |
| `environment.json` | `f5987fce321f35fcf9d0c64661e77097f5961bd00ce5d72ec0c7258a763dac5b` |

기존 실행을 덮어쓰지 않는다. 다른 commit이나 seed의 측정은 새 run ID로 추가한다.
