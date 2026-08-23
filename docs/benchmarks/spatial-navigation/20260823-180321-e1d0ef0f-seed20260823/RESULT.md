# 공간 탐색과 AI Release 측정

## 실행 조건

- run ID: `20260823-180321-e1d0ef0f-seed20260823`
- commit: `e1d0ef0fe99b4b0ff36ff8e85fd8da25ab9d25d9`
- branch: `feat/spatial-navigation-ai`
- seed: `20260823`
- Windows 11 Home 10.0.26200
- AMD Ryzen 7 6800HS Creator Edition, 8 core, 16 logical processor
- MSVC `194435228`, Release
- case마다 준비 1회, 측정 5회, 중앙값 사용

```powershell
./scripts/run_simulation_benchmark.ps1
```

## 동등성

| 비교 | checksum | mismatch |
| --- | --- | ---: |
| Nav linear와 grid | `8995d4e2c5340501` | 0 |
| Spatial AABB linear와 quadtree | `aa659a673b1def5c` | 0 |
| Spatial picking linear와 quadtree | `2a4d6959cbae9df9` | 0 |
| FSM과 behavior tree | `d82152856faf9747` | 0 |

전체 result checksum은 `8cf777ffee07e269`이고 runner validation은 `passed`다.

## 측정 결과

| case | 중앙값 | 후보, bounds 또는 평가 수 |
| --- | ---: | ---: |
| Nav linear | 6531.8967ms | 819,200,000 |
| Nav grid | 31.7562ms | 118,612 |
| Spatial linear AABB | 105.3028ms | 22,480,000 |
| Loose quadtree AABB | 66.7800ms | 13,323,264 |
| Spatial linear picking | 239.1861ms | 22,480,000 |
| Loose quadtree picking | 117.9218ms | 13,138,396 |
| FSM | 3.0411ms | 100,000 |
| Behavior tree | 6.1247ms | 100,000 |

Nav grid 시간은 99.513829%, 후보 수는 99.985521% 줄었다. loose quadtree 시간은 AABB query 36.582883%, picking 50.698724% 줄었다. behavior tree 시간은 FSM보다 101.397521% 늘었다.

## 원본 파일

- [result.json](result.json): workload, 각 case의 5개 sample, 중앙값, 검사량, checksum
- [samples.csv](samples.csv): 8개 case의 40개 원시 시간 sample
- [environment.json](environment.json): Git, 빌드, OS, CPU, runner validation

이 문서는 원본을 읽기 위한 요약이다. 재측정 결과로 기존 JSON이나 CSV를 덮어쓰지 않는다.
