# 공간 탐색과 AI Release 측정

## 실행 조건

- run ID: `20260823-182453-5d318dea-seed20260823`
- commit: `5d318deab1b23e050d846b5321def30380d65adb`
- branch: `feat/spatial-navigation-ai`
- seed: `20260823`
- Windows 11 Home 10.0.26200
- AMD Ryzen 7 6800HS Creator Edition, 8 core, 16 logical processor
- MSVC `194435228`, Release
- case마다 준비 1회, 측정 5회, 중앙값 사용

```powershell
./scripts/run_simulation_benchmark.ps1
```

## 동등성과 원본 검증

| 비교 | checksum | mismatch |
| --- | --- | ---: |
| Nav linear와 grid | `8995d4e2c5340501` | 0 |
| Spatial AABB linear와 quadtree | `aa659a673b1def5c` | 0 |
| Spatial picking linear와 quadtree | `2a4d6959cbae9df9` | 0 |
| FSM과 behavior tree | `d82152856faf9747` | 0 |

전체 result checksum은 `8e4984d98be95814`다. runner는 JSON의 case별 5개 sample, CSV 40개 행, 연속된 sample index, CPU와 compiler 필드를 확인했고 validation은 `passed`다.

## 측정 결과

| case | 중앙값 | 순회 후보, bounds 또는 평가 수 |
| --- | ---: | ---: |
| Nav linear | 7308.5022ms | 819,200,000 |
| Nav grid | 36.4567ms | 2,499,032 |
| Spatial linear AABB | 127.7082ms | 22,480,000 |
| Loose quadtree AABB | 89.8673ms | 13,323,264 |
| Spatial linear picking | 237.3660ms | 22,480,000 |
| Loose quadtree picking | 132.2706ms | 13,138,396 |
| FSM | 2.8142ms | 100,000 |
| Behavior tree | 6.6890ms | 100,000 |

Nav grid 시간은 99.501174%, 순회 후보 수는 99.694942% 줄었다. loose quadtree 시간은 AABB query 29.630752%, picking 44.275676% 줄었다. behavior tree 시간은 FSM보다 137.687442% 늘었다.

## 원본 파일

- [result.json](result.json): workload, 각 case의 5개 sample, 중앙값, 검사량, checksum
- [samples.csv](samples.csv): 8개 case의 40개 원시 시간 sample
- [environment.json](environment.json): Git, 빌드, OS, CPU, runner validation

이 문서는 원본을 읽기 위한 요약이다. 재측정 결과로 기존 JSON이나 CSV를 덮어쓰지 않는다.
